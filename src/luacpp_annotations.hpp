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
struct assist_class_declaration;

class assist_visitor {
public:
    virtual ~assist_visitor() = default;
    virtual void visit(const assist_value&) = 0;
    virtual void visit(const assist_table&) = 0;
    virtual void visit(const assist_function&) = 0;
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
        if constexpr (LuaInteger<T>)
            return "integer";
        if constexpr (LuaFloat<T>)
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
    void add_overload(const details::lua_function_traits<ReturnT (*)(ArgsT...)>&,
                      const std::vector<std::string>& argument_names) {
        overloads.emplace_back(std::vector<parameter_t>{parameter_t(get_typename<std::decay_t<ArgsT>>())...},
                               get_typename<std::decay_t<ReturnT>>(),
                               member_function_check<std::decay_t<ArgsT>...>{}(holder_table_name));
        auto& overload = overloads.back();

        std::string name_c     = overload.self_allowed ? "self" : "a";
        auto        arg_name_b = argument_names.begin();
        auto        arg_name_e = argument_names.end();

        for (auto& param : overload.parameters) {
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

    template <typename ReturnT, typename... ArgsT>
    assist_function(std::string name,
                    const details::lua_function_traits<ReturnT (*)(ArgsT...)>& f,
                    std::string                     iholder_table_name,
                    const std::vector<std::string>& argument_names = {}):
        assist_value_base(std::move(name)), holder_table_name(std::move(iholder_table_name)) {
        add_overload(f, argument_names);
    }

    std::string info() const final {
        return name + ": function";
    }

    void accept(assist_visitor& visitor) const final {
        visitor.visit(*this);
    }

    struct overload_t {
        std::vector<parameter_t> parameters;
        std::string              return_type;
        bool                     self_allowed;
    };

    std::string             holder_table_name;
    std::vector<overload_t> overloads;
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
        provide_value_main<Ts...>(name, "nil", &global);
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
            return "nil";

        std::stringstream ss;
        ss << num;
        return ss.str();
    }

    std::string strcast(const LuaStringLike auto& str) {
        if (!store_value_enabled())
            return "nil";

        return std::string(str);
    }

    std::string strcast(const LuaOptionalLike auto& opt) {
        if (!store_value_enabled())
            return "nil";

        if (opt)
            return strcast(*opt);
        return "nil";
    }

    template <std::same_as<std::nullptr_t>>
    assist_value_base* provide_value_impl(const auto& name, const std::string& strvalue, assist_table* table) {
        return table->values.insert_or_assign(name, std::make_unique<assist_value>(get_type("any"), name, strvalue))
            .first->second.get();
    }

    template <LuaInteger>
    assist_value_base* provide_value_impl(const auto& name, const std::string& strvalue, assist_table* table) {
        return table->values.insert_or_assign(name, std::make_unique<assist_value>("integer", name, strvalue))
            .first->second.get();
    }

    template <LuaFloat>
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
        return table->values.insert_or_assign(name, std::make_unique<assist_value>(get_type("any"), name))
            .first->second.get();
    }

    template <typename FT, typename... FTs>
    assist_value_base* provide_func_impl(const auto& name, FT ft, assist_table* table, FTs... fts) {
        auto func = static_cast<assist_function*>(
            table->values
                .insert_or_assign(name, std::make_unique<assist_function>(name, ft, table->name, argument_names()))
                .first->second.get());

        if constexpr (sizeof...(FTs) > 0) {
            auto add_overload = [this](assist_function* func, const auto& ft) {
                if (!annotations.empty())
                    annotations.pop();
                func->add_overload(ft, argument_names());
            };
            (add_overload(func, fts), ...);
        }

        return func;
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
        auto comment_str = comment();
        auto new_value = provide_value_impl<Ts...>(name, strvalue, table);

        new_value->comment = std::move(comment_str);

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
    void append_comment(const assist_value_base& value) {
        if (!value.comment.empty()) {
            append_text("---", value.comment, '\n');
            put_indent();
        }
    }

    void visit(const assist_value& value) final {
        append_text('\n');
        put_indent();
        append_comment(value);
        append_text("---@type ", value.type, '\n');
        put_indent();
        append_text(value.name, " = ", value.value.empty() ? "nil" : value.value);
    }

    void visit(const assist_table& table) final {
        append_text('\n');
        put_indent();
        append_comment(table);
        append_text(table.name, " = {");
        increment_indent();

        bool was_inner = false;
        for (auto& [_, inner] : table.values) {
            inner->accept(*this);
            append_text(',');
            was_inner = true;
        }
        if (was_inner)
            text.pop_back();

        append_text('\n');
        decrement_indent();
        append_text("}");
    }

    void visit(const assist_function& function) final {
        append_text('\n');
        put_indent();
        append_comment(function);

        auto& overload1 = function.overloads.front();

        for (auto& param : overload1.parameters) {
            append_text("---@param ", param.name, ' ', param.type, '\n');
            put_indent();
        }
        append_text("---@return ", overload1.return_type, '\n');
        put_indent();

        if (function.overloads.size() > 1) {
            for (size_t i = 1 ; i < function.overloads.size(); ++i) {
                auto& overload = function.overloads[i];
                append_text("---@overload fun(");
                for (auto& param : overload.parameters)
                    append_text(param.name, ':', param.type, ',');
                if (!overload.parameters.empty())
                    text.pop_back();
                append_text("):", overload.return_type, '\n');
                put_indent();
            }
        }

        append_text(function.name, " = function(");
        for (auto& param : overload1.parameters)
            append_text(param.name, ", ");
        if (!overload1.parameters.empty())
            text.resize(text.size() - 2);
        append_text(") end");
    }

    void visit(const assist_class_declaration& class_declaration) final {
        append_text('\n');

        put_indent();
        append_comment(class_declaration);
        append_text("---@class ", class_declaration.name, '\n');
        put_indent();
        append_text(class_declaration.name, " = {");
        increment_indent();

        for (auto& [_, inner] : class_declaration.values) {
            inner->accept(*this);
            if (!inner->is_member_function())
                append_text(',');
        }

        append_text('\n');
        put_indent();
        append_text("__index = ", class_declaration.name, '\n');
        decrement_indent();
        append_text("}");
    }

    void put_indent() {
        text.resize(text.size() + indent, ' ');
    }

    void increment_indent() {
        indent += 4;
    }

    void decrement_indent() {
        if (indent >= 4)
            indent -= 4;
        else
            indent = 0;
    }

    void append_text(auto&&... txt) {
        ((text += txt), ...);
    }

    [[nodiscard]]
    const std::string& result() const {
        return text;
    }

private:
    size_t      indent  = 0;
    std::string text;
};
} // namespace luacpp
