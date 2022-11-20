#ifndef LUA_WRAPPER_GENERATOR_PATCHES_UNREAL_HPP
#define LUA_WRAPPER_GENERATOR_PATCHES_UNREAL_HPP

#include <memory>

#include <LuaWrapperGenerator/CodeParser.hpp>
#include <LuaWrapperGenerator/CodeGenerator.hpp>

namespace RC::LuaWrapperGenerator::TypePatches::Unreal
{
    auto cxtype_to_type_post(Type::Base*) -> void;
    auto cxtype_to_type(CodeGenerator&, const CXType&, IsPointer = IsPointer::No) -> std::unique_ptr<Type::Base>;
    auto generate_state_file_pre(const Container&) -> std::string;
    auto generate_state_file_post(const Container&) -> std::string;
    auto generate_lua_setup_state_function_post() -> std::string;
    auto generate_per_class_static_functions(const Class&) -> std::string;

    class TArray : public Type::BaseTemplate<TArray>
    {
    private:
        static constexpr std::string_view struct_name{"ArrayTest"};
        static constexpr std::string_view fully_qualified_struct_scope{"::RC::UnrealRuntimeTypes"};
        static inline std::string fully_qualified_name{std::format("{}::{}", fully_qualified_struct_scope, struct_name)};
        static constexpr std::string_view wrapped_struct_name{"FScriptArray"};
        static constexpr std::string_view fully_qualified_wrapped_struct_scope{"::RC::Unreal"};
        static inline std::string fully_qualified_wrapped_name{std::format("{}::{}", fully_qualified_wrapped_struct_scope, wrapped_struct_name)};

    private:
        std::unique_ptr<Type::Base> m_element_type{};

    public:
        virtual auto generate_cxx_name() const -> std::string override;
        virtual auto generate_lua_stack_validation_condition(int stack_index) const -> std::string override;
        virtual auto generate_lua_stack_retriever(int stack_index) const -> std::string override;
        virtual auto generate_lua_stack_pusher(std::string_view variable_to_push, std::string_view param_prefix) const -> std::string override;
        virtual auto generate_converted_type(size_t param_num, std::vector<std::string>& atomic_resets) const -> std::string override;
        virtual auto needs_conversion_from_lua() const -> bool override { return true; };

    public:
        //explicit TArray(const Container& container) : BaseTemplate<TArray>(container) {}
        TArray(const Container& container, std::unique_ptr<Type::Base> element_type) : BaseTemplate<TArray>(container), m_element_type(std::move(element_type)) {}
    };
}

#endif //LUA_WRAPPER_GENERATOR_PATCHES_UNREAL_HPP
