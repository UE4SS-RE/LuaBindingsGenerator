#include <LuaWrapperGenerator/Patches/Unreal.hpp>

namespace RC::LuaWrapperGenerator::TypePatches::Unreal
{
    auto cxtype_to_type_post(Type::Base* type) -> void
    {
        if (type->is_a<Type::CustomStruct>())
        {
            auto as_custom_struct = static_cast<Type::CustomStruct*>(type);
            if (as_custom_struct->get_type_name() == "ArrayTest")
            {
                as_custom_struct->set_move_on_construction(true);
            }
        }
    }

    auto cxtype_to_type(CodeGenerator& code_generator, const CXType& cxtype, IsPointer is_pointer) -> std::unique_ptr<Type::Base>
    {
        std::unique_ptr<Type::Base> type{};

        if (cxtype.kind == CXTypeKind::CXType_LValueReference)
        {
            type = ::RC::LuaWrapperGenerator::cxtype_to_type(code_generator, clang_getPointeeType(cxtype), is_pointer);
            if (!type)
            {
                type = ::RC::LuaWrapperGenerator::TypePatches::Unreal::cxtype_to_type(code_generator, clang_getPointeeType(cxtype), is_pointer);
            }

            if (type)
            {
                type->set_is_ref(true);
            }
        }
        else if (cxtype.kind == CXTypeKind::CXType_Elaborated)
        {
            auto named_type = clang_Type_getNamedType(cxtype);
            type = ::RC::LuaWrapperGenerator::cxtype_to_type(code_generator, named_type, is_pointer);
            if (!type)
            {
                type = ::RC::LuaWrapperGenerator::TypePatches::Unreal::cxtype_to_type(code_generator, named_type, is_pointer);
            }
        }
        else if (cxtype.kind == CXTypeKind::CXType_Unexposed)
        {
            auto canonical_type_spelling = clang_getTypeSpelling(clang_getCanonicalType(cxtype));
            std::string canonical_name = clang_getCString(canonical_type_spelling);
            clang_disposeString(canonical_type_spelling);
            auto first_angle_brace = canonical_name.find_first_of('<');
            size_t start_of_type{};
            if (first_angle_brace != canonical_name.npos)
            {
                auto space = canonical_name.rfind(' ', first_angle_brace);
                if (space == canonical_name.npos) { space = 0; }
                std::string type_without_template{};
                // We'll be inserting at the start of the string later and that could cause a reallocation.
                // To avoid the reallocation, we reserve the same size as the canonical name.
                // The canonical name is always considerably bigger than the type without template thus guaranteeing that we don't reallocate when we insert later.
                type_without_template.reserve(canonical_name.size());
                type_without_template = canonical_name.substr(0, first_angle_brace);
                if (space > 0)
                {
                    type_without_template.erase(0, space + 1);
                }
                type_without_template.insert(0, "::");

                //if (auto it = m_out_of_line_template_class_map.find(type_without_template); it != m_out_of_line_template_class_map.end())
                if (type_without_template == "::RC::Unreal::TArray")
                {
                    auto type_decl = clang_getTypeDeclaration(cxtype);
                    auto canonical_type_decl = clang_getTypeDeclaration(clang_getCanonicalType(cxtype));

                    /*
                    std::vector<std::unique_ptr<Type::Base>> template_args{};
                    auto num_template_args = clang_Type_getNumTemplateArguments(cxtype);
                    for (int current_template_arg_index = 0; current_template_arg_index < num_template_args; ++current_template_arg_index)
                    {
                        auto template_arg_type = clang_Type_getTemplateArgumentAsType(cxtype, current_template_arg_index);
                        template_args.emplace_back(cxtype_to_type(template_arg_type));
                        printf_s("    template_arg type: %s\n", clang_getCString(clang_getTypeSpelling(template_arg_type)));
                    }
                    //*/

                    auto num_template_args = clang_Type_getNumTemplateArguments(cxtype);
                    if (num_template_args < 1) { return nullptr; }

                    auto array_element_cxtype = clang_Type_getTemplateArgumentAsType(cxtype, 0);

                    auto array_element_type = ::RC::LuaWrapperGenerator::cxtype_to_type(code_generator, array_element_cxtype);

                    if (!array_element_type)
                    {
                        throw DoNotParseException{"templated type not supported"};
                    }
                    //printf_s("    array_element_type: %s\n", array_element_type->generate_cxx_name().c_str());

                    //printf_s("Do some custom stuff for TArray, maybe involving FScriptArray.\n");
                    type = std::make_unique<TArray>(code_generator.get_container(), std::move(array_element_type));

                    CXCursor array_element_cursor{};
                    if (array_element_cxtype.kind == CXTypeKind::CXType_Pointer)
                    {
                        array_element_cursor = clang_getTypeDeclaration(clang_getPointeeType(array_element_cxtype));
                    }
                    else
                    {
                        array_element_cursor = clang_getTypeDeclaration(array_element_cxtype);
                    }

                    if (array_element_cursor.kind != CXCursorKind::CXCursor_NoDeclFound)
                    {
                        auto loc = clang_getCursorLocation(array_element_cursor);
                        CXFile loc_file;
                        unsigned loc_line;
                        unsigned loc_column;
                        unsigned loc_offset;
                        clang_getSpellingLocation(loc, &loc_file, &loc_line, &loc_column, &loc_offset);
                        auto path_name = clang_File_tryGetRealPathName(loc_file);
                        code_generator.get_container().extra_includes.emplace_back(clang_getCString(path_name));
                        clang_disposeString(path_name);
                    }
                }
            }
        }

        return type;
    }

    //template<typename T>
    //concept HasStaticClassMemberFunction =
    //requires(T t) {
    //    { T::StaticClass() };
    //};

    auto generate_state_file_pre(const Container& container) -> std::string
    {
        return R"()";
    }

    auto generate_state_file_post(const Container& container) -> std::string
    {
        std::string buffer{};
        buffer.append("inline std::unordered_map<std::string, void (*)(lua_State*, void*, uint32_t)> lua_type_name_to_lua_object_from_heap {\n");
        buffer.append("    // Custom types.\n");
        for (const auto&[_, the_class] : container.classes)
        {
            buffer.append(std::format("    {{\"{}::{}\", &lua_Userdata_to_lua_from_heap<\"{}_{}Metatable\", {}::{}>}},\n", the_class.fully_qualified_scope, the_class.name, scope_as_function_name(the_class.fully_qualified_scope), the_class.name, the_class.fully_qualified_scope, the_class.name));
        }

        buffer.append("\n    // Built-in types.\n");
        buffer.append("    {\"int8_t\", &lua_int8_t_to_lua_from_heap},\n");
        buffer.append("    {\"int16_t\", &lua_int16_t_to_lua_from_heap},\n");
        buffer.append("    {\"int32_t\", &lua_int32_t_to_lua_from_heap},\n");
        buffer.append("    {\"int64_t\", &lua_int64_t_to_lua_from_heap},\n");
        buffer.append("    {\"uint8_t\", &lua_uint8_t_to_lua_from_heap},\n");
        buffer.append("    {\"uint16_t\", &lua_uint16_t_to_lua_from_heap},\n");
        buffer.append("    {\"uint32_t\", &lua_uint32_t_to_lua_from_heap},\n");
        buffer.append("    {\"uint64_t\", &lua_uint64_t_to_lua_from_heap},\n");
        buffer.append("    {\"float\", &lua_float_to_lua_from_heap},\n");
        buffer.append("    {\"double\", &lua_double_to_lua_from_heap},\n");

        buffer.append("};\n\n");

        buffer.append("inline std::unordered_map<std::string, void (*)(lua_State*, void*, uint32_t)> lua_ue_type_name_to_lua_object_from_heap {\n");
        buffer.append("    // Custom types.\n");
        for (const auto&[_, the_class] : container.classes)
        {
            std::string class_name{the_class.name};
            if (class_name.starts_with('F') || class_name.starts_with('U'))
            {
                class_name.erase(0, 1);
            }
            buffer.append(std::format("    {{\"{}\", &lua_Userdata_to_lua_from_heap<\"{}_{}Metatable\", {}::{}>}},\n", class_name, scope_as_function_name(the_class.fully_qualified_scope), the_class.name, the_class.fully_qualified_scope, the_class.name));
        }

        buffer.append("};\n\n");

        return buffer;
    }

    auto generate_lua_setup_state_function_post() -> std::string
    {
        return R"(#define NUMERICAL_METATABLE_GET_SELF(Type)                                                                                        \
    luaL_argcheck(lua_state, lua_isuserdata(lua_state, 1), 1, "first param was not userdata");                                                      \
    lua_getiuservalue(lua_state, 1, 1);                                                                                                             \
    int pointer_depth = lua_tointeger(lua_state, -1);                                                                                               \
    bool is_pointer = pointer_depth > 0;                                                                                                            \
    lua_getmetatable(lua_state, 1);                                                                                                                 \
    lua_pushliteral(lua_state, "__name");                                                                                                           \
    lua_rawget(lua_state, -2);                                                                                                                      \
    auto metatable_name = std::string{lua_tostring(lua_state, -1)};                                                                                 \
    auto bad_self_error_message = std::format("self was '{}', expected '"#Type"Metatable' or derivative", metatable_name);                          \
    luaL_argcheck(lua_state, metatable_name == #Type"Metatable", 1, bad_self_error_message.c_str());                                                \
    lua_pop(lua_state, 3);                                                                                                                          \
    Type** self_container{};                                                                                                                        \
    Type* self{};                                                                                                                                   \
    if (is_pointer)                                                                                                                                 \
    {                                                                                                                                               \
        auto* outer_most_container = lua_touserdata(lua_state, 1);                                                                                  \
        self_container = static_cast<Type**>(deref(outer_most_container, pointer_depth - 1));                                                       \
        self = *self_container;                                                                                                                     \
    }                                                                                                                                               \
    else                                                                                                                                            \
    {                                                                                                                                               \
        self = static_cast<Type*>(lua_touserdata(lua_state, 1));                                                                                    \
    }                                                                                                                                               \
    lua_remove(lua_state, 1);                                                                                                                       \
    {                                                                                                                                               \
        luaL_argcheck(lua_state, self, 1, "self was nullptr");                                                                                      \
    }

#define INTEGRAL_NEWINDEX_METAMETHOD_BODY(Type, IsFunction, ToFunction)                                                                             \
        NUMERICAL_METATABLE_GET_SELF(Type)                                                                                                          \
                                                                                                                                                    \
        if (IsFunction(lua_state, 1))                                                                                                               \
        {                                                                                                                                           \
            *static_cast<Type*>(self) = ToFunction(lua_state, 1);                                                                                   \
        }                                                                                                                                           \
        else                                                                                                                                        \
        {                                                                                                                                           \
            luaL_argerror(lua_state, 1, "Invalid argument for 'Set'");                                                                              \
        }                                                                                                                                           \
        return 0;

#define REGISTER_NUMERICAL_METATABLE(Type, PushFunction, IsFunction, ToFunction)                                                                    \
    luaL_newmetatable(lua_state, #Type"Metatable");                                                                                                 \
    lua_pushliteral(lua_state, "__index");                                                                                                          \
    auto Type##_my_index = [](lua_State* lua_state) -> int {                                                                                        \
        NUMERICAL_METATABLE_GET_SELF(Type)                                                                                                          \
                                                                                                                                                    \
        if (lua_isstring(lua_state, -1))                                                                                                            \
        {                                                                                                                                           \
            auto index = std::string_view{lua_tostring(lua_state, -1)};                                                                             \
            lua_pop(lua_state, 1);                                                                                                                  \
            if (index == "Get" || index == "get")                                                                                                   \
            {                                                                                                                                       \
                lua_pushcfunction(lua_state, [](lua_State* lua_state) {                                                                             \
                    NUMERICAL_METATABLE_GET_SELF(Type)                                                                                              \
                    PushFunction(lua_state, *self);                                                                                                 \
                    return 1;                                                                                                                       \
                });                                                                                                                                 \
                return 1;                                                                                                                           \
            }                                                                                                                                       \
            else if (index == "Set" || index == "set")                                                                                              \
            {                                                                                                                                       \
                lua_pushcfunction(lua_state, [](lua_State* lua_state) {                                                                             \
                    INTEGRAL_NEWINDEX_METAMETHOD_BODY(Type, IsFunction, ToFunction)                                                                 \
                });                                                                                                                                 \
                return 1;                                                                                                                           \
            }                                                                                                                                       \
        }                                                                                                                                           \
                                                                                                                                                    \
        return 0;                                                                                                                                   \
    };                                                                                                                                              \
    lua_pushcfunction(lua_state, Type##_my_index);                                                                                                  \
    lua_rawset(lua_state, -3);                                                                                                                      \
    lua_settop(lua_state, 0);

    lua_settop(lua_state, 0);
    REGISTER_NUMERICAL_METATABLE(int8_t, lua_pushinteger, lua_isinteger, lua_tointeger)
    REGISTER_NUMERICAL_METATABLE(int16_t, lua_pushinteger, lua_isinteger, lua_tointeger)
    REGISTER_NUMERICAL_METATABLE(int32_t, lua_pushinteger, lua_isinteger, lua_tointeger)
    REGISTER_NUMERICAL_METATABLE(int64_t, lua_pushinteger, lua_isinteger, lua_tointeger)
    REGISTER_NUMERICAL_METATABLE(uint8_t, lua_pushinteger, lua_isinteger, lua_tointeger)
    REGISTER_NUMERICAL_METATABLE(uint16_t, lua_pushinteger, lua_isinteger, lua_tointeger)
    REGISTER_NUMERICAL_METATABLE(uint32_t, lua_pushinteger, lua_isinteger, lua_tointeger)
    REGISTER_NUMERICAL_METATABLE(uint64_t, lua_pushinteger, lua_isinteger, lua_tointeger)
    REGISTER_NUMERICAL_METATABLE(float, lua_pushnumber, lua_isnumber, lua_tonumber)
    REGISTER_NUMERICAL_METATABLE(double, lua_pushnumber, lua_isnumber, lua_tonumber)

#undef REGISTER_NUMERICAL_METATABLE
#undef INTEGRAL_NEWINDEX_METAMETHOD_BODY

)";
    }

    auto generate_per_class_static_functions(const Class& the_class) -> std::string
    {
        std::string buffer;

        buffer.append("    lua_pushliteral(lua_state, \"__ue_name\");\n");

        std::string class_name{the_class.name};
        if (class_name.starts_with('F') || class_name.starts_with('U'))
        {
            class_name.erase(0, 1);
        }

        buffer.append(std::format("    lua_pushliteral(lua_state, \"{}\");\n", class_name));
        buffer.append("    lua_rawset(lua_state, -3);\n\n");

        return buffer;
    }

    auto TArray::generate_cxx_name() const -> std::string
    {
        // TODO: Figure out this type name.
        //       It should be a type that contains an FScriptArray* as well as the element size or type.
        return "FScriptArray";
    }

    auto TArray::generate_lua_stack_validation_condition(int stack_index) const -> std::string
    {
        return std::format("lua_isuserdata(lua_state, {})", stack_index);
    }

    auto TArray::generate_lua_stack_retriever(int stack_index) const -> std::string
    {
        std::string buffer{};

        buffer.append(std::format("static_cast<{}::{}*>(luaL_checkudata(lua_state, {}, \"{}_{}Metatable\"))",
                                  fully_qualified_struct_scope,
                                  struct_name,
                                  stack_index,
                                  scope_as_function_name(fully_qualified_struct_scope),
                                  struct_name));

        return buffer;
    }

    auto TArray::generate_lua_stack_pusher(std::string_view variable_to_push, std::string_view param_prefix) const -> std::string
    {
        std::string buffer{};

        /*
        if (is_pointer())
        {
            buffer.append(std::format("auto* userdata = static_cast<{}::{}*>(lua_newuserdatauv(lua_state, sizeof({}::{}*), 1));\n", fully_qualified_struct_scope, struct_name, fully_qualified_struct_scope, struct_name));
            buffer.append("lua_pushboolean(lua_state, true);\n");
        }
        else
        {
            buffer.append(std::format("auto* userdata = static_cast<{}::{}*>(lua_newuserdatauv(lua_state, sizeof({}::{}), 1));\n", fully_qualified_struct_scope, struct_name, fully_qualified_struct_scope, struct_name));
            buffer.append("lua_pushboolean(lua_state, false);\n");
        }
        buffer.append("lua_setiuservalue(lua_state, -2, 1);\n");

        if (is_pointer())
        {
            buffer.append(std::format("new(userdata) {}::{}*{{{}}};\n", fully_qualified_struct_scope, struct_name, variable_to_push));
        }
        else
        {
            buffer.append(std::format("new(userdata) {}::{}{{{}}};\n", fully_qualified_struct_scope, struct_name, variable_to_push));
        }

        buffer.append(std::format("luaL_getmetatable(lua_state, \"{}_{}Metatable\");\n", scope_as_function_name(fully_qualified_struct_scope), struct_name));
        buffer.append("    lua_setmetatable(lua_state, -2)");
        //*/

        auto generate_stack_pusher_internal = [&]() {
            buffer.append(std::format("        auto* userdata = static_cast<{}::{}*>(lua_newuserdatauv({}lua_state, sizeof({}::{}), 1));\n", fully_qualified_struct_scope, struct_name, param_prefix, fully_qualified_struct_scope, struct_name));
            buffer.append(std::format("        lua_pushinteger({}lua_state, 0);\n", param_prefix));
            buffer.append(std::format("        lua_setiuservalue({}lua_state, -2, 1);\n", param_prefix));

            if (!is_pointer())
            {
                buffer.append(std::format("        new(userdata) {}::{}{{std::move(array_wrapper)}};\n", fully_qualified_struct_scope, struct_name));
            }
            else
            {
                buffer.append(std::format("        new(userdata) {}::{}{{array_wrapper}};\n", fully_qualified_struct_scope, struct_name));
            }

            buffer.append(std::format("        luaL_getmetatable({}lua_state, \"{}_{}Metatable\");\n", param_prefix, scope_as_function_name(fully_qualified_struct_scope), struct_name));
            buffer.append(std::format("        lua_setmetatable({}lua_state, -2);\n", param_prefix));
        };

        auto element_type_as_custom_struct = dynamic_cast<Type::CustomStruct*>(m_element_type.get());
        auto fully_qualified_element_type_name =
                element_type_as_custom_struct ? std::format("{}::{}", element_type_as_custom_struct->get_fully_qualified_scope(), element_type_as_custom_struct->generate_cxx_name())
                                              : std::format("{}", m_element_type->generate_cxx_name());

        // TODO: Deal with whether the return type (TArray<T>) is a value, pointer or reference.
        //       If value: We need to allocate space and construct a new FScriptArray. Use Lua for this so that GC can handle deallocating.
        //       If pointer: bit_cast 'variable_to_push' to FScriptArray*.
        //       If reference: bit_cast the address of 'variable_to_push' to FScriptArray.
        auto generate_bit_cast = [&]() -> std::string {
            if (is_pointer())
            {
                return std::format("std::bit_cast<{}::{}*>({})", fully_qualified_wrapped_struct_scope, wrapped_struct_name, variable_to_push);
            }
            else if (is_ref())
            {
                return std::format("std::bit_cast<{}::{}*>(&{})", fully_qualified_wrapped_struct_scope, wrapped_struct_name, variable_to_push);
            }
            else
            {
                return std::format("std::move(*std::bit_cast<{}::{}*>(&{}))", fully_qualified_wrapped_struct_scope, wrapped_struct_name, variable_to_push);
            }
        };

        bool is_element_type_trivial{true};
        if (element_type_as_custom_struct)
        {
            auto element_type = get_container().find_class_by_name(element_type_as_custom_struct->get_fully_qualified_scope(), element_type_as_custom_struct->generate_cxx_name());
            if (element_type && element_type->find_static_function_by_name("StaticClass"))
            {
                if (element_type_as_custom_struct->is_pointer())
                {
                    buffer.append(std::format("        auto array_wrapper = {}::{}{{{}, sizeof({}*), alignof({}*), \"{}\", true}};\n", fully_qualified_struct_scope, struct_name, generate_bit_cast(), fully_qualified_element_type_name, fully_qualified_element_type_name, fully_qualified_element_type_name));
                }
                else
                {
                    buffer.append(std::format("        auto array_wrapper = {}::{}{{{}, static_cast<size_t>({}::StaticClass()->GetPropertiesSize()), static_cast<size_t>({}::StaticClass()->GetMinAlignment()), \"{}\", false}};\n", fully_qualified_struct_scope, struct_name, generate_bit_cast(), fully_qualified_element_type_name, fully_qualified_element_type_name, fully_qualified_element_type_name));
                }
                is_element_type_trivial = false;
            }
        }

        if (is_element_type_trivial)
        {
            if (m_element_type->is_pointer())
            {
                buffer.append(std::format("        auto array_wrapper = {}::{}{{{}, sizeof({}), alignof({}), \"{}\", true}};\n", fully_qualified_struct_scope, struct_name, generate_bit_cast(), fully_qualified_element_type_name, fully_qualified_element_type_name, fully_qualified_element_type_name));
            }
            else
            {
                buffer.append(std::format("        auto array_wrapper = {}::{}{{{}, sizeof({}), alignof({}), \"{}\", false}};\n", fully_qualified_struct_scope, struct_name, generate_bit_cast(), fully_qualified_element_type_name, fully_qualified_element_type_name, fully_qualified_element_type_name));
            }
        }
        generate_stack_pusher_internal();

        //buffer.append(std::format("if constexpr (HasStaticClassMemberFunction<{}>)\n    {{\n", fully_qualified_element_type_name));
        //buffer.append(std::format("        auto array_wrapper = {}::{}{{{}, {}::StaticClass()->GetMinAlignment()}};\n", fully_qualified_struct_scope, struct_name, generate_bit_cast(), fully_qualified_element_type_name));
        //buffer.append("        \n");
        //generate_stack_pusher_internal();
        //buffer.append("    }    \n    else\n    {\n");
        //buffer.append(std::format("        auto array_wrapper = {}::{}{{{}, sizeof({})}};\n", fully_qualified_struct_scope, struct_name, generate_bit_cast(), fully_qualified_element_type_name));
        //generate_stack_pusher_internal();
        //buffer.append("        \n");
        //buffer.append("    }\n");

        return buffer;
    }

    auto TArray::generate_converted_type(size_t param_num, std::vector<std::string>& recursion_resetters) const -> std::string
    {
        //return std::format("auto param_ansi_{}{{param_inter_{}}}; auto param_wide_{} = std::string{{param_ansi_{}}};\nauto param_{} = {}{{param_wide_{}.begin(), param_wide_{}.end()}}",
        //                   param_num,
        //                   param_num,
        //                   param_num,
        //                   param_num,
        //                   param_num,
        //                   generate_cxx_name(),
        //                   param_num,
        //                   param_num);

        std::string pointer_ref{};
        if (m_element_type->is_pointer())
        {
            pointer_ref.append("*");
        }
        if (m_element_type->is_ref())
        {
            pointer_ref.append("&");
        }

        // TODO: Check if the TArray (param) is a reference. If so, then we should allocate & construct a new TArray (with Lua for GC & ease of use) and pass that to the C++ function.
        //       Then we need to cast that TArray to a FScriptArray and pass it back to Lua somehow.
        //       This is treated as an out-param because it more or less has to be. We can't know if it's used like one but it could be so we must assume that it is.

        std::string buffer{};

        auto element_type_as_custom_struct = dynamic_cast<Type::CustomStruct*>(m_element_type.get());
        auto fully_qualified_element_type_name =
                element_type_as_custom_struct ? std::format("{}::{}", element_type_as_custom_struct->get_fully_qualified_scope(), element_type_as_custom_struct->generate_cxx_name())
                                              : std::format("{}", m_element_type->generate_cxx_name());
        buffer.append(std::format("auto& param_{} = *std::bit_cast<::RC::Unreal::TArray<{}{}{}>*>(param_inter_{})",
                                  param_num,
                                  m_element_type->is_const() ? "const " : "",
                                  fully_qualified_element_type_name,
                                  pointer_ref,
                                  param_num));

        /*
        auto generate_bit_cast = [&]() -> std::string {
            std::string bit_cast_buffer{"std::bit_cast<"};

            if (is_pointer())
            {
                bit_cast_buffer.append(std::format("{}::{}*>(param_inter_{})", fully_qualified_wrapped_struct_scope, wrapped_struct_name, param_num));
            }
            else if (is_ref())
            {
                bit_cast_buffer.append(std::format("{}::{}*>(&param_inter_{})", fully_qualified_wrapped_struct_scope, wrapped_struct_name, param_num));
            }
            else
            {
                bit_cast_buffer.append(std::format("FIXME*>()"));
            }

            return bit_cast_buffer;
        };

        if (is_ref())
        {
            auto element_type_as_custom_struct = dynamic_cast<Type::CustomStruct*>(m_element_type.get());
            auto fully_qualified_element_type_name =
                    element_type_as_custom_struct ? std::format("{}::{}", element_type_as_custom_struct->get_fully_qualified_scope(), element_type_as_custom_struct->generate_cxx_name())
                                                  : std::format("{}", m_element_type->generate_cxx_name());

            bool is_element_type_trivial{true};
            if (element_type_as_custom_struct)
            {
                auto element_type = get_container().find_class_by_name(element_type_as_custom_struct->get_fully_qualified_scope(), element_type_as_custom_struct->generate_cxx_name());
                if (element_type && element_type->find_static_function_by_name("StaticClass"))
                {
                    buffer.append(std::format("auto array_wrapper = {}::{}{{{}, static_cast<size_t>({}::StaticClass()->GetMinAlignment())}};\n", fully_qualified_struct_scope, struct_name, generate_bit_cast(), fully_qualified_element_type_name));
                    is_element_type_trivial = false;
                }
            }

            if (is_element_type_trivial)
            {
                buffer.append(std::format("auto array_wrapper = {}::{}{{{}, sizeof({}{})}};\n", fully_qualified_struct_scope, struct_name, generate_bit_cast(), fully_qualified_element_type_name, pointer_ref));
            }

            buffer.append(std::format("auto* userdata = static_cast<{}::{}*>(lua_newuserdatauv(lua_state, sizeof({}::{}), 1));\n", fully_qualified_struct_scope, struct_name, fully_qualified_struct_scope, struct_name));
            buffer.append("lua_pushboolean(lua_state, false);\n");
            buffer.append("lua_setiuservalue(lua_state, -2, 1);\n");
            buffer.append(std::format("new(userdata) {}::{}{{array_wrapper}};\n", fully_qualified_struct_scope, struct_name));
            buffer.append(std::format("luaL_getmetatable(lua_state, \"{}_{}Metatable\");\n", scope_as_function_name(fully_qualified_struct_scope), struct_name));
            buffer.append("lua_setmetatable(lua_state, -2);\n");

            buffer.append(std::format("auto& param_{} = *std::bit_cast<::RC::Unreal::TArray<{}{}>*>(param_inter_{})",
                                      param_num,
                                      fully_qualified_element_type_name,
                                      pointer_ref,
                                      param_num));
        }
        //*/

        //std::string buffer{std::format("auto param_{} = std::bit_cast<::RC::Unreal::TArray<{}{}>*>(param_inter_{})", wrapped_struct_name, pointer_ref, param_num)};

        return buffer;
    }
}