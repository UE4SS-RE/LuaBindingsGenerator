#ifndef LUA_WRAPPER_GENERATOR_TESTING_PARSER_OUTPUT_HPP
#define LUA_WRAPPER_GENERATOR_TESTING_PARSER_OUTPUT_HPP

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <filesystem>
#include <format>

#include <clang-c/Index.h>

namespace RC::LuaWrapperGenerator
{
    class Function;
    struct FunctionParam;
    struct Class;
    class Enum;

    namespace Type
    {
        class FunctionProto;
        class Base;
    }

    enum class IsPointer { Yes, No };
    enum class IsFunctionScopeless { Yes, No };

    auto scope_as_function_name(std::string_view scope) -> std::string;

    using FunctionContainer = std::unordered_map<std::string, Function>;
    using ClassContainer = std::unordered_map<std::string, Class>;
    using EnumContainer = std::unordered_map<std::string, Enum>;
    using FunctionProtoContainer = std::unordered_map<std::string, Type::FunctionProto*>;

    struct DoNotParseException : public std::exception
    {
        std::string reason{};
        bool is_verbose{true};
        void* data{};

        DoNotParseException(const std::string why, bool is_verbose = true, void* data = nullptr) : reason(std::move(why)), is_verbose(is_verbose), data(data) {};
    };

    struct Container
    {
        FunctionContainer functions;
        ClassContainer classes;
        ClassContainer thin_classes;
        std::unordered_set<std::string> lua_state_types;
        mutable std::vector<std::string> extra_includes{};
        EnumContainer enums;
        FunctionProtoContainer function_proto_container;

        auto find_class_by_name(std::string_view fully_qualified_scope, std::string_view name) const -> const Class*;
        auto find_mutable_class_by_name(std::string_view fully_qualified_scope, std::string_view name) -> Class*;
    };

    auto add_function_to_container(FunctionContainer& container, const std::string& function_name, const std::string& parent_name, const std::string& full_path_to_file, const std::string& fully_qualified_scope, const std::string& parent_scope, Class* contained_in_class = nullptr, IsFunctionScopeless = IsFunctionScopeless::No) -> Function&;

    class Function
    {
    private:
        std::string m_name{};
        std::string m_lua_name{};
        std::string m_wrapper_name{};
        std::string m_parent_name{};
        std::string m_parent_scope{};
        std::string m_fully_qualified_scope{};
        std::string m_scope_override{};
        std::string m_full_path_to_file{};
        std::vector<std::vector<FunctionParam>> m_overloads{};
        std::unique_ptr<Type::Base> m_return_type{};
        Class* m_containing_class{};
        bool m_is_custom_redirector{false};
        bool m_shares_file_with_containing_class{true};
        bool m_is_static{false};
        bool m_is_constructor{false};
        bool m_is_alias{false};

    public:
        Function() = default;

        Function(std::string name, std::string parent_name, std::string full_path_to_file, std::string fully_qualified_scope, std::string parent_scope, Type::Base* return_type)
                : m_name(std::move(name)),
                  m_parent_name(std::move(parent_name)),
                  m_full_path_to_file(full_path_to_file),
                  m_fully_qualified_scope(std::move(fully_qualified_scope)),
                  m_parent_scope(std::move(parent_scope)),
                  m_return_type(return_type),
                  m_lua_name(m_name) {}

        Function(std::string name, std::string parent_name, std::string full_path_to_file, std::string fully_qualified_scope, std::string parent_scope, Type::Base* return_type, Class* containing_class)
                : m_name(std::move(name)),
                  m_parent_name(std::move(parent_name)),
                  m_full_path_to_file(full_path_to_file),
                  m_fully_qualified_scope(std::move(fully_qualified_scope)),
                  m_parent_scope(std::move(parent_scope)),
                  m_return_type(return_type),
                  m_containing_class(containing_class),
                  m_lua_name(m_name) {}

    public:
        //auto add_param(std::string param_name, std::unique_ptr<Type::Base> param_type) -> void;

        auto set_return_type(std::unique_ptr<Type::Base> type) -> void { m_return_type = std::move(type); };
        template<typename T>
        auto set_return_type() -> void { set_return_type(T::static_class); }

        auto set_is_custom_redirector(bool new_is_custom_redirector) -> void { m_is_custom_redirector = new_is_custom_redirector; }
        auto is_custom_redirector() const -> bool { return m_is_custom_redirector; }

        auto set_is_alias(bool new_is_alias) -> void { m_is_alias = new_is_alias; }
        auto is_alias() const -> bool { return m_is_alias; }

        auto set_shares_file_with_containing_class(bool new_shares_file_with_containing_class) -> void { m_shares_file_with_containing_class = new_shares_file_with_containing_class; }
        auto shares_file_with_containing_class() const -> bool { return m_shares_file_with_containing_class; }

        auto set_is_static(bool new_is_static) -> void { m_is_static = new_is_static; }
        auto is_static() const -> bool { return m_is_static; }

        auto set_is_constructor(bool new_is_constructor) -> void { m_is_constructor = new_is_constructor; }
        auto is_constructor() const -> bool { return m_is_constructor; }

        auto set_wrapper_name(const std::string& new_wrapper_name) -> void { m_wrapper_name = new_wrapper_name; }
        auto get_wrapper_name() const -> std::string_view { return m_wrapper_name; }

        auto set_scope_override(const std::string& new_override_scope) -> void { m_scope_override = new_override_scope; }
        auto get_scope_override() const -> std::string_view { return m_scope_override; }

        auto set_name(const std::string& new_name) -> void { m_name = new_name; }
        auto get_name() const -> std::string_view { return m_name; }

        auto get_fully_qualified_scope() const -> std::string_view { return m_fully_qualified_scope; }

        auto set_parent_name(const std::string& new_parent_name) -> void { m_parent_name = new_parent_name; }
        auto get_parent_name() const -> std::string_view { return m_parent_name; }

        auto set_lua_name(const std::string& new_lua_name) -> void { m_lua_name = new_lua_name; }
        auto get_lua_name() const -> std::string_view { return m_lua_name; }

        auto set_parent_scope(const std::string& new_parent_scope) -> void { m_parent_scope = new_parent_scope; }
        auto get_parent_scope() const -> std::string_view { return m_parent_scope; }

        auto get_overloads() const -> const std::vector<std::vector<FunctionParam>>& { return m_overloads; }
        auto get_overloads() -> std::vector<std::vector<FunctionParam>>& { return m_overloads; }
        auto get_return_type() const -> Type::Base* { return m_return_type.get(); }
        auto get_full_path_to_file() const -> std::string_view { return m_full_path_to_file; }

        auto set_containing_class(Class* new_containing_class) -> void { m_containing_class = new_containing_class; }
        auto get_containing_class() const -> const Class* { return m_containing_class; }

        auto generate_lua_wrapper_function_body() const -> std::string;
        auto generate_lua_return_statement() const -> std::string;
    };

    struct Class
    {
        class CodeGenerator& code_generator;
        const std::string name;
        const std::string fully_qualified_scope;
        std::string scope_override;
        const std::string full_path_to_file;
        FunctionContainer metamethods{};
        FunctionContainer static_functions{};
        FunctionContainer constructors{};
        Container container;
        bool has_parameterless_constructor{};

    private:
        std::unordered_set<const Class*> bases{};

    public:
        Class(CodeGenerator& code_generator, std::string name, std::string fully_qualified_scope, std::string full_path_to_file) : code_generator(code_generator), name(std::move(name)), fully_qualified_scope(std::move(fully_qualified_scope)), full_path_to_file(std::move(full_path_to_file)) {}

    public:
        auto add_class(const std::string& class_name, const std::string& full_path_to_file, const std::string& fully_qualified_scope) -> Class&;
        auto find_static_function_by_name(std::string_view function_name) const -> const Function*;
        auto get_bases() const -> std::vector<const Class*>;
        auto get_metamethod_by_name(const std::string& metamethod_name) const -> const Function*;
        auto get_mutable_bases() -> std::unordered_set<const Class*>&;

        auto generate_metamethods_map() const -> std::string;
        auto generate_member_functions_map() const -> std::string;
        auto generate_member_functions() const -> std::string;
        auto generate_constructor() const -> std::string;
        auto generate_setup_function() const -> std::string;
        auto generate_create_instance_of_function() const -> std::string;
        auto generate_internal_get_self_function() const -> std::string;

    private:
        auto generate_metamethods_map_contents() const -> std::string;
        auto generate_member_functions_map_contents() const -> std::string;
    };

    class Enum
    {
    private:
        const std::string m_name{};
        const std::string m_fully_qualified_scope{};
        std::vector<std::pair<std::string, uint64_t>> m_keys_and_values{};

    public:
        Enum(std::string name, std::string fully_qualified_name) : m_name(std::move(name)), m_fully_qualified_scope(std::move(fully_qualified_name)) {}

    public:
        auto get_name() const -> std::string_view { return m_name; }
        auto get_fully_qualified_scope() const -> std::string_view { return m_fully_qualified_scope; }
        auto add_key_value_pair(std::string key, uint64_t value) -> void;
        auto get_key_value_pairs() const -> const std::vector<std::pair<std::string, uint64_t>>& { return m_keys_and_values; }
    };

    namespace Type
    {
        class Base
        {
        private:
            const Container& m_container;
            bool m_is_pointer{};
            bool m_is_ref{};
            bool m_is_const{};

        public:
            explicit Base(const Container& container) : m_container(container) {}
            ~Base() = default;

        public:
            virtual auto get_static_class() const -> Base* = 0;
            virtual auto get_super() const -> Base* = 0;
            virtual auto get_fully_qualified_type_name() const -> std::string { throw std::runtime_error{"Direct call to 'Base::generate_unique_type_name' not allowed"}; };
            virtual auto generate_cxx_name() const -> std::string { throw std::runtime_error{"Direct call to 'Base::generate_cxx_name' not allowed"}; };
            virtual auto generate_lua_stack_validation_condition(int stack_index) const -> std::string { throw std::runtime_error{"Direct call to 'Base::generate_lua_stack_validation_condition' not allowed"}; };
            virtual auto generate_lua_overload_resolution_condition(int stack_index) const -> std::string
            {
                return std::format("                return true;");
            }
            virtual auto generate_lua_stack_retriever(int stack_index) const -> std::string { throw std::runtime_error{"Direct call to 'Base::generate_lua_stack_retriever' not allowed"}; };
            virtual auto generate_lua_stack_pusher(std::string_view variable_to_push, std::string_view param_prefix) const -> std::string { throw std::runtime_error{"Direct call to 'Base::generate_lua_stack_pusher' not allowed"}; };
            virtual auto generate_converted_type(size_t param_num, std::vector<std::string>& atomic_resets) const -> std::string { throw std::runtime_error{"Call to 'generate_converted_type' not allowed"}; };
            virtual auto needs_conversion_from_lua() const -> bool { return false; };
            virtual auto generate_extra_processing(int stack_index, int current_param) const -> std::string { throw std::runtime_error{"Call to 'generate_extra_processing' not allowed"}; };
            virtual auto needs_extra_processing() const -> bool { return false; };

            auto set_is_pointer(bool new_is_pointer) -> void { m_is_pointer = new_is_pointer; }
            auto is_pointer() const -> bool { return m_is_pointer; }
            auto set_is_ref(bool new_is_ref) -> void { m_is_ref = new_is_ref; }
            auto is_ref() const -> bool { return m_is_ref; }
            auto set_is_const(bool new_is_const) -> void  { m_is_const = new_is_const; }
            auto is_const() const -> bool { return m_is_const; }
            auto get_container() const -> const Container& { return m_container; }

            template<typename T>
            [[nodiscard]] auto is_a() const -> bool
            {
                if (get_static_class() == T::static_class.get()) { return true; }
                auto super = get_super();

                while (super && super != this)
                {
                    if (super == T::static_class.get())
                    {
                        return true;
                    }
                    else
                    {
                        auto* next_super = super->get_super();
                        if (next_super == super) { break; }
                        super = next_super;
                    }
                }

                return false;
            }
        };

        template<typename T>
        class BaseTemplate : public Base
        {
        public:
            static inline std::unique_ptr<T> static_class{};

        public:
            virtual auto get_static_class() const -> Base* override { return static_class.get(); }
            virtual auto get_super() const -> Base* override { return static_class.get(); };

        public:
            explicit BaseTemplate(const Container& container) : Base(container) {}
        };

        class EmptyBaseType : public BaseTemplate<EmptyBaseType>
        {
        public:
            virtual auto get_super() const -> Base* override { return nullptr; };
            virtual auto generate_cxx_name() const -> std::string override { throw std::runtime_error{"Direct call to 'EmptyBaseType::generate_cxx_name' not allowed"}; }

        public:
            explicit EmptyBaseType(const Container& container) : BaseTemplate<EmptyBaseType>(container) {}
        };

        class Void : public BaseTemplate<Void>
        {
        public:
            virtual auto get_fully_qualified_type_name() const -> std::string override;
            virtual auto generate_cxx_name() const -> std::string override { return "void"; };
            virtual auto generate_lua_stack_validation_condition(int stack_index) const -> std::string override;
            virtual auto generate_lua_stack_retriever(int stack_index) const -> std::string override;
            virtual auto generate_lua_stack_pusher(std::string_view variable_to_push, std::string_view param_prefix) const -> std::string override;

        public:
            explicit Void(const Container& container) : BaseTemplate<Void>(container) {}
        };

        template<typename T>
        class NumericBaseTemplate : public BaseTemplate<T>
        {
        protected:
            virtual auto is_floating_point() const -> bool { return false; }

        public:
            virtual auto generate_lua_stack_validation_condition(int stack_index) const -> std::string override
            {
                if (this->is_pointer())
                {
                    return std::format("lua_islightuserdata(lua_state, {}) || lua_isnil(lua_state, {}) || lua_isuserdata(lua_state, {})", stack_index, stack_index, stack_index);
                }
                else if (this->is_ref())
                {
                    return std::format("lua_islightuserdata(lua_state, {}) || lua_isuserdata(lua_state, {})", stack_index, stack_index);
                }
                else
                {
                    return std::format("lua_isnumber(lua_state, {})", stack_index);
                }
            }

            virtual auto generate_lua_stack_retriever(int stack_index) const -> std::string override
            {
                if (this->is_pointer())
                {
                    return std::format("static_cast<{}{}{}>(lua_isnil(lua_state, {}) ? nullptr : lua_touserdata(lua_state, {}))",
                                       this->is_const() ? "const " : "",
                                       this->generate_cxx_name(),
                                       this->is_pointer() ? "*" : "",
                                       stack_index,
                                       stack_index);
                }
                else if (this->is_ref())
                {
                    return std::format("*static_cast<{}{}*>(lua_touserdata(lua_state, {}))",
                                       this->is_const() ? "const " : "",
                                       this->generate_cxx_name(),
                                       stack_index,
                                       stack_index);
                }
                else
                {
                    return std::format("static_cast<{}>(lua_tonumber(lua_state, {}))", this->generate_cxx_name(), stack_index);
                }
            }

            virtual auto generate_lua_stack_pusher(std::string_view variable_to_push, std::string_view param_prefix) const -> std::string override
            {
                if (this->is_pointer())
                {
                    return std::format("        lua_pushlightuserdata({}lua_state, {})", param_prefix, variable_to_push);
                }
                else
                {
                    return std::format("        lua_{}({}lua_state, static_cast<{}>({}))", is_floating_point() ? "pushnumber" : "pushinteger", param_prefix, this->generate_cxx_name(), variable_to_push);
                }
            }

        public:
            explicit NumericBaseTemplate(const Container& container) : BaseTemplate<T>(container) {}
        };
        using NumericBase = NumericBaseTemplate<EmptyBaseType>;

        class Int8 : public NumericBaseTemplate<Int8>
        {
        public:
            virtual auto get_fully_qualified_type_name() const -> std::string override;
            virtual auto generate_cxx_name() const -> std::string override { return "int8_t"; };

        public:
            explicit Int8(const Container& container) : NumericBaseTemplate<Int8>(container) {}
        };

        class Int16 : public NumericBaseTemplate<Int16>
        {
        public:
            virtual auto get_fully_qualified_type_name() const -> std::string override;
            virtual auto generate_cxx_name() const -> std::string override { return "int16_t"; };

        public:
            explicit Int16(const Container& container) : NumericBaseTemplate<Int16>(container) {}
        };

        class Int32 : public NumericBaseTemplate<Int32>
        {
        public:
            virtual auto get_fully_qualified_type_name() const -> std::string override;
            virtual auto generate_cxx_name() const -> std::string override { return "int32_t"; };

        public:
            explicit Int32(const Container& container) : NumericBaseTemplate<Int32>(container) {}
        };

        class Int64 : public NumericBaseTemplate<Int64>
        {
        public:
            virtual auto get_fully_qualified_type_name() const -> std::string override;
            virtual auto generate_cxx_name() const -> std::string override { return "int64_t"; };

        public:
            explicit Int64(const Container& container) : NumericBaseTemplate<Int64>(container) {}
        };

        class UInt8 : public NumericBaseTemplate<UInt8>
        {
        public:
            virtual auto get_fully_qualified_type_name() const -> std::string override;
            virtual auto generate_cxx_name() const -> std::string override { return "uint8_t"; };

        public:
            explicit UInt8(const Container& container) : NumericBaseTemplate<UInt8>(container) {}
        };

        class UInt16 : public NumericBaseTemplate<UInt16>
        {
        public:
            virtual auto get_fully_qualified_type_name() const -> std::string override;
            virtual auto generate_cxx_name() const -> std::string override { return "uint16_t"; };

        public:
            explicit UInt16(const Container& container) : NumericBaseTemplate<UInt16>(container) {}
        };

        class UInt32 : public NumericBaseTemplate<UInt32>
        {
        public:
            virtual auto get_fully_qualified_type_name() const -> std::string override;
            virtual auto generate_cxx_name() const -> std::string override { return "uint32_t"; };

        public:
            explicit UInt32(const Container& container) : NumericBaseTemplate<UInt32>(container) {}
        };

        class UInt64 : public NumericBaseTemplate<UInt64>
        {
        public:
            virtual auto get_fully_qualified_type_name() const -> std::string override;
            virtual auto generate_cxx_name() const -> std::string override { return "uint64_t"; };

        public:
            explicit UInt64(const Container& container) : NumericBaseTemplate<UInt64>(container) {}
        };

        class Float : public NumericBaseTemplate<Float>
        {
        public:
            virtual auto get_fully_qualified_type_name() const -> std::string override;
            virtual auto generate_cxx_name() const -> std::string override { return "float"; };
            virtual auto is_floating_point() const -> bool override { return true; }

        public:
            explicit Float(const Container& container) : NumericBaseTemplate<Float>(container) {}
        };

        class Double : public NumericBaseTemplate<Double>
        {
        public:
            virtual auto get_fully_qualified_type_name() const -> std::string override;
            virtual auto generate_cxx_name() const -> std::string override { return "double"; };
            virtual auto is_floating_point() const -> bool override { return true; }

        public:
            explicit Double(const Container& container) : NumericBaseTemplate<Double>(container) {}
        };

        template<typename T>
        class StringBaseTemplate : public BaseTemplate<T>
        {
        public:
            //virtual auto generate_lua_stack_validation_condition(int stack_index) const -> std::string override { throw std::runtime_error{"Direct call to 'StringBaseTemplate::generate_lua_stack_validation_condition' not allowed"}; };
            virtual auto generate_lua_stack_validation_condition(int stack_index) const -> std::string override
            {
                return std::format("lua_isstring(lua_state, {})", stack_index);
            }

            // Generate a type that the C++ function can use.
            // Only generates the type, does not convert any the string.
            virtual auto generate_usable_cxx_string_type() const -> std::string { throw std::runtime_error{"Direct call to 'StringBase::generate_usable_cxx_string_type' not allowed"}; };

            // Generate a type that Lua can use.
            // Only generates the type, does not convert any the string.
            virtual auto generate_usable_lua_string_type() const -> std::string { throw std::runtime_error{"Direct call to 'StringBase::generate_usable_lua_string_type' not allowed"}; };

        public:
            explicit StringBaseTemplate(const Container& container) : BaseTemplate<T>(container) {}
        };
        using StringBase = StringBaseTemplate<EmptyBaseType>;

        class CString : public StringBaseTemplate<CString>
        {
        public:
            virtual auto get_super() const -> Base* override { return StringBaseTemplate<EmptyBaseType>::static_class.get(); };
            virtual auto get_fully_qualified_type_name() const -> std::string override;
            virtual auto generate_cxx_name() const -> std::string override { return "char*"; };
            //virtual auto generate_lua_stack_validation_condition(int stack_index) const -> std::string override;
            virtual auto generate_lua_stack_retriever(int stack_index) const -> std::string override;
            virtual auto generate_lua_stack_pusher(std::string_view variable_to_push, std::string_view param_prefix) const -> std::string override;
            virtual auto generate_converted_type(size_t param_num, std::vector<std::string>& atomic_resets) const -> std::string override;

            virtual auto generate_usable_cxx_string_type() const -> std::string override { return "char* for c++"; };
            virtual auto generate_usable_lua_string_type() const -> std::string override { return "char* for lua"; };

        public:
            explicit CString(const Container& container) : StringBaseTemplate<CString>(container) {}
        };

        class CWString : public StringBaseTemplate<CWString>
        {
        public:
            virtual auto get_super() const -> Base* override { return StringBaseTemplate<EmptyBaseType>::static_class.get(); };
            virtual auto get_fully_qualified_type_name() const -> std::string override;
            virtual auto generate_cxx_name() const -> std::string override { return "wchar_t*"; };
            //virtual auto generate_lua_stack_validation_condition(int stack_index) const -> std::string override;
            virtual auto generate_lua_stack_retriever(int stack_index) const -> std::string override;
            virtual auto generate_lua_stack_pusher(std::string_view variable_to_push, std::string_view param_prefix) const -> std::string override;
            virtual auto generate_converted_type(size_t param_num, std::vector<std::string>& atomic_resets) const -> std::string override;

            virtual auto generate_usable_cxx_string_type() const -> std::string override { return "wchar_t* for c++"; };
            virtual auto generate_usable_lua_string_type() const -> std::string override { return "wchar_t* for lua"; };
            virtual auto needs_conversion_from_lua() const -> bool override { return true; };

        public:
            explicit CWString(const Container& container) : StringBaseTemplate<CWString>(container) {}
        };

        class String : public StringBaseTemplate<String>
        {
        public:
            virtual auto get_super() const -> Base* override { return StringBaseTemplate<EmptyBaseType>::static_class.get(); };
            virtual auto get_fully_qualified_type_name() const -> std::string override;
            virtual auto generate_cxx_name() const -> std::string override { return "std::string"; };
            //virtual auto generate_lua_stack_validation_condition(int stack_index) const -> std::string override;
            virtual auto generate_lua_stack_retriever(int stack_index) const -> std::string override;
            virtual auto generate_lua_stack_pusher(std::string_view variable_to_push, std::string_view param_prefix) const -> std::string override;
            virtual auto generate_converted_type(size_t param_num, std::vector<std::string>& atomic_resets) const -> std::string override;

            virtual auto generate_usable_cxx_string_type() const -> std::string override { return "std::string for c++"; };
            virtual auto generate_usable_lua_string_type() const -> std::string override { return "std::string for lua"; };

        public:
            explicit String(const Container& container) : StringBaseTemplate<String>(container) {}
        };

        class WString : public StringBaseTemplate<WString>
        {
        public:
            virtual auto get_super() const -> Base* override { return StringBaseTemplate<EmptyBaseType>::static_class.get(); };
            virtual auto get_fully_qualified_type_name() const -> std::string override;
            virtual auto generate_cxx_name() const -> std::string override { return "std::wstring"; };
            //virtual auto generate_lua_stack_validation_condition(int stack_index) const -> std::string override;
            virtual auto generate_lua_stack_retriever(int stack_index) const -> std::string override;
            virtual auto generate_lua_stack_pusher(std::string_view variable_to_push, std::string_view param_prefix) const -> std::string override;
            virtual auto generate_converted_type(size_t param_num, std::vector<std::string>& atomic_resets) const -> std::string override;
            virtual auto needs_conversion_from_lua() const -> bool override { return true; };

            virtual auto generate_usable_cxx_string_type() const -> std::string override { return "std::wstring for c++"; };
            virtual auto generate_usable_lua_string_type() const -> std::string override { return "std::wstring for lua"; };

        public:
            explicit WString(const Container& container) : StringBaseTemplate<WString>(container) {}
        };

        class AutoString : public StringBaseTemplate<AutoString>
        {
        public:
            bool m_is_wide_string{true};

        public:
            virtual auto get_super() const -> Base* override { return StringBaseTemplate<EmptyBaseType>::static_class.get(); };
            virtual auto get_fully_qualified_type_name() const -> std::string override;
            virtual auto generate_cxx_name() const -> std::string override { return "RC::File::StringType"; };
            //virtual auto generate_lua_stack_validation_condition(int stack_index) const -> std::string override;
            virtual auto generate_lua_stack_retriever(int stack_index) const -> std::string override;
            virtual auto generate_lua_stack_pusher(std::string_view variable_to_push, std::string_view param_prefix) const -> std::string override;
            virtual auto generate_converted_type(size_t param_num, std::vector<std::string>& atomic_resets) const -> std::string override;
            virtual auto needs_conversion_from_lua() const -> bool override { return true; };

            virtual auto generate_usable_cxx_string_type() const -> std::string override { return "RC::File::StringType for c++"; };
            virtual auto generate_usable_lua_string_type() const -> std::string override { return "RC::File::StringType for lua"; };

        public:
            explicit AutoString(const Container& container) : StringBaseTemplate<AutoString>(container) {}
        };

        class CustomStruct : public BaseTemplate<CustomStruct>
        {
        private:
            const std::string m_type_name{};
            std::string m_fully_qualified_scope{};
            bool m_is_struct_forward_declaration{};
            bool m_is_class_forward_declaration{};
            bool m_move_on_construction{};

        public:
            virtual auto get_fully_qualified_type_name() const -> std::string override;
            virtual auto generate_cxx_name() const -> std::string override;
            virtual auto generate_lua_stack_validation_condition(int stack_index) const -> std::string override;
            virtual auto generate_lua_overload_resolution_condition(int stack_index) const -> std::string override;
            virtual auto generate_lua_stack_retriever(int stack_index) const -> std::string override;
            virtual auto generate_lua_stack_pusher(std::string_view variable_to_push, std::string_view param_prefix) const -> std::string override;
            virtual auto needs_extra_processing() const -> bool override { return true; };
            virtual auto generate_extra_processing(int stack_index, int current_param) const -> std::string override;

        public:
            explicit CustomStruct(const Container& container) : BaseTemplate<CustomStruct>(container) {}
             CustomStruct(const Container& container, std::string type_name) : BaseTemplate<CustomStruct>(container), m_type_name(std::move(type_name)) {}

        public:
            auto set_is_struct_forward_declaration(bool new_is_forward_declaration) -> void
            {
                m_is_struct_forward_declaration = new_is_forward_declaration;
                m_is_class_forward_declaration = !new_is_forward_declaration;
            }
            auto is_struct_forward_declaration() const -> bool { return m_is_struct_forward_declaration; }

            auto set_is_class_forward_declaration(bool new_is_forward_declaration) -> void
            {
                m_is_class_forward_declaration = new_is_forward_declaration;
                m_is_struct_forward_declaration = !new_is_forward_declaration;
            }
            auto is_class_forward_declaration() const -> bool { return m_is_class_forward_declaration; }

            auto set_fully_qualified_scope(std::string new_fully_qualified_scope) -> void
            {
                m_fully_qualified_scope = std::move(new_fully_qualified_scope);
            }
            auto get_fully_qualified_scope() const -> std::string_view { return m_fully_qualified_scope; }

            auto get_type_name() const -> std::string_view { return m_type_name; }

            auto set_move_on_construction(bool new_value) -> void { m_move_on_construction = new_value; }
            auto should_move_on_construction() const -> bool { return m_move_on_construction; }
        };

        /**/
        class Enum : public BaseTemplate<Enum>
        {
        private:
            const std::string m_enum_name{};

        public:
            virtual auto get_fully_qualified_type_name() const -> std::string { return std::format("{}{}{}", m_enum_name, is_pointer() ? "*" : "", is_ref() ? "&" : ""); };
            virtual auto generate_cxx_name() const -> std::string override { return m_enum_name; };
            virtual auto generate_lua_stack_validation_condition(int stack_index) const -> std::string override { return std::format("lua_isinteger(lua_state, {})", stack_index); };
            virtual auto generate_lua_stack_retriever(int stack_index) const -> std::string override { return std::format("static_cast<{}>(lua_tointeger(lua_state, {}))", m_enum_name, stack_index); };
            virtual auto generate_lua_stack_pusher(std::string_view variable_to_push, std::string_view param_prefix) const -> std::string override { return std::format("        lua_pushinteger({}lua_state, static_cast<lua_Integer>({}))", param_prefix, variable_to_push); };

        public:
            explicit Enum(const Container& container) : BaseTemplate<Enum>(container) {}
             Enum(const Container& container, std::string enum_name) : BaseTemplate<Enum>(container), m_enum_name(std::move(enum_name)) {}
        };
        //*/

        class Bool : public BaseTemplate<Bool>
        {
        public:
            virtual auto get_fully_qualified_type_name() const -> std::string { return std::format("bool{}{}", is_pointer() ? "*" : "", is_ref() ? "&" : ""); };
            virtual auto generate_cxx_name() const -> std::string override { return "bool"; };
            virtual auto generate_lua_stack_validation_condition(int stack_index) const -> std::string override { return std::format("lua_isboolean(lua_state, {})", stack_index); };
            virtual auto generate_lua_stack_retriever(int stack_index) const -> std::string override { return std::format("lua_toboolean(lua_state, {})", stack_index); };
            virtual auto generate_lua_stack_pusher(std::string_view variable_to_push, std::string_view param_prefix) const -> std::string override { return std::format("        lua_pushboolean({}lua_state, {})", param_prefix, variable_to_push); };

        public:
            explicit Bool(const Container& container) : BaseTemplate<Bool>(container) {}
        };

        class FunctionProto : public BaseTemplate<FunctionProto>
        {
        private:
            std::vector<std::unique_ptr<Base>> m_param_types{};
            Base* m_return_type{};
            const std::string m_function_proto{};
            Function m_function{};
            bool m_has_storage{};

        public:
            virtual auto get_fully_qualified_type_name() const -> std::string override;
            virtual auto generate_cxx_name() const -> std::string override;
            virtual auto generate_lua_stack_validation_condition(int stack_index) const -> std::string override;
            virtual auto generate_lua_stack_retriever(int stack_index) const -> std::string override;
            virtual auto generate_lua_stack_pusher(std::string_view variable_to_push, std::string_view param_prefix) const -> std::string override;
            virtual auto generate_converted_type(size_t param_num, std::vector<std::string>& atomic_resets) const -> std::string override;
            virtual auto needs_conversion_from_lua() const -> bool override { return true; };

        public:
            explicit FunctionProto(const Container& container, const std::string& function_proto) : BaseTemplate<FunctionProto>(container), m_function_proto(function_proto) {}

        public:
            auto add_param(std::unique_ptr<Base> param) -> void { m_param_types.emplace_back(std::move(param)); };
            auto set_return_type(Base* new_return_type) -> void { m_return_type = new_return_type; }

            auto has_storage() const -> bool { return m_has_storage; }
            auto set_has_storage(bool new_has_storage) -> void { m_has_storage = new_has_storage; }

            auto get_function() const -> const Function& { return m_function; }
            auto set_function(Function&& new_function) -> void { m_function = std::move(new_function); }

            auto get_function_proto() const -> const std::string& { return m_function_proto; }

            auto generate_function_signature(bool real_function_pointer) const -> std::string;
            auto generate_function_signature_as_function_name() const -> std::string;
            auto generate_lua_wrapper_function() const -> std::string;
        };

        auto generate_static_class_types(const Container& container) -> void;
    }

    struct FunctionParam
    {
        // TODO: Come up with a good way to store types so that they can be used with ease later to determine what kind of Lua code to generate.
        const std::string name;
        std::unique_ptr<Type::Base> type;
    };

    struct TypePatch
    {
        using TypePatchGenerateStateFilePre = std::string (*)(const Container&);
        using TypePatchCXTypeToTypeCallable = std::unique_ptr<Type::Base> (*)(CodeGenerator&, const CXType&, IsPointer);
        using StringReturnNoParamCallable = std::string (*)();
        using GeneratePerClassStaticFunctionsCallable = std::string (*)(const Class&);
        using CXTypeToTypePostCallable = void (*)(Type::Base*);

        TypePatchGenerateStateFilePre generate_state_file_pre{};
        TypePatchGenerateStateFilePre generate_state_file_post{};
        TypePatchCXTypeToTypeCallable cxtype_to_type{};
        CXTypeToTypePostCallable cxtype_to_type_post{};
        StringReturnNoParamCallable generate_lua_setup_state_function_post{};
        GeneratePerClassStaticFunctionsCallable generate_per_class_static_functions{};
    };

    class CodeGenerator
    {
    private:
        std::filesystem::path m_output_path;
        Container m_container;
        const std::vector<TypePatch>& m_type_patches;

    public:
        CodeGenerator() = delete;
        explicit CodeGenerator(std::filesystem::path output_path, const std::vector<TypePatch>& type_patches) : m_output_path(std::move(output_path)), m_type_patches(type_patches) {}

    public:
        auto add_class(const std::string& class_name, const std::string& full_path_to_file, const std::string& fully_qualified_scope) -> Class&;
        auto add_thin_class(const std::string& class_name, const std::string& full_path_to_file, const std::string& fully_qualified_scope) -> Class&;
        auto add_lua_state_type(const std::string& lua_state_type_name) -> void
        {
            m_container.lua_state_types.emplace(lua_state_type_name);
        }

    public:
        auto add_class_to_container(const std::string& class_name, ClassContainer& container, const std::string& full_path_to_file, const std::string& fully_qualified_scope) -> Class&;

    private:
        auto generate_setup_functions_map() const -> std::string;
        auto generate_lua_dynamic_setup_state_function() const -> std::string;
        auto generate_lua_setup_state_functions() const -> std::string;
        auto generate_free_functions() const -> std::string;
        auto generate_lua_setup_global_free_functions() const -> std::string;
        auto generate_lua_setup_enums() const -> std::string;
        auto generate_convertible_to_set() const -> std::string;
        auto generate_builtin_to_lua_from_heap_functions() const -> std::string;
        auto generate_utility_member_functions() const -> std::string;

    public:
        auto generate_lua_setup_file() const -> void;

        // States/<StateName>/Main.hpp
    public:
        auto generate_state_file_pre() const -> std::string;
        auto generate_state_file() const -> void;

        auto get_type_patches() const -> const std::vector<TypePatch>& { return m_type_patches; };
        auto get_container() const -> const Container& { return m_container; };
        auto get_container() -> Container& { return m_container; };
        auto debug_get_container() const -> const Container& { return m_container; };
    };
}

#endif //LUA_WRAPPER_GENERATOR_TESTING_PARSER_OUTPUT_HPP
