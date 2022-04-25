#pragma once

#include <map>
#include <set>
#include <optional>
#include <queue>
#include <string>
#include <vector>
#include <memory>
#include <sstream>

#include "luacpp_details.hpp"

namespace luacpp
{

struct assist_value;
struct assist_table;
struct assist_function;
struct assist_overloaded_function;
struct assist_class_declaration;

class assist_visitor {
public:
    virtual ~assist_visitor() = default;
    virtual void visit(const assist_value&) = 0;
    virtual void visit(const assist_table&) = 0;
    virtual void visit(const assist_function&) = 0;
    virtual void visit(const assist_overloaded_function&) = 0;
    virtual void visit(const assist_class_declaration&) = 0;
};

struct assist_value_base {
    assist_value_base() = default;
    assist_value_base(std::string iname): name(std::move(iname)) {}
    std::string name;
    std::string comment;

    virtual ~assist_value_base()                      = default;
    virtual std::string info() const                  = 0;
    virtual void        accept(assist_visitor&) const = 0;

    virtual bool is_table() const {
        return false;
    }

    virtual bool is_member_function() const {
        return false;
    }
};

struct assist_value : public assist_value_base {
    assist_value(std::string type, std::string name, std::string value = {}):
        assist_value_base(std::move(name)), type(std::move(type)), value(std::move(value)) {}

    std::string info() const final {
        return name + ": " + type;
    }

    void accept(assist_visitor& visitor) const final {
        visitor.visit(*this);
    }

    std::string type;
    std::string value;
};

struct assist_table : public assist_value_base {
    assist_table() = default;
    assist_table(std::string name): assist_value_base(std::move(name)) {}

    std::string info() const override {
        return name + ": table";
    }

    bool is_table() const final {
        return true;
    }

    void accept(assist_visitor& visitor) const override {
        visitor.visit(*this);
    }

    std::map<std::string, std::unique_ptr<assist_value_base>> values;
};

struct assist_function : public assist_value_base {
    struct parameter_t {
        parameter_t(std::string itype): type(std::move(itype)) {}

        std::string type;
        std::string name;
    };

    template <typename T>
    static std::string get_typename() {
        if constexpr (LuaNumber<T>)
            return "number";
        if constexpr (LuaStringLike<T>)
            return "string";
        if constexpr (std::is_same_v<T, bool>)
            return "boolean";
        if constexpr (LuaRegisteredType<T>)
            return type_registry::get_typespec<T>().lua_name();
        if constexpr (std::is_pointer_v<T> && LuaRegisteredType<std::remove_const_t<std::remove_pointer_t<T>>>)
            return type_registry::get_typespec<std::remove_const_t<std::remove_pointer_t<T>>>().lua_name();
        if constexpr (LuaFunctionLike<T> || LuaMemberFunction<T>)
            return "function";
        if constexpr (LuaOptionalLike<T>)
            return get_typename<std::decay_t<decltype(*T{})>>() + '?';
        return "table";
    }

    template <typename...>
    struct first_arg_t { using type = void; };

    template <typename T, typename... Ts>
    struct first_arg_t<T, Ts...> { using type = T; };

    template <typename...>
    struct member_function_check {
        bool operator()(const std::string&) const {
            return false;
        }
    };

    template <typename T, typename... Ts>
    struct member_function_check<T, Ts...> {
        bool operator()(const std::string& usertype_name) {
            if constexpr (LuaRegisteredType<T>)
                return type_registry::get_typespec<T>().lua_name() == usertype_name;
            if constexpr (LuaRegisteredType<std::remove_const_t<std::remove_pointer_t<T>>>)
                return type_registry::get_typespec<std::remove_const_t<std::remove_pointer_t<T>>>().lua_name() ==
                       usertype_name;
            return false;
        }
    };

    template <typename ReturnT, typename... ArgsT>
    assist_function(std::string name,
                    const details::lua_function_traits<ReturnT (*)(ArgsT...)>&,
                    const std::string&              holder_table_name,
                    const std::vector<std::string>& argument_names = {}):
        assist_value_base(std::move(name)),
        parameters{parameter_t(get_typename<std::decay_t<ArgsT>>())...},
        return_type(get_typename<std::decay_t<ReturnT>>()),
        self_allowed(member_function_check<std::decay_t<ArgsT>...>{}(holder_table_name)) {

        std::string name_c     = self_allowed ? "self" : "a";
        auto        arg_name_b = argument_names.begin();
        auto        arg_name_e = argument_names.end();

        for (auto& param : parameters) {
            if (arg_name_b != arg_name_e) {
                param.name = *arg_name_b;
                ++arg_name_b;
            }
            else {
                param.name = name_c;
                if (name_c == "self")
                    name_c = "a";
                else
                    ++name_c[0];
            }
        }
    }

    std::string info() const final {
        return name + ": function";
    }

    void accept(assist_visitor& visitor) const final {
        visitor.visit(*this);
    }

    bool is_member_function() const final {
        return self_allowed;
    }

    std::vector<parameter_t> parameters;
    std::string              return_type;
    bool                     self_allowed;
};

struct assist_overloaded_function : public assist_value_base {
    assist_overloaded_function(std::string name): assist_value_base(std::move(name)) {}

    std::string info() const final {
        return name + ": overloded function";
    }

    void accept(assist_visitor& visitor) const final {
        visitor.visit(*this);
    }

    std::vector<assist_function> overloads;
};

struct assist_class_declaration : public assist_table {
    assist_class_declaration(std::string name): assist_table(std::move(name)) {}

    std::string info() const final {
        return name + ": class";
    }

    void accept(assist_visitor& visitor) const final {
        visitor.visit(*this);
    }
};

inline bool operator<(const std::unique_ptr<assist_value_base>& lhs, const std::unique_ptr<assist_value_base>& rhs) {
    return lhs->name < rhs->name;
}

struct annotation_spec {
    std::string              comment        = {};
    std::vector<std::string> argument_names = {};
    std::string              explicit_type  = {};
    bool                     store_value    = false;
};

class annotator {
public:
    annotator() {
        /* Register all usertypes */
        tforeach<typespec_list<0>>([this](auto typespec) {
            auto class_name = std::string(typespec.lua_name());
            if (!values.emplace(class_name, std::make_unique<assist_class_declaration>(class_name)).second)
                warnings.push_back("The usertype " + std::string(class_name.data()) + " was defined twice");
        });
    }

    [[nodiscard]]
    bool warning_exists() const {
        return !warnings.empty();
    }

    void handle_warnings(auto&& handler) {
        for (auto& warn : warnings)
            handler(warn);
        warnings.clear();
    }

    void annotate(const annotation_spec& annotation) {
        annotations.push(annotation);
    }

    void enable_store_values(bool value = true) {
        force_store_value = value;
    }

    void disable_store_values() {
        enable_store_values(false);
    }

    bool store_values_enabled() const {
        return force_store_value;
    }

    void enable_implicit_mode(bool value = true) {
        implicit_mode = value;
    }

    void disable_implicit_mode() {
        enable_implicit_mode(false);
    }

    bool implicit_mode_enabled() const {
        return implicit_mode;
    }

    void traverse(assist_visitor& visitor) const {
        for (auto& [_, v] : global.values)
            v->accept(visitor);
    }

    template <typename... Ts>
    void provide_value(const auto& name) {
        provide_value_main<Ts...>(name, "{}", &global);
    }

    template <typename T, typename... Ts>
    void provide_value(const auto& name, const T& value) {
        provide_value_main<T, Ts...>(name, strcast(value), &global);
    }

private:
    bool store_value_enabled() const {
        return force_store_value || (!annotations.empty() && annotations.front().store_value);
    }

    std::vector<std::string> argument_names() const {
        if (!annotations.empty())
            return annotations.front().argument_names;
        return {};
    }

    std::string comment() const {
        if (!annotations.empty())
            return annotations.front().comment;
        return {};
    }

    const std::string& get_type(const std::string& default_type) const {
        if (!annotations.empty() && !annotations.front().explicit_type.empty())
            return annotations.front().explicit_type;
        return default_type;
    }

    std::string strcast(auto&&) {
        return "{}";
    }

    std::string strcast(const LuaNumber auto& num) {
        if (!store_value_enabled())
            return "{}";

        std::stringstream ss;
        ss << num;
        return ss.str();
    }

    std::string strcast(const LuaStringLike auto& str) {
        if (!store_value_enabled())
            return "{}";

        return std::string(str);
    }

    std::string strcast(const LuaOptionalLike auto& opt) {
        if (!store_value_enabled())
            return "{}";

        if (opt)
            return strcast(*opt);
        return "nil";
    }

    template <std::same_as<std::nullptr_t>>
    assist_value_base* provide_value_impl(const auto& name, const std::string& strvalue, assist_table* table) {
        return table->values.insert_or_assign(name, std::make_unique<assist_value>(get_type("any"), name, strvalue))
            .first->second.get();
    }

    template <LuaNumber>
    assist_value_base* provide_value_impl(const auto& name, const std::string& strvalue, assist_table* table) {
        return table->values.insert_or_assign(name, std::make_unique<assist_value>("number", name, strvalue))
            .first->second.get();
    }
    template <typename>
    struct is_c_str : std::false_type {};

    template <size_t N>
    struct is_c_str<char[N]> : std::true_type {};

    template <typename T> requires LuaStringLike<T> || is_c_str<T>::value
    assist_value_base* provide_value_impl(const auto& name, const std::string& strvalue, assist_table* table) {
        return table->values.insert_or_assign(name, std::make_unique<assist_value>("string", name, strvalue))
            .first->second.get();
    }

    template <std::same_as<bool>>
    assist_value_base* provide_value_impl(const auto& name, const std::string& strvalue, assist_table* table) {
        return table->values.insert_or_assign(name, std::make_unique<assist_value>("boolean", name, strvalue))
            .first->second.get();
    }

    template <LuaRegisteredType T>
    assist_value_base* provide_value_impl(const auto& name, const std::string&, assist_table* table) {
        return table->values
            .insert_or_assign(name, std::make_unique<assist_value>(type_registry::get_typespec<T>().lua_name(), name))
            .first->second.get();
    }

    template <typename... Ts>
        requires(sizeof...(Ts) == 0) || ((LuaTupleLike<Ts> || LuaListLike<Ts>) && ... && true)
    assist_value_base* provide_value_impl(const auto& name, const std::string&, assist_table* table) {
        return table->values.insert_or_assign(name, std::make_unique<assist_value>(get_type("any"), name, "{}"))
            .first->second.get();
    }

    template <typename FT, typename... FTs>
    assist_value_base* provide_func_impl(const auto& name, FT ft, assist_table* table, FTs... fts) {
        if constexpr (sizeof...(FTs) == 0) {
            return table->values
                .insert_or_assign(name, std::make_unique<assist_function>(name, ft, table->name, argument_names()))
                .first->second.get();
        }
        else {
            auto overloaded_function = static_cast<assist_overloaded_function*>( // NOLINT
                table->values
                    .insert_or_assign(
                        name,
                        std::make_unique<assist_overloaded_function>(table == &global ? std::string(name) : "table"))
                    .first->second.get());

            auto& overloads = overloaded_function->overloads;

            overloads.emplace_back(name, ft, table->name, argument_names());
            overloads.back().comment = comment();

            auto push_f = [&](auto trait) {
                if (!annotations.empty())
                    annotations.pop();

                overloads.emplace_back(name, trait, table->name, argument_names());
                overloads.back().comment = comment();
            };
            (push_f(fts), ...);

            return overloaded_function;
        }
    }

    template <typename F>
    static auto make_function_trait() {
        if constexpr (LuaFunctionLike<F>) {
            return details::lua_function_traits<F>{};
        }
        else {
            return []<bool IsConst, bool IsNoexcept, typename ReturnT, typename ClassT, typename... ArgsT>(
                const details::lua_member_function_traits_base<IsConst, IsNoexcept, ReturnT, ClassT, ArgsT...>&) {
                return details::lua_function_traits<ReturnT (*)(ClassT, ArgsT...)>{};
            }
            (details::lua_member_function_traits<F>{});
        }
    }

    template <typename F, typename... Fs>
        requires(LuaFunctionLike<F> || LuaMemberFunction<F>) && ((LuaFunctionLike<Fs> || LuaMemberFunction<Fs>) && ...)
    assist_value_base* provide_value_impl(const auto& name, const std::string&, assist_table* table) {
        return provide_func_impl(name, make_function_trait<F>(), table, make_function_trait<Fs>()...);
    }

    template <LuaOptionalLike>
    assist_value_base* provide_value_impl(const auto& name, const std::string& strvalue, assist_table* table) {
        return table->values.insert_or_assign(name, std::make_unique<assist_value>("any", name, strvalue))
            .first->second.get();
    }

    template <typename... Ts>
    void provide_value_main(const auto& name, const std::string& strvalue, assist_table* table) {
        if (!implicit_mode && annotations.empty())
            return;

        auto divide_result = name.divide_by(int_const<'.'>());
        if (divide_result) {
            auto left  = divide_result.left();
            auto right = divide_result.right();

            auto found = table->values.find(left);
            if (found == table->values.end() || !found->second->is_table()) {
                if (found != table->values.end() && !found->second->is_table())
                    warnings.push_back("Value \"" + found->second->info() + "\" will be replaced by table");
                found = table->values.insert_or_assign(left, std::make_unique<assist_table>(left)).first;
            }
            provide_value_main<Ts...>(
                std::move(right), strvalue, static_cast<assist_table*>(found->second.get())); // NOLINT
            return;
        }
        auto new_value = provide_value_impl<Ts...>(name, strvalue, table);

        new_value->comment = comment();

        if (!annotations.empty())
            annotations.pop();
    }

private:
    assist_table                    global;
    decltype(assist_table::values)& values = global.values;
    std::vector<std::string>        warnings;

    std::queue<annotation_spec> annotations;
    bool                        force_store_value = false;
    bool                        implicit_mode     = true;
};

class assist_printer_visitor : public assist_visitor {
public:
    void append_comment(const assist_value_base& value, bool functions_text = false) {
        if (!value.comment.empty()) {
            append_text(functions_text, "---", value.comment, '\n');
            put_indent(functions_text);
        }
    }

    void visit(const assist_value& value) final {
        append_text(false, '\n');
        put_indent();
        append_comment(value, false);
        append_text(false, "---@type ", value.type, '\n');
        put_indent();
        append_text(false, value.name, " = ", value.value.empty() ? "nil" : value.value);
    }

    void visit(const assist_table& table) final {
        append_text(false, '\n');
        put_indent();
        append_comment(table, false);
        append_text(false, table.name, " = {\n");
        increment_indent();

        for (auto& [_, inner] : table.values) {
            inner->accept(*this);
        }
        var_text.pop_back();

        decrement_indent();
        append_text(false, "}");
    }

    void visit(const assist_function& function) final {
        auto func_location = function.self_allowed;

        append_text(func_location, '\n');
        put_indent(func_location);
        append_comment(function, func_location);

        auto param_b = function.parameters.begin();
        auto param_e = function.parameters.end();
        if (function.self_allowed)
            ++param_b;
        for (auto b = param_b; b < param_e; ++b) {
            append_text(func_location, "---@param ", b->name, ' ', b->type, '\n');
            put_indent(func_location);
        }
        append_text(func_location, "---@return ", function.return_type, '\n');
        put_indent(func_location);

        if (function.self_allowed) {
            append_text(true, "function ", function.parameters.front().type, ':', function.name, '(');
            for (auto b = param_b; b < param_e; ++b)
                append_text(true, b->name, ", ");
            if (param_b < param_e)
                func_text.resize(func_text.size() - 2);
        }
        else {
            append_text(false, function.name, " = function(");
            for (auto b = param_b; b < param_e; ++b)
                append_text(false, b->name, ", ");
            if (param_b != param_e)
                var_text.resize(var_text.size() - 2);
        }
        append_text(func_location, ") end");
    }

    void visit(const assist_overloaded_function& overloaded_function) final {
        bool need_comma = overloaded_function.name == "table";
        bool insert_was = false;

        for (auto& function : overloaded_function.overloads) {
            function.accept(*this);
            if (need_comma && !function.self_allowed) {
                append_text(false, ',');
                insert_was = true;
            }
        }

        if (insert_was)
            var_text.pop_back();
    }

    void visit(const assist_class_declaration& class_declaration) final {
        append_text(false, '\n');

        put_indent();
        append_comment(class_declaration, false);
        append_text(false, "---@class ", class_declaration.name, '\n');
        put_indent();
        append_text(false, class_declaration.name, " = {");
        increment_indent();

        for (auto& [_, inner] : class_declaration.values) {
            inner->accept(*this);
            if (!inner->is_member_function())
                append_text(false, ',');
        }

        append_text(false, '\n');
        put_indent();
        append_text(false, "__index = ", class_declaration.name, '\n');
        decrement_indent();
        append_text(false, "}");
    }

    void put_indent(bool functions_location = false) {
        if (functions_location)
            func_text.resize(func_text.size() + func_indent, ' ');
        else
            var_text.resize(var_text.size() + var_indent, ' ');
    }

    void increment_indent(bool functions_location = false) {
        (functions_location ? func_indent : var_indent) += 4;
    }

    void decrement_indent(bool functions_location = false) {
        auto& ind = (functions_location ? func_indent : var_indent);
        if (ind >= 4)
            ind -= 4;
        else
            ind = 0;
    }

    void append_text(bool functions_location, auto&&... text) {
        if (functions_location)
            ((func_text += text), ...);
        else
            ((var_text += text), ...);
    }

    std::string result() const {
        std::string res = var_text;
        res += "\n\n";
        res += func_text;
        return res;
    }

private:
    size_t      var_indent  = 0;
    size_t      func_indent = 0;
    std::string var_text;
    std::string func_text;
};
} // namespace luacpp
