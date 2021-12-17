#pragma once

#include <string>
#include <vector>
#include <map>
#include <queue>
#include <optional>

namespace luacpp {

struct assist_function {
    assist_function(size_t args_count) {
        if (args_count > 16) {
            vararg = true;
            return;
        }

        arg_names.reserve(args_count);
        for (char v = 'a'; v < 'a' + char(args_count); ++v)
            arg_names.emplace_back(1, v);
    }

    template <typename... Ts>
    assist_function(Ts&&... iarg_names): arg_names{std::forward<Ts>(iarg_names)...} {}

    std::vector<std::string> arg_names;
    bool vararg = false;
};

enum class assist_type {
    field,
    function,
    member_function
};

struct assist_some {
    void push_function(std::string name, std::vector<std::string> args, std::optional<std::string> metatable_result) {
        auto& func =
            children.insert_or_assign(name + " " + std::to_string(args.size()), assist_some{}).first->second;
        func.type             = assist_type::function;
        func.name             = std::move(name);
        func.args             = std::move(args);
        func.metatable_result = std::move(metatable_result);
    }

    assist_some* push_field(std::string name, bool metatable) {
        auto& field     = children.emplace(name, assist_some{}).first->second;
        field.type      = assist_type::field;
        field.name      = std::move(name);
        if (metatable)
            field.metatable = true;
        return &field;
    }

    void push_member_function(std::string                class_name,
                              std::string                name,
                              std::vector<std::string>   args,
                              std::optional<std::string> metatable_result,
                              bool                       captured_self) {
        auto& func =
            children.insert_or_assign(name + " " + std::to_string(args.size()), assist_some{}).first->second;
        func.type             = assist_type::member_function;
        func.name             = std::move(name);
        func.args             = std::move(args);
        func.class_name       = std::move(class_name);
        func.metatable_result = std::move(metatable_result);
        func.captured_self    = captured_self;
    }

    static void put_indent(std::string& out, size_t count) {
        if (count)
            out.resize(out.size() + count, ' ');
    }

    void traverse(std::string& out, size_t indent = 0) const {
        if (name.empty()) {
            for (auto& [_, child] : children)
                if (child.type == assist_type::field)
                    child.traverse(out, indent);
            for (auto& [_, child] : children)
                if (child.type != assist_type::field)
                    child.traverse(out, indent);
            return;
        }

        put_indent(out, indent);
        switch (type) {
        case assist_type::field:
            out += name;
            out += " = {";
            if (!children.empty()) {
                out += '\n';
                for (auto& [_, child] : children)
                    if (child.type == assist_type::field)
                        child.traverse(out, indent + 4);
                for (auto& [_, child] : children)
                    if (child.type != assist_type::field)
                        child.traverse(out, indent + 4);
            }
            if (metatable) {
                out += '\n';
                put_indent(out, indent + 4);
                out += "__index = ";
                out += name;
                out += '\n';
            }
            out += '}';
            break;
        case assist_type::function:
            out += name;
            out += " = function(";
            if (!args.empty())
                out += args.front();
            for (size_t i = 1; i < args.size(); ++i) {
                out += ", ";
                out += args[i];
            }
            out += ')';
            if (metatable_result) {
                out += '\n';
                put_indent(out, indent + 4);
                out += "local result = { __index = ";
                out += *metatable_result + " }\n";
                put_indent(out, indent + 4);
                out += "return setmetatable(result, ";
                out += *metatable_result;
                out += ")\n";
                put_indent(out, indent);
            }
            else
                out += ' ';
            out += "end";
            break;
        case assist_type::member_function:
            out += "function ";
            out += class_name;
            out += captured_self ? ':' : '.';
            out += name;
            out += '(';
            if (!args.empty())
                out += args.front();
            for (size_t i = 1; i < args.size(); ++i) {
                out += ", ";
                out += args[i];
            }
            out += ')';
            if (metatable_result) {
                out += '\n';
                put_indent(out, indent + 4);
                out += "local result = { __index = ";
                out += *metatable_result + " }\n";
                put_indent(out, indent + 4);
                out += "return setmetatable(result, ";
                out += *metatable_result;
                out += ")\n";
                put_indent(out, indent);
            }
            else
                out += ' ';
            out += "end";
        }
        if (indent > 0)
            out += ',';
        out += '\n';
    }

    std::string                               name;
    std::string                               class_name;
    assist_type                        type;
    std::map<std::string, assist_some> children;
    std::vector<std::string>                  args;
    bool                                      metatable     = false;
    bool                                      captured_self = true;
    std::optional<std::string>                metatable_result;
};

class assist_gen {
public:
    template <bool push_last = true>
    auto field(std::string_view name, bool metatable = false) {
        auto current = &root;
        auto founddot = name.find('.');
        while (founddot != std::string_view::npos) {
            current = current->push_field(std::string(name.substr(0, founddot)), false);
            name = name.substr(founddot + 1);
            founddot = name.find('.');
        }
        if constexpr (push_last)
            current->push_field(std::string(name), metatable);
        else
            return std::tuple{current, name};
    }

    void
    function(std::string_view name, size_t args_count, std::optional<std::string> metatable_result = {}) {
        auto [obj, last_name] = field<false>(name);
        obj->push_function(std::string(last_name), generate_argnames(args_count), std::move(metatable_result));
    }

    void member_function(std::string_view           class_name,
                         std::string_view           name,
                         size_t                     args_count,
                         std::optional<std::string> metatable_result = {},
                         bool                       captured_self    = true) {
        root.push_member_function(std::string(class_name),
                                  std::string(name),
                                  generate_argnames(args_count),
                                  std::move(metatable_result),
                                  captured_self);
    }

    std::string generate() const {
        std::string result;
        root.traverse(result);
        return result;
    }

    std::vector<std::string> generate_argnames(size_t args_count) {
        if (args_count > ('z' - 'a') + 1)
            return {};

        std::vector<std::string> result;
        if (!current_argnames.empty()) {
            auto sz = std::min(args_count, current_argnames.front().size());
            for (size_t i = 0; i < sz; ++i)
                result.push_back(current_argnames.front()[i]);
            current_argnames.pop();
        }

        for (char argname = 'a'; argname < 'a' + char(args_count - result.size()); ++argname)
            result.emplace_back(1, argname);

        return result;
    }

    template <typename... Ts>
    void annotate_args(Ts&&... argument_names) {
        current_argnames.push(std::vector<std::string>{std::forward<Ts>(argument_names)...});
    }

private:
    assist_some root;
    std::queue<std::vector<std::string>> current_argnames;
};

} // namespace luacpp
