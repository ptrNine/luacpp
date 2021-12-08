#pragma once

#include <string>
#include <vector>
#include <map>

struct luacpp_assist_function {
    luacpp_assist_function(size_t args_count) {
        if (args_count > 16) {
            vararg = true;
            return;
        }

        arg_names.reserve(args_count);
        for (char v = 'a'; v < 'a' + char(args_count); ++v)
            arg_names.emplace_back(1, v);
    }

    template <typename... Ts>
    luacpp_assist_function(Ts&&... iarg_names): arg_names{std::forward<Ts>(iarg_names)...} {}

    std::vector<std::string> arg_names;
    bool vararg = false;
};

enum class luacpp_assist_type {
    field,
    function,
    member_function
};

struct luacpp_assist_some {
    void push_function(std::string name, std::vector<std::string> args, std::optional<std::string> metatable_result) {
        auto& func            = children.insert_or_assign(name, luacpp_assist_some{}).first->second;
        func.type             = luacpp_assist_type::function;
        func.name             = std::move(name);
        func.args             = std::move(args);
        func.metatable_result = std::move(metatable_result);
    }

    luacpp_assist_some* push_field(std::string name, bool metatable) {
        auto& field     = children.emplace(name, luacpp_assist_some{}).first->second;
        field.type      = luacpp_assist_type::field;
        field.name      = std::move(name);
        if (metatable)
            field.metatable = true;
        return &field;
    }

    void push_member_function(std::string                class_name,
                              std::string                name,
                              std::vector<std::string>   args,
                              std::optional<std::string> metatable_result) {
        auto& func            = children.insert_or_assign(name, luacpp_assist_some{}).first->second;
        func.type             = luacpp_assist_type::member_function;
        func.name             = std::move(name);
        func.args             = std::move(args);
        func.class_name       = std::move(class_name);
        func.metatable_result = std::move(metatable_result);
    }

    static void put_indent(std::string& out, size_t count) {
        if (count)
            out.resize(out.size() + count, ' ');
    }

    void traverse(std::string& out, size_t indent = 0) const {
        if (name.empty()) {
            for (auto& [_, child] : children)
                child.traverse(out, indent);
            return;
        }

        put_indent(out, indent);
        switch (type) {
        case luacpp_assist_type::field:
            out += name;
            out += " = {";
            if (!children.empty()) {
                out += '\n';
                for (auto& [_, child] : children) {
                    child.traverse(out, indent + 4);
                }
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
        case luacpp_assist_type::function:
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
                out += "result = {}\n";
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
        case luacpp_assist_type::member_function:
            out += "function ";
            out += class_name;
            out += ':';
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
                out += "result = {}\n";
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
    luacpp_assist_type                        type;
    std::map<std::string, luacpp_assist_some> children;
    std::vector<std::string>                  args;
    bool                                      metatable = false;
    std::optional<std::string>                metatable_result;
};

class luacpp_assist_gen {
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
    function(std::string_view name, std::vector<std::string> args, std::optional<std::string> metatable_result = {}) {
        auto [obj, last_name] = field<false>(name);
        obj->push_function(std::string(last_name), std::move(args), std::move(metatable_result));
    }

    void member_function(std::string_view           class_name,
                         std::string_view           name,
                         std::vector<std::string>   args,
                         std::optional<std::string> metatable_result = {}) {
        root.push_member_function(
            std::string(class_name), std::string(name), std::move(args), std::move(metatable_result));
    }

    std::string generate() {
        std::string result;
        root.traverse(result);
        return result;
    }

private:
    luacpp_assist_some root;
};
