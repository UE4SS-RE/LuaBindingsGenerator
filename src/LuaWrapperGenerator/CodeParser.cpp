#include <ranges>
#include <stdexcept>
#include <functional>
#include <ranges>
#include <format>
#include <iostream>
#include <utility>

#include <LuaWrapperGenerator/CodeParser.hpp>
#include <LuaWrapperGenerator/CommentParser.hpp>
#include <Helpers/String.hpp>
#include <Timer/ScopedTimer.hpp>

namespace RC::LuaWrapperGenerator
{
    auto static resolve_scope(const CXCursor& cursor) -> std::string
    {
        std::string buffer{};

        auto parent_cursor = clang_getCursorSemanticParent(cursor);
        auto cursor_kind = clang_getCursorKind(parent_cursor);

        std::vector<std::string> scopes{};
        while (cursor_kind != CXCursorKind::CXCursor_TranslationUnit && cursor_kind != CXCursorKind::CXCursor_FirstInvalid)
        {
            //buffer.append(std::format("::{}", clang_getCString(clang_getCursorSpelling(parent_cursor))));
            auto cursor_spelling = clang_getCursorSpelling(parent_cursor);
            scopes.emplace_back(std::format("::{}", clang_getCString(cursor_spelling)));
            clang_disposeString(cursor_spelling);
            parent_cursor = clang_getCursorSemanticParent(parent_cursor);
            cursor_kind = clang_getCursorKind(parent_cursor);
        }

        for (auto& scope : std::ranges::reverse_view(scopes))
        {
            buffer.append(scope);
        }

        //auto parent_cursor = clang_getCursorSemanticParent(cursor);
        //auto parent_kind = clang_getCursorKind(parent_cursor);
        //auto parent_name = std::string{clang_getCString(clang_getCursorSpelling(parent_cursor))};
        //printf_s("parent_kind #1: %i\n", parent_kind);
        //printf_s("parent_name #1: %s\n", parent_name.c_str());
//
        //auto parent_cursor2 = clang_getCursorSemanticParent(parent_cursor);
        //auto parent_kind2 = clang_getCursorKind(parent_cursor2);
        //auto parent_name2 = std::string{clang_getCString(clang_getCursorSpelling(parent_cursor2))};
        //printf_s("parent_kind #2: %i\n", parent_kind2);
        //printf_s("parent_name #2: %s\n", parent_name2.c_str());
//
        //auto parent_cursor3 = clang_getCursorSemanticParent(parent_cursor2);
        //auto parent_kind3 = clang_getCursorKind(parent_cursor3);
        //auto parent_name3 = std::string{clang_getCString(clang_getCursorSpelling(parent_cursor3))};
        //printf_s("parent_kind #3: %i\n", parent_kind3);
        //printf_s("parent_name #3: %s\n", parent_name3.c_str());

        return buffer;
    }

    auto cxtype_to_type(CodeGenerator& code_generator, const CXType& cxtype, IsPointer is_pointer) -> std::unique_ptr<Type::Base>
    {
        std::unique_ptr<Type::Base> type{};

        auto cxtype_spelling = clang_getTypeSpelling(cxtype);
        std::string type_spelling = clang_getCString(cxtype_spelling);
        clang_disposeString(cxtype_spelling);
        //printf_s("type_spelling: %s, type_kind: %i\n", type_spelling.c_str(), cxtype.kind);
        if (type_spelling.find("std::function") != type_spelling.npos)
        {
            CXType real_cxtype = cxtype;
            if (cxtype.kind == CXType_LValueReference)
            {
                real_cxtype = clang_getPointeeType(cxtype);
            }

            auto named_cxtype = clang_Type_getNamedType(real_cxtype);
            auto function_proto_cxtype = clang_Type_getTemplateArgumentAsType(named_cxtype, 0);
            type = cxtype_to_type(code_generator, function_proto_cxtype, is_pointer);

            if (type)
            {
                static_cast<Type::FunctionProto*>(type.get())->set_has_storage(true);
            }
        }
        else if (type_spelling.find('<') != type_spelling.npos)
        {
            //throw DoNotParseException{"templated types are not supported"};
            return nullptr;
        }
        // We don't support 'using namespace std;' with the C++ standard library.
        else if (type_spelling == "std::string")
        {
            //return Type::String::static_class.get();
            type = std::make_unique<Type::String>(code_generator.get_container());
        }
        else if (type_spelling == "std::wstring")
        {
            //return Type::WString::static_class.get();
            type = std::make_unique<Type::WString>(code_generator.get_container());
        }
        else if (type_spelling == "File::StringType" || type_spelling == "RC::File::StringType")
        {
            //return Type::AutoString::static_class.get();
            // TODO: Resolve the 'using' statement to figure out if this is a wide string.
            //       We're assuming this is a wide string at the moment.
            type = std::make_unique<Type::AutoString>(code_generator.get_container());
        }
        else if (type_spelling == "std::string_view")
        {
            // TODO: Properly support string_view.
            type = std::make_unique<Type::String>(code_generator.get_container());
        }
        else if (type_spelling == "std::wstring_view")
        {
            // TODO: Properly support wstring_view.
            type = std::make_unique<Type::WString>(code_generator.get_container());
        }
        else if (type_spelling == "File::StringViewType")
        {
            // TODO: Properly support File::StringViewType.
            type = std::make_unique<Type::AutoString>(code_generator.get_container());
        }
        else if (cxtype.kind == CXTypeKind::CXType_Void)
        {
            //return Type::Void::static_class.get();
            type = std::make_unique<Type::Void>(code_generator.get_container());
        }
        else if (cxtype.kind == CXTypeKind::CXType_SChar)
        {
            //return Type::Int8::static_class.get();
            type = std::make_unique<Type::Int8>(code_generator.get_container());
        }
        else if (cxtype.kind == CXTypeKind::CXType_Short)
        {
            //return Type::Int16::static_class.get();
            type = std::make_unique<Type::Int16>(code_generator.get_container());
        }
        else if (cxtype.kind == CXTypeKind::CXType_Int)
        {
            //return Type::Int32::static_class.get();
            type = std::make_unique<Type::Int32>(code_generator.get_container());
        }
        else if (cxtype.kind == CXTypeKind::CXType_LongLong)
        {
            //return Type::Int64::static_class.get();
            type = std::make_unique<Type::Int64>(code_generator.get_container());
        }
        else if (cxtype.kind == CXTypeKind::CXType_UChar)
        {
            //return Type::UInt8::static_class.get();
            type = std::make_unique<Type::UInt8>(code_generator.get_container());
        }
        else if (cxtype.kind == CXTypeKind::CXType_UShort)
        {
            //return Type::UInt16::static_class.get();
            type = std::make_unique<Type::UInt16>(code_generator.get_container());
        }
        else if (cxtype.kind == CXTypeKind::CXType_UInt)
        {
            //return Type::UInt32::static_class.get();
            type = std::make_unique<Type::UInt32>(code_generator.get_container());
        }
        else if (cxtype.kind == CXTypeKind::CXType_ULongLong)
        {
            //return Type::UInt64::static_class.get();
            type = std::make_unique<Type::UInt64>(code_generator.get_container());
        }
        else if (cxtype.kind == CXTypeKind::CXType_Float)
        {
            //return Type::Float::static_class.get();
            type = std::make_unique<Type::Float>(code_generator.get_container());
        }
        else if (cxtype.kind == CXTypeKind::CXType_Double)
        {
            //return Type::Double::static_class.get();
            type = std::make_unique<Type::Double>(code_generator.get_container());
        }
        else if (cxtype.kind == CXTypeKind::CXType_Char_S)
        {
            if (is_pointer == IsPointer::Yes)
            {
                //printf_s("Is Pointer == Yes\n");
                type = std::make_unique<Type::CString>(code_generator.get_container());
                type->set_is_pointer(true);
            }
            else
            {
                throw DoNotParseException{"type 'char' is not supported"};
            }
        }
        else if (cxtype.kind == CXTypeKind::CXType_WChar)
        {
            if (is_pointer == IsPointer::Yes)
            {
                type = std::make_unique<Type::CWString>(code_generator.get_container());
                type->set_is_pointer(true);
            }
            else
            {
                throw DoNotParseException{"type 'wchar_t' is not supported"};
            }
        }
        else if (cxtype.kind == CXTypeKind::CXType_Bool)
        {
            type = std::make_unique<Type::Bool>(code_generator.get_container());
        }
            /**/
        else if (cxtype.kind == CXTypeKind::CXType_Enum)
        {
            type = std::make_unique<Type::Enum>(code_generator.get_container(), type_spelling);
        }
            //*/
        else if (cxtype.kind == CXTypeKind::CXType_Pointer)
        {
            //printf_s("is pointer!\n");
            type = cxtype_to_type(code_generator, clang_getPointeeType(cxtype), IsPointer::Yes);
            type->set_is_pointer(true);
        }
        else if (cxtype.kind == CXTypeKind::CXType_LValueReference)
        {
            //printf_s("is reference!\n");
            //printf_s("type: %i\n", clang_getPointeeType(cxtype).kind);
            //printf_s("name: %s\n", clang_getCString(clang_getTypeSpelling(clang_getPointeeType(cxtype))));
            type = cxtype_to_type(code_generator, clang_getPointeeType(cxtype), is_pointer);
            if (type)
            {
                type->set_is_ref(true);
            }
        }
        else if (cxtype.kind == CXTypeKind::CXType_Typedef)
        {
            //return typedef_to_type(cxtype);
            //printf_s("typedef type: %i\n", clang_getCanonicalType(cxtype).kind);
            //printf_s("typedef type spelling: %s\n", clang_getCString(clang_getTypeSpelling(clang_getCanonicalType(cxtype))));
            type = cxtype_to_type(code_generator, clang_getCanonicalType(cxtype), is_pointer);
        }
        else if (cxtype.kind == CXTypeKind::CXType_Elaborated)
        {
            auto a = clang_Type_getNamedType(cxtype);
            //printf_s("named type: %i\n", a.kind);
            type = cxtype_to_type(code_generator, a, is_pointer);
        }
        else if (cxtype.kind == CXTypeKind::CXType_Record)
        {
            // "const std::function<RC::LoopAction (RC::Unreal::UObject *)>"
            auto canonical_type_spelling = clang_getTypeSpelling(clang_getCanonicalType(cxtype));
            std::string canonical_name = clang_getCString(canonical_type_spelling);
            clang_disposeString(canonical_type_spelling);
            auto last_semi_colon = canonical_name.find_last_of(':');
            if (last_semi_colon != canonical_name.npos)
            {
                type = std::make_unique<Type::CustomStruct>(code_generator.get_container(), canonical_name.substr(last_semi_colon + 1));
            }
            else
            {
                type = std::make_unique<Type::CustomStruct>(code_generator.get_container(), canonical_name);
            }

            if (clang_isReference(clang_getCursorKind(clang_getTypeDeclaration(cxtype))))
            {
                type->set_is_ref(is_pointer == IsPointer::No);
            }

            auto* typed_type = static_cast<Type::CustomStruct*>(type.get());

            auto type_decl_cursor = clang_getTypeDeclaration(cxtype);

            auto scope = resolve_scope(type_decl_cursor);
            typed_type->set_fully_qualified_scope(scope);

            if (clang_isCursorDefinition(type_decl_cursor) == 0)
            {
                auto type_decl_cursor_kind = clang_getCursorKind(type_decl_cursor);
                if (type_decl_cursor_kind == CXCursorKind::CXCursor_ClassDecl)
                {
                    typed_type->set_is_class_forward_declaration(true);
                }
                else if (type_decl_cursor_kind == CXCursorKind::CXCursor_StructDecl)
                {
                    typed_type->set_is_struct_forward_declaration(true);
                }
            }
        }
        else if (cxtype.kind == CXTypeKind::CXType_Unexposed)
        {
            //throw DoNotParseFunctionException{std::format("unexposed types are not yet supported, type '{}'", clang_getCString(clang_getTypeSpelling(cxtype)))};
            auto canonical_type_spelling = clang_getTypeSpelling(clang_getCanonicalType(cxtype));
            std::string canonical_name = clang_getCString(canonical_type_spelling);
            clang_disposeString(canonical_type_spelling);
            // canonical_name: std::chrono::time_point<std::chrono::steady_clock, std::chrono::duration<long long, std::ratio<1, 1000000000>>>
            // Figure out how to parse the name above to retrieve the actual struct name, in this case, 'time_point'.
            // Supposedly it's not important to retrieve the scopes 'std::' or 'chrono::' because that's done later. Confirm this.
            auto first_angle_brace = canonical_name.find_first_of('<');
            size_t start_of_type{};
            if (first_angle_brace == canonical_name.npos)
            {
                start_of_type = canonical_name.find_last_of(':');
                if (start_of_type == canonical_name.npos) { start_of_type = 0; }

                if (start_of_type != 0)
                {
                    type = std::make_unique<Type::CustomStruct>(code_generator.get_container(), canonical_name.substr(start_of_type + 1, first_angle_brace - start_of_type - 1));
                }
                else
                {
                    type = std::make_unique<Type::CustomStruct>(code_generator.get_container(), canonical_name);
                }
            }
            else
            {
                //throw DoNotParseException{"templated types are not supported"};
            }
        }
        else if (cxtype.kind == CXTypeKind::CXType_FunctionProto)
        {
            // The following counts as a FunctionProto:
            // An inline declaration, a typedef, or a using statement.

            auto function_proto_type_spelling = clang_getTypeSpelling(cxtype);
            auto function_proto = std::string{clang_getCString(function_proto_type_spelling)};
            clang_disposeString(function_proto_type_spelling);
            type = std::make_unique<Type::FunctionProto>(code_generator.get_container(), function_proto);

            auto typed_type = static_cast<Type::FunctionProto*>(type.get());
            for (int i = 0; i < clang_getNumArgTypes(cxtype); i++)
            {
                typed_type->add_param(cxtype_to_type(code_generator, clang_getArgType(cxtype, i)));
            }

            auto[function_proto_it, was_inserted] = code_generator.get_container().function_proto_container.emplace(typed_type->get_function_proto(), typed_type);

            auto function = Function{
                    typed_type->get_function_proto(),
                    "",
                    "",
                    "",
                    "",
                    nullptr,
                    nullptr};

            auto& overload = function.get_overloads().emplace_back();
            for (int i = 0; i < clang_getNumArgTypes(cxtype); ++i)
            {
                auto arg_cxtype = clang_getArgType(cxtype, i);
                auto arg_type = cxtype_to_type(code_generator, arg_cxtype);
                overload.emplace_back("", std::move(arg_type));
            }

            auto return_type = cxtype_to_type(code_generator, clang_getResultType(cxtype));
            function.set_return_type(std::move(return_type));
            typed_type->set_return_type(function.get_return_type());
            typed_type->set_function(std::move(function));
        }
        else if (cxtype.kind == CXTypeKind::CXType_Auto)
        {
            // TODO: Figure out how to extract the real return type.
            //       This might require entering into the function and finding the return statement.
            throw DoNotParseException{std::format("auto type is not supported in this context")};
        }

        if (type)
        {
            for (const auto& type_patch : code_generator.get_type_patches())
            {
                if (type_patch.cxtype_to_type_post)
                {
                    type_patch.cxtype_to_type_post(type.get());
                }
            }

            if (clang_isConstQualifiedType(cxtype))
            {
                type->set_is_const(true);
            }
        }

        return type;
    }

    auto CodeParser::cxtype_to_type(const CXType& cxtype, IsPointer is_pointer) -> std::unique_ptr<Type::Base>
    {
        auto cxtype_spelling = clang_getTypeSpelling(cxtype);
        std::string type_spelling = clang_getCString(cxtype_spelling);
        clang_disposeString(cxtype_spelling);
        auto type = ::RC::LuaWrapperGenerator::cxtype_to_type(m_parser_output, cxtype, is_pointer);
        if (!type)
        {
            //if (auto it = type_patches.find(); it != type_patches.end())
            for (const auto& type_patch : m_type_patches)
            {
                type = type_patch.cxtype_to_type(m_parser_output, cxtype, is_pointer);
            }

            if (!type)
            {
                auto cxtype_kind_spelling = clang_getTypeKindSpelling(cxtype.kind);
                auto type_kind_spelling = std::string{clang_getCString(cxtype_kind_spelling)};
                clang_disposeString(cxtype_kind_spelling);
                throw DoNotParseException{std::format("type is unhandled, type_spelling: {}", type_kind_spelling)};
            }
        }

        return type;
    }

    auto CodeParser::cursor_to_type(const CXCursor& cursor) -> std::unique_ptr<Type::Base>
    {
        auto cursor_type = clang_getCursorResultType(cursor);
        return cxtype_to_type(cursor_type);
    }

    CodeParser::CodeParser(std::vector<std::string> files_to_parse, const char** compiler_flags, int num_compiler_flags, std::filesystem::path output_path, std::filesystem::path code_root) : m_parser_output(std::move(output_path), m_type_patches), m_code_root(std::move(code_root))
    {
        m_files_to_parse = std::move(files_to_parse);
        m_compiler_flags = compiler_flags;
        m_num_compiler_flags = num_compiler_flags;
        m_current_index = clang_createIndex(0, 0);
    }

    CodeParser::~CodeParser()
    {
        clang_disposeIndex(m_current_index);
    }

    auto CodeParser::generate_lua_function(const CXCursor& cursor, FunctionContainer& function_container, Class* the_class, const std::string& function_name_override, const std::string& scope_override, IsFunctionScopeless is_function_scopeless) -> Function*
    {
        auto cursor_spelling = clang_getCursorSpelling(cursor);
        auto function_name = !function_name_override.empty() ? function_name_override : std::string{clang_getCString(cursor_spelling)};
        clang_disposeString(cursor_spelling);
        auto function_scope = resolve_scope(cursor);
        auto function_scope_and_name = function_scope + "::" + function_name;

        std::vector<std::pair<std::string, std::unique_ptr<Type::Base>>> checked_parameters{};
        if (clang_Cursor_getNumArguments(cursor) > 0)
        {
            struct VisitorData
            {
                CodeParser& self;
                const std::string& full_function_name;
                decltype(checked_parameters)& checked_parameters;
                bool do_not_parse{};
            };
            VisitorData visitor_data{*this, function_scope_and_name, checked_parameters};
            clang_visitChildren(cursor, [](CXCursor inner_cursor, CXCursor outer_cursor, CXClientData visitor_data_raw) -> CXChildVisitResult {
                auto& visitor_data = *static_cast<VisitorData*>(visitor_data_raw);
                auto inner_cursor_spelling = clang_getCursorSpelling(inner_cursor);
                auto param_name = std::string{clang_getCString(inner_cursor_spelling)};
                clang_disposeString(inner_cursor_spelling);
                auto cursor_kind = clang_getCursorKind(inner_cursor);

                if (cursor_kind != CXCursor_ParmDecl)
                {
                    return CXChildVisitResult::CXChildVisit_Continue;
                }
                //else if (cursor_kind != CXCursor_ParmDecl)
                //{
                //    // TODO: Figure out what to do about CXCursor_TypeRef here.
                //    //       Ref: error : Kind for param type 'class RC::Mod' for function 'find_mod_by_name' wasn't 'CXCursor_ParmDecl' while iterating function parameters, it was '43'
                //    throw std::runtime_error{std::format("Kind for param type '{}' for function '{}' wasn't 'CXCursor_ParmDecl' while iterating function parameters, it was '{}'", param_name, visitor_data.full_function_name, std::underlying_type_t<CXCursorKind>(cursor_kind))};
                //}
                auto cursor_type = clang_getCursorType(inner_cursor);
                auto cursor_type_spelling = clang_getTypeSpelling(cursor_type);
                std::string type_spelling{clang_getCString(cursor_type_spelling)};
                clang_disposeString(cursor_type_spelling);
                auto is_def = clang_isCursorDefinition(inner_cursor);
                //auto display_name = clang_getCString(clang_getCursorDisplayName(inner_cursor));
                //printf_s("param_name: %s, kind: %i, cursor_type_kind: %i, type_spelling: %s, is_def: %i\n", param_name.c_str(), cursor_kind, cursor_type.kind, type_spelling.c_str(), is_def);
                //printf_s("display_name: %s\n", display_name);

                try
                {
                    visitor_data.checked_parameters.emplace_back(std::make_pair(std::move(param_name), std::move(visitor_data.self.cxtype_to_type(cursor_type))));
                }
                catch (DoNotParseException& e)
                {
                    printf_s("%s\n", std::format("Skipped function '{}' due to the param '{}' because {}", visitor_data.full_function_name, param_name.empty() ? "<name-less-param>" : param_name , e.reason).c_str());
                    visitor_data.do_not_parse = true;
                    return CXChildVisitResult::CXChildVisit_Break;
                }

                return CXChildVisitResult::CXChildVisit_Continue;
            }, &visitor_data);

            if (visitor_data.do_not_parse) { return nullptr; }
        }

        std::unique_ptr<Type::Base> return_type{};
        try
        {
            return_type = cursor_to_type(cursor);
        }
        catch (DoNotParseException& e)
        {
            printf_s("%s\n", std::format("Skipped function '{}' due to the return type because {}", function_scope_and_name, e.reason).c_str());
            return nullptr;
        }

        Function* function{};
        try
        {
            //function = &the_class.add_function(function_name);
            auto loc = clang_getCursorLocation(cursor);
            CXFile loc_file;
            unsigned loc_line;
            unsigned loc_column;
            unsigned loc_offset;
            clang_getSpellingLocation(loc, &loc_file, &loc_line, &loc_column, &loc_offset);
            auto path_name = clang_File_tryGetRealPathName(loc_file);

            function = &add_function_to_container(function_container,
                                                  function_name,
                                                  the_class ? the_class->name : std::string{},
                                                  clang_getCString(path_name),
                                                  resolve_scope(cursor),
                                                  the_class ? the_class->fully_qualified_scope : std::string{},
                                                  the_class,
                                                  is_function_scopeless);
            clang_disposeString(path_name);

            if (function && !scope_override.empty())
            {
                function->set_scope_override(scope_override);
            }
        }
        catch (DoNotParseException& e)
        {
            if (e.is_verbose)
            {
                printf_s("%s\n", std::format("Skipped function '{}' because {}", function_scope_and_name, e.reason).c_str());
            }
            return nullptr;
        }

        int num_overload_param_matches{};
        for (size_t i = 0; i < checked_parameters.size(); i++)
        {
            auto& [checked_param_name, checked_param_type] = checked_parameters[i];

            auto& overloads = function->get_overloads();
            for (size_t x = 0; x < overloads.size(); x++)
            {
                auto& overload_params = overloads[x];
                if (overload_params.size() == checked_parameters.size())
                {
                    auto& param = overload_params[i];
                    if (param.type->get_fully_qualified_type_name() == checked_param_type->get_fully_qualified_type_name())
                    {
                        ++num_overload_param_matches;
                        break;
                    }

                    // Hack for hard-coded strings.
                    if ((param.type->is_a<Type::AutoString>() || param.type->is_a<Type::WString>() || param.type->is_a<Type::String>() || param.type->is_a<Type::CWString>() || param.type->is_a<Type::CString>())
                        &&
                        (checked_param_type->is_a<Type::AutoString>() || checked_param_type->is_a<Type::WString>() || checked_param_type->is_a<Type::String>() || checked_param_type->is_a<Type::CWString>() || checked_param_type->is_a<Type::CString>()))
                    {
                        ++num_overload_param_matches;
                        break;
                    }
                }
            }
        }

        auto loc = clang_getCursorLocation(cursor);
        CXFile loc_file;
        unsigned loc_line;
        unsigned loc_column;
        unsigned loc_offset;
        clang_getSpellingLocation(loc, &loc_file, &loc_line, &loc_column, &loc_offset);
        auto file_name = clang_getFileName(loc_file);

        if ((num_overload_param_matches != 0 && num_overload_param_matches == checked_parameters.size()) || (checked_parameters.empty() && !function->get_overloads().empty()))
        {
            //printf_s("Overload already exists function: %s::%s\n", function->get_fully_qualified_scope().data(), function->get_name().data());
            //printf_s("L%i in %s\n", loc_line, clang_getCString(file_name));
            clang_disposeString(file_name);
            return nullptr;
        }
        else
        {
            //printf_s("Adding overload for function: %s::%s\n", function->get_fully_qualified_scope().data(), function->get_name().data());
            //printf_s("L%i in %s\n", loc_line, clang_getCString(file_name));
        }
        clang_disposeString(file_name);

        auto& params_overload = function->get_overloads().emplace_back();
        for (auto& [checked_param_name, checked_param_type] : checked_parameters)
        {
            params_overload.emplace_back(FunctionParam{std::move(checked_param_name), std::move(checked_param_type)});
        }

        function->set_return_type(std::move(return_type));

        return function;
    }

    auto CodeParser::generate_lua_free_function(const CXCursor& cursor) -> void
    {
        auto cursor_spelling = clang_getCursorSpelling(cursor);
        auto name = std::string{clang_getCString(cursor_spelling)};
        clang_disposeString(cursor_spelling);
        auto scope = resolve_scope(cursor);

        // TODO: Support for requesting bindings to be generated with an inline comment just like classes.

        bool was_requested_out_of_line{};
        std::string* lua_state_type{};
        std::string* scope_override{};
        std::string* wrapper_name{};
        std::vector<std::string>* override_names{};
        if (auto it = m_out_of_line_free_function_requests.find(scope + "::" + name); it != m_out_of_line_free_function_requests.end())
        {
            lua_state_type = &it->second.lua_state_type;
            scope_override = &it->second.scope;
            override_names = &it->second.names;
            was_requested_out_of_line = true;
        }
        else if (auto it = m_out_of_line_custom_free_function_requests.find(scope + "::" + name); it != m_out_of_line_custom_free_function_requests.end())
        {
            lua_state_type = &it->second.lua_state_type;
            scope_override = &it->second.scope;
            wrapper_name = &it->second.wrapper_name;
            override_names = &it->second.names;
            was_requested_out_of_line = true;
        }
        else
        {
            // TODO: Inline free functions.
            return;
        }

        Function* function{};
        if (was_requested_out_of_line)
        {
            const auto& original_override_name = override_names->front();
            for (size_t i = 0; i < override_names->size(); ++i)
            {
                const auto& override_name = (*override_names)[i];
                function = generate_lua_function(cursor, m_parser_output.get_container().functions, nullptr, override_name, *scope_override);
                if (function)
                {
                    function->set_name(original_override_name);
                    if (wrapper_name) { function->set_wrapper_name(*wrapper_name); }
                    if (i > 0) { function->set_is_alias(true); }
                }
            }
        }
    }

    auto CodeParser::create_custom_function(const CXCursor& cursor, Class* the_class, const std::string& wrapper_scope_and_name, CustomMemberFunction& custom_member_function, FunctionContainer& function_container, IsFunctionScopeless is_function_scopeless) -> Function*
    {
        //if (!the_class) { return nullptr; }

        //auto function = generate_lua_function(cursor, function_container, the_class, custom_member_function.function_name, {}, is_function_scopeless);
        auto function = generate_lua_function(cursor, function_container, the_class, custom_member_function.function_name, {}, is_function_scopeless);
        if (!function) { return nullptr; }

        function->set_wrapper_name(wrapper_scope_and_name);
        function->set_is_custom_redirector(true);
        function->set_shares_file_with_containing_class(false);

        return function;
    };

    auto CodeParser::does_exact_class_exist(const std::string& class_scope_and_name) -> bool
    {
        // TODO: This doesn't take into account structs in structs.
        //       For those cases, this will always return false which isn't accurate.
        //       Is it even needed to fix this ?
        //       It's a compiler error if you have two structs with the same name in the same scope so the results of the generator doesn't matter.
        const auto& global_container = m_parser_output.get_container().classes;
        auto it = global_container.find(class_scope_and_name);
        if (it == global_container.end())
        {
            return false;
        }
        else
        {
            return true;
        }
    }

    auto CodeParser::add_type_patch(TypePatch&& type_patch) -> void
    {
        m_type_patches.emplace_back(type_patch);
    }

    auto CodeParser::resolve_base(CXCursor& inner_cursor, Class& class_ref) -> void
    {
        //printf_s("Resolving bases for %s...\n", visitor_data.class_ref.name.c_str());
        std::string all_base_names{"Bases: "};

        //resolve_base(visitor_data.this_ref, inner_cursor, all_base_names, visitor_data.class_ref.get_mutable_bases());

        //visitor_data.this_ref.resolve_bases(inner_cursor, all_base_names, visitor_data.class_ref.bases);
        //printf_s("%s %s\n", visitor_data.class_ref.name.c_str(), all_base_names.c_str());
        //printf_s("Bases resolved.\n");
        //for (const auto& a : visitor_data.class_ref.bases)
        //{
        //    printf_s("%s\n", a->name.c_str());
        //}

        struct VisitorData
        {
            CodeParser& this_ref;
            CXCursor true_base_cursor{};
        };
        VisitorData visitor_data_inner{*this};

        clang_visitChildren(inner_cursor, [](CXCursor inner_cursor, CXCursor outer_cursor, CXClientData visitor_data_raw) -> CXChildVisitResult {
            auto& visitor_data = *static_cast<VisitorData*>(visitor_data_raw);
            auto ref_cursor = clang_getCursorReferenced(inner_cursor);
            visitor_data.true_base_cursor = ref_cursor;

            auto ref_cursor_kind = clang_getCursorKind(inner_cursor);
            if (ref_cursor_kind == CXCursor_TemplateRef)
            {
                return CXChildVisit_Break;
            }
            else
            {
                return CXChildVisit_Continue;
            }
        }, &visitor_data_inner);
        auto base_scope = resolve_scope(visitor_data_inner.true_base_cursor);
        auto base_cursor_spelling = clang_getCursorSpelling(visitor_data_inner.true_base_cursor);
        auto base_name = std::string{clang_getCString(base_cursor_spelling)};
        clang_disposeString(base_cursor_spelling);
        auto the_base = m_parser_output.get_container().find_class_by_name(base_scope, base_name);
        if (!the_base)
        {
            the_base = generate_lua_class(visitor_data_inner.true_base_cursor);
        }
        class_ref.get_mutable_bases().emplace(the_base);
    }

    auto static resolve_base(CodeParser& this_ref, CXCursor cursor, std::string& buffer, std::vector<const Class*>& bases) -> void
    {
        struct VisitorData
        {
            CodeParser& this_ref;
            CXCursor true_base_cursor{};
        };
        VisitorData visitor_data{this_ref};

        clang_visitChildren(cursor, [](CXCursor inner_cursor, CXCursor outer_cursor, CXClientData visitor_data_raw) -> CXChildVisitResult {
            auto& visitor_data = *static_cast<VisitorData*>(visitor_data_raw);
            auto ref_cursor = clang_getCursorReferenced(inner_cursor);
            visitor_data.true_base_cursor = ref_cursor;

            auto ref_cursor_kind = clang_getCursorKind(inner_cursor);
            if (ref_cursor_kind == CXCursor_TemplateRef)
            {
                return CXChildVisit_Break;
            }
            else
            {
                return CXChildVisit_Continue;
            }
        }, &visitor_data);

        visitor_data.this_ref.resolve_bases(visitor_data.true_base_cursor, buffer, bases);
    }

    auto CodeParser::resolve_bases(CXCursor& cursor_in, std::string& buffer, std::vector<const Class*>& bases) -> void
    {
        /*
        auto cursor_spelling = clang_getCursorSpelling(cursor);
        auto cursor_name = std::string{clang_getCString(cursor_spelling)};
        clang_disposeString(cursor_spelling);
        if (cursor_name == "TFObjectPropertyBase")
        {
            printf_s("");
        }
        auto fully_qualified_scope = resolve_scope(cursor);
        auto* the_class = m_parser_output.get_container().find_mutable_class_by_name(fully_qualified_scope, cursor_name);
        if (!the_class)
        {
            the_class = generate_lua_class(cursor);
        }
        bases.emplace_back(the_class);

        struct VisitorData
        {
            CodeParser& this_ref;
            std::string& buffer;
            std::vector<const Class*>& bases;
            CXCursor resolved_cursor{};
        };
        VisitorData visitor_data{*this, buffer, bases};

        printf_s("    Resolving base for %s...\n", cursor_name.c_str());

        clang_visitChildren(cursor, [](CXCursor inner_cursor, CXCursor outer_cursor, CXClientData visitor_data_raw) -> CXChildVisitResult {
            printf_s("    inner_cursor: %i, %s, %s\n", clang_getCursorKind(inner_cursor), clang_getCString(clang_getCursorKindSpelling(clang_getCursorKind(inner_cursor))), clang_getCString(clang_getCursorSpelling(inner_cursor)));

            auto visitor_data = static_cast<VisitorData*>(visitor_data_raw);

            auto cursor_kind = clang_getCursorKind(inner_cursor);
            //if (cursor_kind == CXCursor_TemplateTypeParameter)
            //{
            //    printf_s("        TemplateTypeParameter type: %i, %s, %s\n", clang_getCursorType(inner_cursor).kind, clang_getCString(clang_getTypeKindSpelling(clang_getCursorType(inner_cursor).kind)), clang_getCString(clang_getTypeSpelling(clang_getCursorType(inner_cursor))));
            //    printf_s("        TemplateTypeParameter canonical type: %i, %s, %s\n", clang_getCanonicalType(clang_getCursorType(inner_cursor)).kind, clang_getCString(clang_getTypeKindSpelling(clang_getCanonicalType(clang_getCursorType(inner_cursor)).kind)), clang_getCString(clang_getTypeSpelling(clang_getCanonicalType(clang_getCursorType(inner_cursor)))));
            //    auto type_cursor = clang_getTypeDeclaration(clang_getCursorType(inner_cursor));
            //    auto canonical_type_cursor = clang_getTypeDeclaration(clang_getCanonicalType(clang_getCursorType(inner_cursor)));
            //    printf_s("        TemplateTypeParameter cursor: %i, %s, %s\n", type_cursor.kind, clang_getCString(clang_getCursorKindSpelling(type_cursor.kind)), clang_getCString(clang_getCursorSpelling(type_cursor)));
            //    printf_s("        TemplateTypeParameter cursor: %i, %s, %s\n", canonical_type_cursor.kind, clang_getCString(clang_getCursorKindSpelling(canonical_type_cursor.kind)), clang_getCString(clang_getCursorSpelling(canonical_type_cursor)));
            //    printf_s("        Canonical cursor: %i, %s, %s\n", clang_getCanonicalCursor(inner_cursor).kind, clang_getCString(clang_getCursorKindSpelling(clang_getCanonicalCursor(inner_cursor).kind)), clang_getCString(clang_getCursorSpelling(clang_getCanonicalCursor(inner_cursor))));
            //}
            if (cursor_kind == CXCursor_CXXBaseSpecifier)
            {
                struct VisitorData
                {
                    CXCursor true_base_cursor{};
                };
                VisitorData visitor_data_inner{};

                visitor_data->resolved_cursor = inner_cursor;

                clang_visitChildren(inner_cursor, [](CXCursor inner_cursor, CXCursor outer_cursor, CXClientData visitor_data_raw) -> CXChildVisitResult {
                    printf_s("        TMPLT_OR_REF inner_cursor: %i, %s, %s\n", clang_getCursorKind(inner_cursor), clang_getCString(clang_getCursorKindSpelling(clang_getCursorKind(inner_cursor))), clang_getCString(clang_getCursorSpelling(inner_cursor)));

                    auto ref_cursor = clang_getCursorReferenced(inner_cursor);
                    printf_s("            TMPLT_OR_REF ref_cursor: %i, %s, %s\n", clang_getCursorKind(ref_cursor), clang_getCString(clang_getCursorKindSpelling(clang_getCursorKind(ref_cursor))), clang_getCString(clang_getCursorSpelling(ref_cursor)));

                    printf_s("            TemplateTypeParameter type: %i, %s, %s\n", clang_getCursorType(ref_cursor).kind, clang_getCString(clang_getTypeKindSpelling(clang_getCursorType(ref_cursor).kind)), clang_getCString(clang_getTypeSpelling(clang_getCursorType(ref_cursor))));
                    printf_s("            TemplateTypeParameter canonical type: %i, %s, %s\n", clang_getCanonicalType(clang_getCursorType(ref_cursor)).kind, clang_getCString(clang_getTypeKindSpelling(clang_getCanonicalType(clang_getCursorType(ref_cursor)).kind)), clang_getCString(clang_getTypeSpelling(clang_getCanonicalType(clang_getCursorType(ref_cursor)))));
                    auto type_cursor = clang_getTypeDeclaration(clang_getCursorType(ref_cursor));
                    auto canonical_type_cursor = clang_getTypeDeclaration(clang_getCanonicalType(clang_getCursorType(ref_cursor)));
                    printf_s("            TemplateTypeParameter cursor: %i, %s, %s\n", type_cursor.kind, clang_getCString(clang_getCursorKindSpelling(type_cursor.kind)), clang_getCString(clang_getCursorSpelling(type_cursor)));
                    printf_s("            TemplateTypeParameter cursor: %i, %s, %s\n", canonical_type_cursor.kind, clang_getCString(clang_getCursorKindSpelling(canonical_type_cursor.kind)), clang_getCString(clang_getCursorSpelling(canonical_type_cursor)));
                    printf_s("            Canonical cursor: %i, %s, %s\n", clang_getCanonicalCursor(ref_cursor).kind, clang_getCString(clang_getCursorKindSpelling(clang_getCanonicalCursor(ref_cursor).kind)), clang_getCString(clang_getCursorSpelling(clang_getCanonicalCursor(ref_cursor))));

                    return CXChildVisit_Continue;
                }, nullptr);
                clang_visitChildren(inner_cursor, [](CXCursor inner_cursor, CXCursor outer_cursor, CXClientData visitor_data_raw) -> CXChildVisitResult {
                    printf_s("        inner_cursor: %i, %s, %s\n", clang_getCursorKind(inner_cursor), clang_getCString(clang_getCursorKindSpelling(clang_getCursorKind(inner_cursor))), clang_getCString(clang_getCursorSpelling(inner_cursor)));

                    auto visitor_data = static_cast<VisitorData*>(visitor_data_raw);
                    auto ref_cursor = clang_getCursorReferenced(inner_cursor);
                    visitor_data->true_base_cursor = ref_cursor;

                    auto cursor_kind = clang_getCursorKind(inner_cursor);
                    if (cursor_kind == CXCursor_TemplateRef)
                    {
                        //printf_s("        template ref_cursor: %i, %s, %s\n", clang_getCursorKind(ref_cursor), clang_getCString(clang_getCursorKindSpelling(clang_getCursorKind(ref_cursor))), clang_getCString(clang_getCursorSpelling(ref_cursor)));

                        return CXChildVisit_Break;
                    }
                    else
                    {
                        //printf_s("        type ref_cursor: %i, %s, %s\n", clang_getCursorKind(ref_cursor), clang_getCString(clang_getCursorKindSpelling(clang_getCursorKind(ref_cursor))), clang_getCString(clang_getCursorSpelling(ref_cursor)));
                        return CXChildVisit_Continue;
                    }
                }, &visitor_data_inner);

                clang_visitChildren(visitor_data_inner.true_base_cursor, [](CXCursor inner_cursor, CXCursor outer_cursor, CXClientData visitor_data_raw) -> CXChildVisitResult {
                    printf_s("            inner_cursor: %i, %s, %s\n", clang_getCursorKind(inner_cursor), clang_getCString(clang_getCursorKindSpelling(clang_getCursorKind(inner_cursor))), clang_getCString(clang_getCursorSpelling(inner_cursor)));

                    if (inner_cursor.kind == CXCursor_CXXBaseSpecifier)
                    {
                        //printf_s("            ?: %s\n");
                    }
                    return CXChildVisit_Continue;
                }, nullptr);

                visitor_data->this_ref.resolve_bases(visitor_data_inner.true_base_cursor, visitor_data->buffer, visitor_data->bases);

                return CXChildVisit_Break;
            }
            else
            {
                return CXChildVisit_Continue;
            }
        }, &visitor_data);

        printf_s("    Resolved %s\n", cursor_name.c_str());
        //*/

            /*
            //auto cursor = clang_getCursorDefinition(cursor_in);
            auto cursor = cursor_in;
            auto cursor_spelling = clang_getCursorSpelling(cursor);
            auto cursor_name = std::string{clang_getCString(cursor_spelling)};
            clang_disposeString(cursor_spelling);
            printf_s("Resolving: %s\n", cursor_name.c_str());
            //if (cursor_name == "TFObjectPropertyBase<class RC::Unreal::UObject *>")
            if (cursor_name == "TInPropertyBaseClass")
            {
                printf_s("");
                auto cursor1 = clang_getCursorDefinition(cursor_in);
                auto cursor2 = clang_getCursorDefinition(cursor1);
                auto cursor_kind = clang_getCursorKind(cursor_in);
                auto inner_cursor_spelling = clang_getCursorSpelling(cursor_in);
                auto cursor_kind_spelling = clang_getCursorKindSpelling(cursor_kind);
                printf_s("RESOLVE4 (type: %i, type: %s, inner_cursor: %s)\n", (int)cursor_kind, clang_getCString(cursor_kind_spelling), clang_getCString(inner_cursor_spelling));
                clang_visitChildren(cursor_in, [](CXCursor inner_cursor, CXCursor outer_cursor, CXClientData visitor_data_raw) -> CXChildVisitResult {
                    //auto cursor1 = clang_getCursorDefinition(inner_cursor);
                    //auto cursor2 = clang_getCursorDefinition(cursor1);
                    //auto cursor_kind = clang_getCursorKind(cursor1);
                    //auto inner_cursor_spelling = clang_getCursorSpelling(cursor1);
                    //auto cursor_kind_spelling = clang_getCursorKindSpelling(cursor_kind);
                    //printf_s("RESOLVE4 (type: %i, type: %s, inner_cursor: %s)\n", (int)cursor_kind, clang_getCString(cursor_kind_spelling), clang_getCString(inner_cursor_spelling));
                    return CXChildVisit_Continue;
                }, nullptr);
            }
            auto fully_qualified_scope = resolve_scope(cursor);
            //printf_s("Resolving: %s\n", clang_getCString(clang_getCursorSpelling(cursor)));
            buffer.append(std::format("{}, ", cursor_name));

            auto* the_class = m_parser_output.get_container().find_class_by_name(fully_qualified_scope, cursor_name);
            if (the_class)
            {
                bases.emplace_back(the_class);
            }
            else
            {
                the_class = generate_lua_class(cursor);
                bases.emplace_back(the_class);
            }

            struct VisitorData
            {
                CodeParser& this_ref;
                std::vector<const Class*>& bases;
                std::string& buffer;
                const Class& class_ref;
            };
            VisitorData visitor_data{*this, bases, buffer, *the_class};
            clang_visitChildren(cursor_in, [](CXCursor inner_cursor, CXCursor outer_cursor, CXClientData visitor_data_raw) -> CXChildVisitResult {
                auto& visitor_data = *static_cast<VisitorData*>(visitor_data_raw);
                auto cursor_kind = clang_getCursorKind(inner_cursor);

                auto inner_cursor_spelling = clang_getCursorSpelling(inner_cursor);
                auto cursor_kind_spelling = clang_getCursorKindSpelling(cursor_kind);
                printf_s("CHILD (type: %i, type: %s, inner_cursor: %s)\n", (int)cursor_kind, clang_getCString(cursor_kind_spelling), clang_getCString(inner_cursor_spelling));
                clang_disposeString(inner_cursor_spelling);
                clang_disposeString(cursor_kind_spelling);

                if (cursor_kind == CXCursor_TemplateRef)
                {
                    printf_s("TEMPLATEREF\n");

                    clang_visitChildren(clang_getCursorDefinition(inner_cursor), [](CXCursor inner_cursor, CXCursor outer_cursor, CXClientData visitor_data_raw) -> CXChildVisitResult {
                        auto cursor_kind = clang_getCursorKind(inner_cursor);

                        if (cursor_kind == CXCursor_CXXBaseSpecifier)
                        {
                            auto& visitor_data = *static_cast<VisitorData*>(visitor_data_raw);
                            auto ref_cursor = inner_cursor;
                            auto inner_cursor_spelling = clang_getCursorSpelling(ref_cursor);
                            auto ref_cursor_kind = clang_getCursorKind(ref_cursor);
                            auto cursor_kind_spelling = clang_getCursorKindSpelling(ref_cursor_kind);
                            printf_s("RESOLVE2 (type: %i, type: %s, inner_cursor: %s)\n", (int)ref_cursor_kind, clang_getCString(cursor_kind_spelling), clang_getCString(inner_cursor_spelling));
                            clang_disposeString(inner_cursor_spelling);
                            clang_disposeString(cursor_kind_spelling);

                            //auto temp_inner_cursor_spelling = clang_getCursorSpelling(inner_cursor);
                            //if (std::string{clang_getCString(temp_inner_cursor_spelling)} == "TProperty<InTCppType, class RC::Unreal::FObjectPropertyBase>")
                            {
                                clang_visitChildren(ref_cursor, [](CXCursor inner_cursor, CXCursor outer_cursor, CXClientData visitor_data_raw) -> CXChildVisitResult {
                                    auto cursor_kind = clang_getCursorKind(inner_cursor);
                                    auto inner_cursor_spelling = clang_getCursorSpelling(inner_cursor);
                                    auto cursor_kind_spelling = clang_getCursorKindSpelling(cursor_kind);
                                    printf_s("RESOLVE7 (type: %i, type: %s, inner_cursor: %s)\n", (int)cursor_kind, clang_getCString(cursor_kind_spelling), clang_getCString(inner_cursor_spelling));

                                    if (cursor_kind == CXCursor_TypeRef)
                                    {
                                        auto ref_cursor2 = clang_getCursorReferenced(inner_cursor);
                                        auto cursor_kind2 = clang_getCursorKind(ref_cursor2);
                                        auto inner_cursor_spelling2 = clang_getCursorSpelling(ref_cursor2);
                                        auto cursor_kind_spelling2 = clang_getCursorKindSpelling(cursor_kind2);
                                        printf_s("RESOLVE8 (type: %i, type: %s, inner_cursor: %s)\n", (int)cursor_kind2, clang_getCString(cursor_kind_spelling2), clang_getCString(inner_cursor_spelling2));

                                        auto ref_type = clang_getCursorType(ref_cursor2);
                                        auto canonical_type = clang_getCanonicalType(ref_type);
                                        auto canonical_type2 = clang_getCanonicalType(canonical_type);
                                        auto type_decl = clang_getTypeDeclaration(ref_type);
                                        auto type_decl2 = clang_getTypeDeclaration(canonical_type);
                                        printf_s("RESOLVE11 (type_kind: %i, type_kind_spelling: %s, type_spelling: %s)\n", ref_type.kind, clang_getCString(clang_getTypeKindSpelling(ref_type.kind)), clang_getCString(clang_getTypeSpelling(ref_type)));
                                        printf_s("RESOLVE12 (canonical_type_kind: %i, canonical_type_kind_spelling: %s, canonical_type_spelling: %s)\n", canonical_type.kind, clang_getCString(clang_getTypeKindSpelling(canonical_type.kind)), clang_getCString(clang_getTypeSpelling(canonical_type)));
                                        printf_s("RESOLVE13 (canonical_type_kind2: %i, canonical_type_kind_spelling2: %s, canonical_type_spelling2: %s)\n", canonical_type2.kind, clang_getCString(clang_getTypeKindSpelling(canonical_type2.kind)), clang_getCString(clang_getTypeSpelling(canonical_type2)));
                                        printf_s("RESOLVE14 (type_decl: %s, type_decl2: %s)\n", clang_getCString(clang_getCursorSpelling(type_decl)), clang_getCString(clang_getCursorSpelling(type_decl2)));

                                        auto template_spec_cursor = clang_getSpecializedCursorTemplate(outer_cursor);
                                        auto template_spec_cursor_kind = clang_getCursorKind(template_spec_cursor);
                                        auto template_spec_cursor_spelling = clang_getCursorSpelling(template_spec_cursor);
                                        auto template_spec_cursor_kind_spelling = clang_getCursorKindSpelling(template_spec_cursor_kind);
                                        auto a = clang_getTemplateCursorKind(ref_cursor2);
                                        printf_s("RESOLVE15 (type: %i, type: %s, inner_cursor: %s, a: %i, a: %s)\n", (int)template_spec_cursor_kind, clang_getCString(template_spec_cursor_kind_spelling), clang_getCString(template_spec_cursor_spelling), (int)a, clang_getCString(clang_getCursorKindSpelling(a)));
                                    }
                                    return CXChildVisit_Continue;
                                }, visitor_data_raw);
                            }

                            visitor_data.this_ref.resolve_bases(inner_cursor, visitor_data.buffer, visitor_data.bases);
                        }
                        return CXChildVisit_Continue;
                    }, visitor_data_raw);
                }
                else if (cursor_kind == CXCursor_TypeRef)
                {
                    auto inner_cursor_spelling = clang_getCursorSpelling(inner_cursor);
                    auto cursor_kind_spelling = clang_getCursorKindSpelling(cursor_kind);
                    printf_s("TYPEREF (type: %i, type: %s, inner_cursor: %s)\n", (int)cursor_kind, clang_getCString(cursor_kind_spelling), clang_getCString(inner_cursor_spelling));
                    clang_disposeString(inner_cursor_spelling);
                    clang_disposeString(cursor_kind_spelling);

                        auto ref_cursor = clang_getCursorReferenced(inner_cursor);
                        auto inner_cursor_spelling2 = clang_getCursorSpelling(ref_cursor);
                        auto ref_cursor_kind2 = clang_getCursorKind(ref_cursor);
                        auto cursor_kind_spelling2 = clang_getCursorKindSpelling(ref_cursor_kind2);
                        printf_s("RESOLVE10 (type: %i, type: %s, inner_cursor: %s)\n", (int)ref_cursor_kind2, clang_getCString(cursor_kind_spelling2), clang_getCString(inner_cursor_spelling2));
                        clang_disposeString(inner_cursor_spelling2);
                        clang_disposeString(cursor_kind_spelling2);

                    //clang_visitChildren(clang_getCursorDefinition(inner_cursor), [](CXCursor inner_cursor, CXCursor outer_cursor, CXClientData visitor_data_raw) -> CXChildVisitResult {
                    //    auto cursor_kind = clang_getCursorKind(inner_cursor);
    //
                    //    auto ref_cursor = inner_cursor;
                    //    auto inner_cursor_spelling = clang_getCursorSpelling(ref_cursor);
                    //    auto ref_cursor_kind = clang_getCursorKind(ref_cursor);
                    //    auto cursor_kind_spelling = clang_getCursorKindSpelling(ref_cursor_kind);
                    //    printf_s("RESOLVE9 (type: %i, type: %s, inner_cursor: %s)\n", (int)ref_cursor_kind, clang_getCString(cursor_kind_spelling), clang_getCString(inner_cursor_spelling));
                    //    clang_disposeString(inner_cursor_spelling);
                    //    clang_disposeString(cursor_kind_spelling);
    //
                    //    return CXChildVisit_Continue;
                    //}, visitor_data_raw);
                }
                return CXChildVisit_Continue;
            }, &visitor_data);

            printf_s("%s resolved.\n", cursor_name.c_str());
            //*/

        //auto cursor = clang_getCursorDefinition(cursor_in);
        auto cursor = cursor_in;
        auto cursor_spelling = clang_getCursorSpelling(cursor);
        auto cursor_name = std::string{clang_getCString(cursor_spelling)};
        clang_disposeString(cursor_spelling);
        auto fully_qualified_scope = resolve_scope(cursor);
        printf_s("Resolving: %s\n", clang_getCString(clang_getCursorSpelling(cursor)));
        buffer.append(std::format("{}, ", cursor_name));

        auto* the_class = m_parser_output.get_container().find_class_by_name(fully_qualified_scope, cursor_name);
        if (the_class)
        {
            bases.emplace_back(the_class);
        }
        else
        {
            bases.emplace_back(generate_lua_class(cursor));
        }

        struct VisitorData
        {
            CodeParser& this_ref;
            std::vector<const Class*>& bases;
            std::string& buffer;
            CXCursor true_base_cursor{};
        };
        VisitorData visitor_data{*this, bases, buffer};
        clang_visitChildren(cursor, [](CXCursor inner_cursor, CXCursor outer_cursor, CXClientData visitor_data_raw) -> CXChildVisitResult {
            auto& visitor_data = *static_cast<VisitorData*>(visitor_data_raw);
            auto cursor_kind = clang_getCursorKind(inner_cursor);
            printf_s("TemplateRef: %s, %s\n", clang_getCString(clang_getCursorKindSpelling(inner_cursor.kind)), clang_getCString(clang_getCursorSpelling(inner_cursor)));
            if (cursor_kind == CXCursor_CXXBaseSpecifier)
            {
                LuaWrapperGenerator::resolve_base(visitor_data.this_ref, inner_cursor, visitor_data.buffer, visitor_data.bases);
            }
            return CXChildVisitResult::CXChildVisit_Continue;
        }, &visitor_data);
    }

    auto CodeParser::generate_lua_class(const CXCursor& cursor, GenerateThinClass generate_thin_class) -> Class*
    {
        //printf_s("getting canonical cursor\n");
        //auto cursor = clang_getCanonicalCursor(cursor_);
        auto cursor_spelling = clang_getCursorSpelling(cursor);
        auto class_name = std::string{clang_getCString(cursor_spelling)};
        clang_disposeString(cursor_spelling);
        auto class_scope = resolve_scope(cursor);
        auto class_scope_and_name = class_scope + "::" + class_name;

        if (does_exact_class_exist(class_scope_and_name)) { return nullptr; }

        auto comment_raw = clang_Cursor_getBriefCommentText(cursor);
        auto comment = std::string{comment_raw.data ? clang_getCString(comment_raw) : ""};
        clang_disposeString(comment_raw);

        bool was_requested_out_of_line{};
        std::string lua_state_type{};
        std::string scope_override{};
        if (auto it = m_out_of_line_class_requests.find(class_scope_and_name); it != m_out_of_line_class_requests.end())
        {
            lua_state_type = it->second.lua_state_type;
            scope_override = it->second.scope;
            was_requested_out_of_line = true;
        }

        CommentParser comment_parser{comment};

        Class* the_class{};

        auto loc = clang_getCursorLocation(cursor);
        CXFile loc_file;
        unsigned loc_line;
        unsigned loc_column;
        unsigned loc_offset;
        clang_getSpellingLocation(loc, &loc_file, &loc_line, &loc_column, &loc_offset);

        if (auto attribute = comment_parser.get_attribute("Lua"); attribute.exists() || was_requested_out_of_line || generate_thin_class == GenerateThinClass::Yes)
        {
            // Expose this struct to Lua.
            if (!was_requested_out_of_line && generate_thin_class == GenerateThinClass::No)
            {
                lua_state_type = attribute.get_param(0);
                //printf_s("Bindings requested inline for class      '%s' in lua state '%s'\n", class_scope_and_name.c_str(), lua_state_type.c_str());
            }

            try
            {
                // We're using absolute paths here for include statements which isn't ideal.
                // As long as we never ever commit the generated files to the repo then it should be fine because every person will have the paths be specific to their setup.
                auto path_name = clang_File_tryGetRealPathName(loc_file);
                if (generate_thin_class == GenerateThinClass::No)
                {
                    the_class = &m_parser_output.add_class(class_name, clang_getCString(path_name), class_scope);
                }
                else
                {
                    the_class = &m_parser_output.add_thin_class(class_name, clang_getCString(path_name), class_scope);
                }
                clang_disposeString(path_name);
            }
            catch (DoNotParseException& e)
            {
                if (e.is_verbose)
                {
                    //printf_s("Skipped class '%s' because %s\n", class_scope_and_name.c_str(), e.reason.c_str());
                }
                return nullptr;
            }

            struct VisitorData
            {
                CodeParser& this_ref;
                Class& class_ref;
                GenerateThinClass generate_thin_class;
            };
            VisitorData visitor_data{*this, *the_class, generate_thin_class};

            clang_visitChildren(cursor, [](CXCursor inner_cursor, CXCursor outer_cursor, CXClientData visitor_data_raw) -> CXChildVisitResult {
                auto& visitor_data = *static_cast<VisitorData*>(visitor_data_raw);
                auto cursor_kind = clang_getCursorKind(inner_cursor);
                if (visitor_data.generate_thin_class == GenerateThinClass::No)
                {
                    if (cursor_kind == CXCursor_FieldDecl)
                    {
                        // Non-static member variable.
                    }
                    else if (cursor_kind == CXCursor_VarDecl)
                    {
                        // Static member variable.
                    }
                    else if (cursor_kind == CXCursor_CXXMethod)
                    {
                        // Static or non-static member function.

                        auto inner_cursor_spelling = clang_getCursorSpelling(inner_cursor);
                        auto fully_qualified_scope_and_name = resolve_scope(inner_cursor) + "::" + clang_getCString(inner_cursor_spelling);
                        clang_disposeString(inner_cursor_spelling);
                        //printf_s("Checking if custom member function should be generated: %s\n", fully_qualified_scope_and_name.c_str());
                        if (auto it = visitor_data.this_ref.m_out_of_line_custom_member_function_names.find(fully_qualified_scope_and_name); it == visitor_data.this_ref.m_out_of_line_custom_member_function_names.end())
                        {
                            //printf_s("Generating custom member function: %s\n", fully_qualified_scope_and_name.c_str());
                            bool is_static = clang_CXXMethod_isStatic(inner_cursor) == 1;
                            IsStaticFunction is_static_function = is_static ? IsStaticFunction::Yes : IsStaticFunction::No;
                            visitor_data.this_ref.generate_lua_class_member_function(visitor_data.class_ref, inner_cursor, is_static ? visitor_data.class_ref.static_functions : visitor_data.class_ref.container.functions, is_static_function);
                        }
                    }
                    else if (cursor_kind == CXCursor_FunctionDecl)
                    {
                        // Non-member function.
                    }
                    else if (cursor_kind == CXCursor_FunctionTemplate)
                    {
                        //auto function_name = std::string{clang_getCString(clang_getCursorSpelling(inner_cursor))};
                        //if (function_name.find("ForEachName") != function_name.npos)
                        //{
                        //    printf_s("func: %s (is def: %s, is decl: %s)\n", function_name.c_str(), clang_isCursorDefinition(inner_cursor) == 0 ? "No" : "Yes", clang_isDeclaration(inner_cursor.kind) == 0 ? "No" : "Yes");
                        //    clang_visitChildren(inner_cursor, [](CXCursor inner_cursor, CXCursor outer_cursor, CXClientData visitor_data_raw) -> CXChildVisitResult {
                        //        printf_s("inner func: %i, %s, %s\n", inner_cursor.kind, clang_getCString(clang_getCursorKindSpelling(inner_cursor.kind)), clang_getCString(clang_getCursorSpelling(inner_cursor)));
                        //        return CXChildVisit_Continue;
                        //    }, nullptr);
                        //}
                    }
                    else if (cursor_kind == CXCursor_CXXBaseSpecifier)
                    {
                        visitor_data.this_ref.resolve_base(inner_cursor, visitor_data.class_ref);
                    }
                    else if (cursor_kind == CXCursor_Constructor)
                    {
                        if (clang_CXXConstructor_isDefaultConstructor(inner_cursor) == 1)
                        {
                            visitor_data.class_ref.has_parameterless_constructor = true;
                        }
                        else
                        {
                            auto function = visitor_data.this_ref.generate_lua_class_member_function(visitor_data.class_ref, inner_cursor, visitor_data.class_ref.constructors, IsStaticFunction::Yes);
                            if (function)
                            {
                                std::unique_ptr<Type::Base> return_type{};
                                try
                                {
                                    function->set_is_constructor(true);
                                    function->set_return_type(std::move(visitor_data.this_ref.cxtype_to_type(clang_getCursorType(outer_cursor))));
                                }
                                catch (DoNotParseException& e)
                                {
                                    printf_s("%s\n", std::format("Skipped constructor '{}' due to the return type because {}", function->get_name(), e.reason).c_str());
                                }
                            }
                        }
                    }
                }

                if (cursor_kind == CXCursor_StructDecl || cursor_kind == CXCursor_ClassDecl)
                {
                    if (clang_isCursorDefinition(inner_cursor))
                    {
                        visitor_data.this_ref.generate_lua_class(inner_cursor, GenerateThinClass::Yes);
                    }
                }

                return CXChildVisitResult::CXChildVisit_Continue;
            }, &visitor_data);

            if (generate_thin_class == GenerateThinClass::No && !lua_state_type.empty())
            {
                m_parser_output.add_lua_state_type(lua_state_type);
            }
        }
        else if (generate_thin_class == GenerateThinClass::No)
        {
            try
            {
                auto cxpath_name = clang_File_tryGetRealPathName(loc_file);
                auto path_name_raw = clang_getCString(cxpath_name);
                std::string path_name{};
                if (path_name_raw)
                {
                    path_name = path_name_raw;
                }
                else
                {
                    auto file_name = clang_getFileName(loc_file);
                    path_name = clang_getCString(file_name);
                    clang_disposeString(file_name);
                }
                clang_disposeString(cxpath_name);
                the_class = &m_parser_output.add_thin_class(class_name, path_name, class_scope);

                struct VisitorData
                {
                    CodeParser& this_ref;
                    Class& class_ref;
                };
                VisitorData visitor_data{*this, *the_class};

                clang_visitChildren(cursor, [](CXCursor inner_cursor, CXCursor outer_cursor, CXClientData visitor_data_raw) -> CXChildVisitResult {
                    auto& visitor_data = *static_cast<VisitorData*>(visitor_data_raw);
                    auto cursor_kind = clang_getCursorKind(inner_cursor);
                    if (cursor_kind == CXCursor_CXXBaseSpecifier)
                    {
                        visitor_data.this_ref.resolve_base(inner_cursor, visitor_data.class_ref);
                    }
                    return CXChildVisitResult::CXChildVisit_Continue;
                }, &visitor_data);
            }
            catch (DoNotParseException& e)
            {
                // The add_thin_class function only throws if the class already exists.
                // It then stores the existing class as 'DoNotParseException::data'.
                the_class = static_cast<Class*>(e.data);
            }

            struct VisitorData
            {
                CodeParser& this_ref;
                Class& class_ref;
            };
            VisitorData visitor_data{*this, *the_class};

            clang_visitChildren(cursor, [](CXCursor inner_cursor, CXCursor outer_cursor, CXClientData visitor_data_raw) -> CXChildVisitResult {
                auto& visitor_data = *static_cast<VisitorData*>(visitor_data_raw);
                auto cursor_kind = clang_getCursorKind(inner_cursor);
                if (cursor_kind == CXCursor_StructDecl || cursor_kind == CXCursor_ClassDecl)
                {
                    if (clang_isCursorDefinition(inner_cursor))
                    {
                        visitor_data.this_ref.generate_lua_class(inner_cursor, GenerateThinClass::Yes);
                    }
                }

                return CXChildVisitResult::CXChildVisit_Continue;
            }, &visitor_data);
        }

        if (the_class)
        {
            if (!scope_override.empty())
            {
                the_class->scope_override = scope_override;
            }

            if (auto it = m_custom_base_classes.find(class_scope_and_name); it != m_custom_base_classes.end())
            {
                for (const auto& [base_class_scope, base_class_name] : it->second)
                {
                    auto base = m_parser_output.get_container().find_class_by_name(base_class_scope, base_class_name);
                    if (base)
                    {
                        the_class->get_mutable_bases().emplace(base);
                    }
                }
            }

            if (auto it = m_custom_base_classes_inverted.find(class_scope_and_name); it != m_custom_base_classes_inverted.end())
            {
                for (auto& [deriving_class_scope, deriving_class_name] : it->second)
                {
                    auto deriving_class = m_parser_output.get_container().find_mutable_class_by_name(deriving_class_scope, deriving_class_name);
                    if (deriving_class)
                    {
                        deriving_class->get_mutable_bases().emplace(the_class);
                    }
                }
            }
        }

        return the_class;
    }

    auto CodeParser::generate_lua_class_member_function(Class& the_class, const CXCursor& cursor, FunctionContainer& function_container, IsStaticFunction is_static_function) -> Function*
    {
        if (clang_getCXXAccessSpecifier(cursor) != CX_CXXPublic) { return nullptr; }
        auto function = generate_lua_function(cursor, function_container, &the_class);
        if (function)
        {
            if (is_static_function == IsStaticFunction::Yes)
            {
                function->set_is_static(true);
            }
        }

        return function;
    }

    auto resolve_parent_chain(CXCursor& cursor, auto get_parent_function) -> std::string
    {
        auto parent_spelling = clang_getCursorSpelling(get_parent_function(cursor));
        std::string buffer{clang_getCString(parent_spelling)};
        clang_disposeString(parent_spelling);

        auto cursor2 = get_parent_function(cursor);
        auto parent_spelling2 = clang_getCursorSpelling(get_parent_function(cursor2));
        buffer.append(std::format(" -> {}", clang_getCString(parent_spelling2)));
        clang_disposeString(parent_spelling2);

        auto cursor3 = get_parent_function(cursor2);
        auto parent_spelling3 = clang_getCursorSpelling(get_parent_function(cursor3));
        buffer.append(std::format(" -> {}", clang_getCString(parent_spelling3)));
        clang_disposeString(parent_spelling3);

        auto cursor4 = get_parent_function(cursor3);
        auto parent_spelling4 = clang_getCursorSpelling(get_parent_function(cursor4));
        buffer.append(std::format(" -> {}", clang_getCString(parent_spelling4)));
        clang_disposeString(parent_spelling4);

        return buffer;
    }

    static bool is_forward_declaration(CXCursor cursor)
    {
        auto definition = clang_getCursorDefinition(cursor);

        // If the definition is null, then there is no definition in this translation
        // unit, so this cursor must be a forward declaration.
        if (clang_equalCursors(definition, clang_getNullCursor()))
            return true;

        // If there is a definition, then the forward declaration and the definition
        // are in the same translation unit. This cursor is the forward declaration if
        // it is _not_ the definition.
        return !clang_equalCursors(cursor, definition);
    }

    auto CodeParser::generate_internal(CXCursor inner_cursor, CXCursor outer_cursor) -> CXChildVisitResult
    {
        auto cursor_kind = clang_getCursorKind(inner_cursor);
        if (cursor_kind == CXCursor_StructDecl || cursor_kind == CXCursor_ClassDecl)
        {
            //dump_struct_or_class(inner_cursor);
            auto inner_cursor_spelling = clang_getCursorSpelling(inner_cursor);
            auto class_name = std::string{clang_getCString(inner_cursor_spelling)};
            clang_disposeString(inner_cursor_spelling);
            // This gets rid of forward declarations it seems.
            // It doesn't do anything about the same struct appearing in multiple translation units.
            // You must check if this class exists and you must use the scope to determine that it's the right class.
            if (clang_isCursorDefinition(inner_cursor))
            {
                //auto cursor_location = clang_getCursorLocation(inner_cursor);
                //CXFile file;
                //unsigned int line{};
                //unsigned int column{};
                //unsigned int offset{};
                //clang_getSpellingLocation(cursor_location, &file, &line, &column, &offset);
                //printf_s("Class: %s, outer: %s\n", class_name.c_str(), clang_getCString(clang_getCursorSpelling(outer_cursor)));
                //printf_s("is_forward_declaration: %s\n", is_forward_declaration(inner_cursor) ? "true" : "false");
                //printf_s("file: %s\n", clang_getCString(clang_getFileName(file)));
                //printf_s("line: %i\n", line);
                generate_lua_class(inner_cursor);
            }
            //else
            {
                //auto cursor_name = std::string{clang_getCString(clang_getCursorSpelling(inner_cursor))};
                //printf_s("Undefined definition: %s\n", cursor_name.c_str());
            }
        }
        else if (cursor_kind == CXCursor_ClassTemplate)
        {
            // TODO: Support this. Without template support, the system is much less powerful.
            //auto name = std::string{clang_getCString(clang_getCursorSpelling(inner_cursor))};
            //auto scope = resolve_scope(inner_cursor);
            //if (name.find("TArray") != name.npos)
            //{
            //    printf_s("%s\n", name.c_str());
            //    clang_visitChildren(inner_cursor, [](CXCursor inner_cursor, CXCursor outer_cursor, CXClientData visitor_data_raw) -> CXChildVisitResult {
            //        //auto& visitor_data = *static_cast<VisitorData*>(visitor_data_raw);
            //        auto name = std::string{clang_getCString(clang_getCursorSpelling(inner_cursor))};
            //        auto cursor_kind = clang_getCursorKind(inner_cursor);
            //        printf_s("    %s (%s)\n", name.c_str(), clang_getCString(clang_getCursorKindSpelling(cursor_kind)));
//
            //        return CXChildVisitResult::CXChildVisit_Continue;
            //    }, nullptr);
//
            //}
        }
        else if (cursor_kind == CXCursor_FieldDecl)
        {
            //dump_member_variable(inner_cursor);
        }
        else if (cursor_kind == CXCursor_VarDecl)
        {
            //dump_static_member_variable(inner_cursor);
        }
        else if (cursor_kind == CXCursor_UnexposedAttr)
        {
            //dump_attr(inner_cursor);
        }
        else if (cursor_kind == CXCursor_CXXMethod)
        {
            //dump_member_function(inner_cursor);
        }
        else if (cursor_kind == CXCursor_FunctionDecl)
        {
            //dump_static_member_function(inner_cursor);
            auto inner_cursor_spelling = clang_getCursorSpelling(inner_cursor);
            auto function_name = std::string{clang_getCString(inner_cursor_spelling)};
            clang_disposeString(inner_cursor_spelling);
            auto function_scope = resolve_scope(inner_cursor);
            auto function_name_and_scope = function_scope + "::" + function_name;

            if (auto metamethod_it = m_out_of_line_custom_metamethod_functions.find(function_name_and_scope); metamethod_it != m_out_of_line_custom_metamethod_functions.end())
            {
                auto* the_class = m_parser_output.get_container().find_mutable_class_by_name(metamethod_it->second.fully_qualified_scope, metamethod_it->second.in_class);
                Function* function{};
                if (the_class)
                {
                    function = create_custom_function(inner_cursor, the_class, metamethod_it->first, metamethod_it->second, the_class->metamethods, IsFunctionScopeless::Yes);
                }
                if (!function)
                {
                    return CXChildVisit_Continue;
                }
            }
            else if (auto member_function_it = m_out_of_line_custom_member_function_requests.find(function_name_and_scope); member_function_it != m_out_of_line_custom_member_function_requests.end())
            {
                auto member_function = CustomMemberFunctionWithCursor{member_function_it->second};
                create_custom_function(inner_cursor, nullptr, member_function_it->first, member_function.data, member_function.function_container);
                m_custom_member_functions_to_generate.emplace_back(member_function_it->first, std::move(member_function));
            }
            else
            {
                generate_lua_free_function(inner_cursor);
            }
        }
        else if (cursor_kind == CXCursor_FunctionTemplate)
        {
            //dump_member_function_template(inner_cursor);
        }
        else if (cursor_kind == CXCursor_InclusionDirective)
        {
            //dump_inclusion_directive(inner_cursor);
            //clang_getCursorSemanticParent(inner_cursor);
            //printf_s("An #include: %s\n", clang_getCString(clang_getFileName(clang_getIncludedFile(inner_cursor))));
            //printf_s("Inner: %s | Outer: %s\n", clang_getCString(clang_getCursorSpelling(inner_cursor)), clang_getCString(clang_getCursorSpelling(outer_cursor)));
            //auto semantic_parent = clang_getCursorSemanticParent(inner_cursor);
            //auto lexical_parent = clang_getCursorLexicalParent(inner_cursor);
            //printf_s("Semantic Parent: %i : %s | Lexical Parent: %i : %s\n", clang_getCursorKind(semantic_parent), clang_getCString(clang_getCursorSpelling(semantic_parent)), clang_getCursorKind(lexical_parent), clang_getCString(clang_getCursorSpelling(lexical_parent)));
        }
        else if (cursor_kind == CXCursor_Namespace)
        {
            return CXChildVisit_Recurse;
        }
        else if (cursor_kind == CXCursor_EnumDecl)
        {
            auto inner_cursor_spelling = clang_getCursorSpelling(inner_cursor);
            auto enum_name = std::string{clang_getCString(inner_cursor_spelling)};
            clang_disposeString(inner_cursor_spelling);
            auto enum_scope = resolve_scope(inner_cursor);
            if (auto it = m_out_of_line_enums.find(enum_scope + "::" + enum_name); it != m_out_of_line_enums.end())
            {
                auto [the_enum_it, was_inserted] = m_parser_output.get_container().enums.emplace(enum_scope + "::" + enum_name, Enum{
                        enum_name,
                        it->second.first,
                });

                if (!was_inserted)
                {
                    return CXChildVisitResult::CXChildVisit_Continue;
                }

                struct VisitorData
                {
                    Enum& enum_ref;
                };
                VisitorData visitor_data{the_enum_it->second};
                clang_visitChildren(inner_cursor, [](CXCursor inner_cursor, CXCursor outer_cursor, CXClientData visitor_data_raw) -> CXChildVisitResult {
                    auto&visitor_data = *static_cast<VisitorData*>(visitor_data_raw);
                    auto enum_key_type_spelling = clang_getCursorSpelling(inner_cursor);
                    auto enum_key = std::string{clang_getCString(enum_key_type_spelling)};
                    clang_disposeString(enum_key_type_spelling);
                    auto enum_value = clang_getEnumConstantDeclUnsignedValue(inner_cursor);
                    visitor_data.enum_ref.add_key_value_pair(enum_key, enum_value);

                    return CXChildVisitResult::CXChildVisit_Continue;
                }, &visitor_data);
            }
        }

        //printf_s("cursor_kind: %i\n", cursor_kind);
        //return CXChildVisit_Recurse;
        return CXChildVisit_Continue;
    }

    auto CodeParser::generator_internal_clang_wrapper(CXCursor inner_cursor, CXCursor outer_cursor, CXClientData self) -> CXChildVisitResult
    {
        return static_cast<CodeParser*>(self)->generate_internal(inner_cursor, outer_cursor);
    }

    bool printDiagnostics(CXTranslationUnit translationUnit){
        unsigned int nbDiag = static_cast<unsigned int>(clang_getNumDiagnostics(translationUnit));
        printf("There are %i diagnostics:\n",nbDiag);

        bool foundError = false;
        for (unsigned int currentDiag = 0; currentDiag < nbDiag; ++currentDiag) {
            CXDiagnostic diagnotic = clang_getDiagnostic(translationUnit, currentDiag);
            CXString errorString = clang_formatDiagnostic(diagnotic,clang_defaultDiagnosticDisplayOptions());
            std::string tmp{clang_getCString(errorString)};
            clang_disposeString(errorString);
            if (tmp.find("error:") != std::string::npos) {
                foundError = true;
            }
            std::cerr << tmp << std::endl;
        }
        if (foundError) {
            std::cerr << "Please resolve these issues and try again." <<std::endl;
            //exit(-1);
        }
        return foundError;
    }

    auto CodeParser::parse() -> const CodeGenerator&
    {
        double total_timer_dur{};
        {
            ScopedTimer total_timer(&total_timer_dur);
            for (const auto& file : m_files_to_parse)
            {
                printf_s("Parsing file: %s\n", file.c_str());
                double timer_dur{};
                {
                    ScopedTimer timer(&timer_dur);

                    auto translation_unit = clang_parseTranslationUnit(m_current_index, file.c_str(), m_compiler_flags, m_num_compiler_flags, nullptr, 0, CXTranslationUnit_SkipFunctionBodies | CXTranslationUnit_DetailedPreprocessingRecord | CXTranslationUnit_KeepGoing | CXTranslationUnit_PrecompiledPreamble);
                    //if (printDiagnostics(translation_unit))
                    //{
                    //    continue;
                    //}

                    auto cursor = clang_getTranslationUnitCursor(translation_unit);
                    Type::generate_static_class_types(m_parser_output.get_container());

                    auto token_range = clang_getCursorExtent(cursor);
                    CXToken* tokens{};
                    unsigned int num_tokens{};
                    clang_tokenize(translation_unit, token_range, &tokens, &num_tokens);
                    for (unsigned int i = 0; i < num_tokens; ++i)
                    {
                        const auto& token = tokens[i];
                        const auto token_kind = clang_getTokenKind(token);

                        if (token_kind != CXTokenKind::CXToken_Comment) { continue; }

                        auto token_spelling = clang_getTokenSpelling(translation_unit, token);
                        auto token_contents = std::string{clang_getCString(token_spelling)};
                        clang_disposeString(token_spelling);
                        //printf_s("Token kind: %i\n", token_kind);
                        //printf_s("Token spelling: %s\n", token_contents);

                        auto parse_scope_and_class = [](std::string_view fully_qualified_scope_and_name) -> std::pair<std::string, std::string> {
                            std::string fully_qualified_scope{};
                            std::string class_name{};

                            auto scope_start = fully_qualified_scope_and_name.find_last_of(':');
                            if (scope_start != fully_qualified_scope_and_name.npos && scope_start + 1 <= fully_qualified_scope_and_name.size())
                            {
                                auto scope_end = fully_qualified_scope_and_name.rfind(':', scope_start);
                                fully_qualified_scope = fully_qualified_scope_and_name.substr(0, scope_end - 1);
                                class_name = fully_qualified_scope_and_name.substr(scope_start + 1);
                            }
                            else
                            {
                                fully_qualified_scope = "::";
                            }

                            return {fully_qualified_scope, class_name};
                        };

                        CommentParser comment_parser{token_contents};
                        if (auto lua_type_attribute = comment_parser.get_attribute("LuaStateTypes"); lua_type_attribute.exists())
                        {
                            auto lua_state_type = lua_type_attribute.get_param(0);
                            if (auto attribute = comment_parser.get_attribute("LuaLate"); attribute.exists() && attribute.num_params() >= 2)
                            {
                                auto type = attribute.get_param(0);
                                if (type == "Class")
                                {
                                    // CUSTOM_ATTRIBUTE[LuaLate(Class, ::RC::Unreal::UObjectBase)]
                                    // or
                                    // CUSTOM_ATTRIBUTE[LuaLate(Class, ::RC::Unreal::UObjectBase, ::)]

                                    auto scoped_class = attribute.get_param(1);
                                    auto scope = attribute.has_param(2) ? attribute.get_param(2) : parse_scope_and_class(scoped_class).first;
                                    //printf_s("Bindings requested out-of-line for class '%s' in lua state '%s'\n", scoped_class.c_str(), lua_state_type.c_str());
                                    m_out_of_line_class_requests.emplace(scoped_class, CustomClass{lua_state_type, scope});
                                }
                                else if (type == "FreeFunction")
                                {
                                    // CUSTOM_ATTRIBUTE[LuaLate(FreeFunction, ::RC::Unreal::UObjectGlobals::FindObject)]
                                    // or
                                    // CUSTOM_ATTRIBUTE[LuaLate(FreeFunction, ::RC::Unreal::UObjectGlobals::FindObject, ::)]
                                    // or
                                    // CUSTOM_ATTRIBUTE[LuaLate(FreeFunction, ::RC::Unreal::UObjectGlobals::FindObject, ::, UnscopedAlias)]

                                    auto scoped_function = attribute.get_param(1);
                                    auto [function_scope, function_name] = parse_scope_and_class(scoped_function);
                                    auto scope_override = attribute.has_param(2) ? attribute.get_param(2) : std::string{};
                                    auto scope = !scope_override.empty() && scope_override != "_" ? scope_override : function_scope;
                                    auto unscoped_alias = attribute.has_param(3) ? attribute.get_param(3) : std::string{};
                                    //printf_s("Bindings requested out-of-line for free-function '%s' in lua state '%s'\n", scoped_function.c_str(), lua_state_type.c_str());
                                    auto& function_data = m_out_of_line_free_function_requests.emplace(scoped_function, CustomFreeFunction{lua_state_type, scope}).first->second;

                                    function_data.names.emplace_back(unscoped_alias.empty() ? function_name : unscoped_alias);
                                }
                                else if (type == "CustomFreeFunction")
                                {
                                    // CUSTOM_ATTRIBUTE[LuaLate(CustomFreeFunction, ::RC::WriteInt8, ::RC::function_wrapper_WriteInt8)]
                                    // or
                                    // CUSTOM_ATTRIBUTE[LuaLate(CustomFreeFunction, ::RC::WriteInt8, ::RC::function_wrapper_WriteInt8, ::)]

                                    auto scoped_function = attribute.get_param(1);
                                    auto [function_scope, function_name] = parse_scope_and_class(scoped_function);
                                    auto wrapper_scope_and_name = attribute.get_param(2);
                                    auto scope = attribute.has_param(3) ? attribute.get_param(3) : function_scope;
                                    //printf_s("Bindings requested out-of-line for free-function '%s' in lua state '%s'\n", scoped_function.c_str(), lua_state_type.c_str());
                                    auto& custom_function_entry = m_out_of_line_custom_free_function_requests[wrapper_scope_and_name];
                                    if (scope.empty()) { scope = "::"; }
                                    if (custom_function_entry.names.empty())
                                    {
                                        custom_function_entry.lua_state_type = lua_state_type;
                                        custom_function_entry.scope = scope;
                                        custom_function_entry.wrapper_name = wrapper_scope_and_name;
                                    }
                                    custom_function_entry.names.emplace_back(function_name);
                                }
                                else if (type == "Enum")
                                {
                                    // CUSTOM_ATTRIBUTE[LuaLate(Enum, ::RC::LoopAction)]

                                    auto scoped_enum = attribute.get_param(1);
                                    auto [enum_scope, enum_name] = parse_scope_and_class(scoped_enum);
                                    auto scope = attribute.has_param(2) ? attribute.get_param(2) : enum_scope;
                                    m_out_of_line_enums.emplace(std::move(scoped_enum), std::pair{std::move(scope), std::move(enum_name)});
                                }
                            }
                            else if (auto attribute = comment_parser.get_attribute("LuaAddMetamethod"); attribute.exists() && attribute.num_params() >= 3)
                            {
                                // CUSTOM_ATTRIBUTE[LuaAddMetamethod(::RC::Unreal::UObjectBase, __index, ::RC::UObjectBase_metamethod_wrapper_Index)]

                                auto in_scoped_class = attribute.get_param(0);
                                auto metamethod_name = attribute.get_param(1);
                                auto wrapper_scope_and_name = attribute.get_param(2);
                                auto [fully_qualified_scope, in_class] = parse_scope_and_class(in_scoped_class);

                                m_out_of_line_custom_metamethod_functions[wrapper_scope_and_name] = {in_class, metamethod_name, wrapper_scope_and_name, fully_qualified_scope};
                            }
                            else if (auto attribute = comment_parser.get_attribute("LuaMapTemplateClass"); attribute.exists() && attribute.num_params() >= 2)
                            {
                                // CUSTOM_ATTRIBUTE[LuaMapTemplateClass(::RC::Unreal::TArray, ::RC::UnrealRuntimeTypes::Array)]

                                auto original_templated_class = attribute.get_param(0);
                                auto non_templated_class = attribute.get_param(1);

                                m_out_of_line_template_class_map.emplace(original_templated_class, non_templated_class);
                            }
                            else if (auto attribute = comment_parser.get_attribute("LuaAddBaseToClass"); attribute.exists() && attribute.num_params() >= 2)
                            {
                                // CUSTOM_ATTRIBUTE[LuaAddBaseToClass(::RC::Unreal::FObjectProperty, ::RC::Unreal::FObjectPropertyBase)]

                                auto the_class = attribute.get_param(0);
                                auto the_base_class = attribute.get_param(1);
                                auto [fully_qualified_scope, in_class] = parse_scope_and_class(the_class);
                                auto [fully_qualified_base_scope, in_base_class] = parse_scope_and_class(the_base_class);

                                m_custom_base_classes[the_class].emplace_back(fully_qualified_base_scope, in_base_class);
                                m_custom_base_classes_inverted[the_base_class].emplace_back(fully_qualified_scope, in_class);
                            }
                            else
                            {
                                enum class RedirectorType
                                {
                                    LuaMemberFunctionRedirector,
                                    LuaStaticMemberFunctionRedirector,
                                } redirector_type;
                                // LuaMemberFunctionRedirector or LuaStaticMemberFunctionRedirector
                                CommentAttribute redirector_attribute{};
                                redirector_attribute = comment_parser.get_attribute("LuaMemberFunctionRedirector");
                                if (!redirector_attribute.exists())
                                {
                                    redirector_attribute = comment_parser.get_attribute("LuaStaticMemberFunctionRedirector");
                                    redirector_type = RedirectorType::LuaStaticMemberFunctionRedirector;
                                }

                                if (redirector_attribute.exists() && redirector_attribute.num_params() >= 3)
                                {
                                    // CUSTOM_ATTRIBUTE[LuaMemberFunctionRedirector(::RC::Unreal::UObjectBase, MyTestFunc, ::RC::UObjectBase_member_function_wrapper_MyTestFunc)]

                                    auto in_scoped_class = redirector_attribute.get_param(0);
                                    auto function_name = redirector_attribute.get_param(1);
                                    auto wrapper_scope_and_name = redirector_attribute.get_param(2);
                                    auto [fully_qualified_scope, in_class] = parse_scope_and_class(in_scoped_class);

                                    if (redirector_type == RedirectorType::LuaStaticMemberFunctionRedirector)
                                    {
                                        m_out_of_line_custom_member_function_requests[wrapper_scope_and_name] = {.in_class = in_class,
                                                                                                                 .function_name = function_name,
                                                                                                                 .wrapper_scope_and_name = wrapper_scope_and_name,
                                                                                                                 .fully_qualified_scope = fully_qualified_scope,
                                                                                                                 .is_static = true};
                                    }
                                    else
                                    {
                                        m_out_of_line_custom_member_function_requests[wrapper_scope_and_name] = {.in_class = in_class,
                                                                                                                 .function_name = function_name,
                                                                                                                 .wrapper_scope_and_name = wrapper_scope_and_name,
                                                                                                                 .fully_qualified_scope = fully_qualified_scope,
                                                                                                                 .is_static = false};
                                    }
                                    m_out_of_line_custom_member_function_names.emplace(in_scoped_class + "::" + function_name);

                                    //printf_s("Bindings requested out-of-line for custom member function '%s' in class '%s', mapped to '%s'\n", function_name.c_str(), in_scoped_class.c_str(), wrapper_scope_and_name.c_str());
                                }
                            }
                        }
                    }
                    clang_disposeTokens(translation_unit, tokens, num_tokens);

                    clang_visitChildren(cursor, &CodeParser::generator_internal_clang_wrapper, this);

                    for (auto& [member_function_wrapper_name, member_function] : m_custom_member_functions_to_generate)
                    {
                        auto* the_class = m_parser_output.get_container().find_mutable_class_by_name(member_function.data.fully_qualified_scope, member_function.data.in_class);
                        auto& function = member_function.function_container.begin()->second;
                        function.set_parent_name(the_class->name);
                        function.set_parent_scope(the_class->fully_qualified_scope);
                        function.set_containing_class(the_class);
                        function.set_is_static(member_function.data.is_static);
                        if (member_function.data.is_static)
                        {
                            the_class->static_functions.emplace(function.get_name(), std::move(function));
                        }
                        else
                        {
                            the_class->container.functions.emplace(function.get_name(), std::move(function));
                        }
                    }

                    clang_disposeTranslationUnit(translation_unit);
                }
                m_custom_member_functions_to_generate.clear();
                printf_s("Parsing file took %f seconds.\n", timer_dur);
            }
        }
        printf_s("Parsing all files units took %f seconds.\n", total_timer_dur);

        return m_parser_output;
    }
}