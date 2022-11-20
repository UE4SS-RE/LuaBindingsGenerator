#ifndef LUA_WRAPPER_GENERATOR_TESTING_WRAPPER_GENERATOR_HPP
#define LUA_WRAPPER_GENERATOR_TESTING_WRAPPER_GENERATOR_HPP

#include <optional>
#include <filesystem>
#include <unordered_map>
#include <memory>

#include <LuaWrapperGenerator/CodeGenerator.hpp>
#include <File/Macros.hpp>

// This needs a ton of work.
// Right now I'm just doing something very simple.
// The 'find_struct_or_class_by_name' function won't take scopes into consideration.
// It will simply stop after the first class with the requested name.
// This could be gotten around by requiring that the name param contains the full namespace unless the struct is in the global namespace.

namespace RC::LuaWrapperGenerator
{
    enum class GenerateThinClass { Yes, No };
    enum class IsStaticFunction { Yes, No };

    struct CustomMemberFunction
    {
        std::string in_class{};
        std::string function_name{};
        std::string wrapper_scope_and_name{};
        std::string fully_qualified_scope{"::"};
        bool is_static{};
    };

    struct CustomMemberFunctionWithCursor
    {
        CustomMemberFunction data{};
        FunctionContainer function_container{};

        CustomMemberFunctionWithCursor(CustomMemberFunction custom_member_function) : data(custom_member_function) {}
        CustomMemberFunctionWithCursor(CustomMemberFunctionWithCursor&&) = default;
    };

    struct CustomFreeFunction
    {
        std::string lua_state_type{};
        std::string scope{};
        std::string wrapper_name{};
        std::vector<std::string> names{};
    };

    using CustomClass = CustomFreeFunction;

    auto cxtype_to_type(CodeGenerator&, const CXType&, IsPointer = IsPointer::No) -> std::unique_ptr<Type::Base>;

    class CodeParser
    {
    private:
        std::vector<CXTranslationUnit> m_translation_units{};
        CXCursor m_current_cursor{};
        CXIndex m_current_index{};
        // Type Name -> CustomClass
        std::unordered_map<std::string, CustomClass> m_out_of_line_class_requests{};
        // Type Name -> CustomFreeFunction
        std::unordered_map<std::string, CustomFreeFunction> m_out_of_line_free_function_requests{};
        std::unordered_map<std::string, CustomFreeFunction> m_out_of_line_custom_free_function_requests{};
        // Wrapper Name -> CustomMemberFunction
        std::unordered_map<std::string, CustomMemberFunction> m_out_of_line_custom_member_function_requests{};
        std::unordered_set<std::string> m_out_of_line_custom_member_function_names{};
        std::vector<std::pair<std::string, CustomMemberFunctionWithCursor>> m_custom_member_functions_to_generate{};
        std::unordered_map<std::string, CustomMemberFunction> m_out_of_line_custom_metamethod_functions{};
        // Original Templated Class -> Non-templated Class
        std::unordered_map<std::string, std::string> m_out_of_line_template_class_map{};
        std::unordered_map<std::string, std::vector<std::pair<std::string, std::string>>> m_custom_base_classes{};
        std::unordered_map<std::string, std::vector<std::pair<std::string, std::string>>> m_custom_base_classes_inverted{};
        // Fully qualified scope + enum namm -> pair of enum scope and enum name
        std::unordered_map<std::string, std::pair<std::string, std::string>> m_out_of_line_enums{};
        CodeGenerator m_parser_output;
        std::filesystem::path m_code_root;
        std::vector<TypePatch> m_type_patches{};
        std::vector<std::string> m_files_to_parse{};
        const char** m_compiler_flags{};
        int m_num_compiler_flags{};

    public:
        CodeParser(std::vector<std::string> files_to_parse, const char** compiler_flags, int num_compiler_flags, std::filesystem::path output_path, std::filesystem::path code_root);
        ~CodeParser();

    private:
        auto cxtype_to_type(const CXType& cxtype, IsPointer is_pointer = IsPointer::No) -> std::unique_ptr<Type::Base>;
        auto cursor_to_type(const CXCursor& cursor) -> std::unique_ptr<Type::Base>;

    public:
        auto add_type_patch(TypePatch&& type_patch) -> void;

        auto resolve_base(CXCursor& cursor, Class& bases) -> void;
        auto resolve_bases(CXCursor& cursor_in, std::string& buffer, std::vector<const Class*>& bases) -> void;
        auto generate_lua_class(const CXCursor& cursor, GenerateThinClass = GenerateThinClass::No) -> Class*;
        auto generate_lua_class_member_function(Class& the_class, const CXCursor& cursor, FunctionContainer&, IsStaticFunction = IsStaticFunction::No) -> Function*;
        auto generate_lua_function(const CXCursor& cursor, FunctionContainer&, Class* = nullptr, const std::string& function_name_override = "", const std::string& scope_override = "", IsFunctionScopeless = IsFunctionScopeless::No) -> Function*;
        auto generate_lua_free_function(const CXCursor& cursor) -> void;

        auto create_custom_function(const CXCursor& cursor, Class* the_class, const std::string& wrapper_scope_and_name, CustomMemberFunction& custom_member_function, FunctionContainer& function_container, IsFunctionScopeless = IsFunctionScopeless::No) -> Function*;

        auto does_exact_class_exist(const std::string& class_scope_and_name) -> bool;

        auto generate_internal(CXCursor inner_cursor, CXCursor outer_cursor) -> CXChildVisitResult;
        auto static generator_internal_clang_wrapper(CXCursor inner_cursor, CXCursor outer_cursor, CXClientData self) -> CXChildVisitResult;
        auto parse() -> const CodeGenerator&;
    };
}

#endif //LUA_WRAPPER_GENERATOR_TESTING_WRAPPER_GENERATOR_HPP
