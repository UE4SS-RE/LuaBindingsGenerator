#include <algorithm>
#include <stdexcept>
#include <type_traits>

#include <LuaWrapperGenerator/CodeGenerator.hpp>
#include <File/File.hpp>

namespace RC::LuaWrapperGenerator
{
    static std::vector<std::string> s_valid_metamethod_names{
            "__add",
            "__sub",
            "__mul",
            "__div",
            "__mod",
            "__pow",
            "__unm",
            "__idiv",
            "__band",
            "__bor",
            "__bxor",
            "__bnot",
            "__shl",
            "__shr",
            "__concat",
            "__len",
            "__eq",
            "__lt",
            "__le",
            "__index",
            "__newindex",
            "__call",
            "__gc",
            "__close",
    };

    auto scope_as_function_name(std::string_view scope) -> std::string
    {
        std::string scope_with_underscores{scope};
        std::transform(scope_with_underscores.cbegin(), scope_with_underscores.cend(), scope_with_underscores.begin(), [](unsigned char c) {
            return c == ':' ? '_' : c;
        });
        return scope_with_underscores;
    }

    auto get_scope_parts(std::string_view fully_qualified_scope, std::vector<std::string>& out_parts) -> void
    {
        size_t last_match{};
        while (true)
        {
            auto start_pos = fully_qualified_scope.find("::", last_match);
            if (start_pos == fully_qualified_scope.npos || start_pos + 2 > fully_qualified_scope.size()) { break; }
            start_pos += 2;
            auto end_pos = fully_qualified_scope.find("::", start_pos);
            out_parts.emplace_back(fully_qualified_scope.substr(start_pos, end_pos - start_pos));
            last_match = end_pos;
        }
    }

    template<typename ReturnType>
    auto find_class_by_name_internal(auto& self, std::string_view fully_qualified_scope, std::string_view name) -> ReturnType
    {
        //printf_s("\nClasses:\n");
        for (auto& the_class : self.classes)
        {
            //printf_s("Class: %s\n", the_class.second.name.c_str());
            if (the_class.second.name == name && the_class.second.fully_qualified_scope == fully_qualified_scope) { return &the_class.second; }
        }

        //printf_s("\nThin classes:\n");
        for (auto& the_class : self.thin_classes)
        {
            //printf_s("Thin class: %s\n", the_class.second.name.c_str());
            if (the_class.second.name == name && the_class.second.fully_qualified_scope == fully_qualified_scope) { return &the_class.second; }
        }

        return nullptr;
    }

    auto Container::find_class_by_name(std::string_view fully_qualified_scope, std::string_view name) const -> const Class*
    {
        return find_class_by_name_internal<const Class*>(*this, fully_qualified_scope, name);
    }

    auto Container::find_mutable_class_by_name(std::string_view fully_qualified_scope, std::string_view name) -> Class*
    {
        return find_class_by_name_internal<Class*>(*this, fully_qualified_scope, name);
    }

    auto CodeGenerator::add_class_to_container(const std::string& class_name, ClassContainer& container, const std::string& full_path_to_file, const std::string& fully_qualified_scope) -> Class&
    {
        auto[the_class, successfully_inserted] = container.insert({fully_qualified_scope + "::" + class_name,
                                                                          Class{*this,
                                                                                  class_name,
                                                                                  fully_qualified_scope,
                                                                                  full_path_to_file}});
        if (!successfully_inserted)
        {
            throw DoNotParseException{"a class by this name already exists",
                    !(class_name.empty() || class_name == ""),
                    &the_class->second};
        }

        return the_class->second;
    }

    auto add_function_to_container(FunctionContainer& container, const std::string& function_name, const std::string& parent_name, const std::string& full_path_to_file, const std::string& fully_qualified_scope, const std::string& parent_scope, Class* contained_in_class, IsFunctionScopeless is_function_scopeless) -> Function&
    {
        auto[function, _] = container.insert({is_function_scopeless == IsFunctionScopeless::No ? fully_qualified_scope + "::" + function_name : function_name,
                                                     Function{function_name,
                                                             parent_name,
                                                             full_path_to_file,
                                                             is_function_scopeless == IsFunctionScopeless::No ? fully_qualified_scope : "",
                                                             parent_scope,
                                                             nullptr,
                                                             contained_in_class}});

        return function->second;
    }

    static auto generate_cxx_call(const Function& function, bool generate_call_and_return_code = true) -> std::string
    {
        std::string buffer{};
        std::vector<std::string> recursion_resetters{};

        enum class IsWrappedInLambda { Yes, No };
        enum class GenerateReturnStatement { Yes, No };
        auto generate_function_tail = [&](const std::vector<FunctionParam>& params, IsWrappedInLambda is_wrapped_in_lambda = IsWrappedInLambda::No, GenerateReturnStatement generate_return_statement = GenerateReturnStatement::Yes) {
            bool return_value_is_void = function.get_return_type()->is_a<Type::Void>() && !function.get_return_type()->is_pointer();
            if (is_wrapped_in_lambda == IsWrappedInLambda::No)
            {
                if (!return_value_is_void)
                {
                    std::string pointer_ref{};
                    if (function.get_return_type()->is_pointer())
                    {
                        pointer_ref.append("*");
                    }
                    if (function.get_return_type()->is_ref())
                    {
                        pointer_ref.append("&");
                    }
                    buffer.append(std::format("        auto{} return_value = ", pointer_ref));
                }
            }

            if (generate_return_statement == GenerateReturnStatement::Yes && is_wrapped_in_lambda == IsWrappedInLambda::Yes)
            {
                buffer.append("                return ");
            }

            if (function.get_containing_class() && !function.is_static())
            {
                buffer.append(std::format("{}self->{}(", return_value_is_void ? "        " : "", function.get_name()));
            }
            else
            {
                if (function.is_constructor())
                {
                    buffer.append(std::format("{}{}(", return_value_is_void ? "        " : "", function.get_fully_qualified_scope()));
                }
                else
                {
                    buffer.append(std::format("{}{}::{}(", return_value_is_void ? "        " : "", function.get_fully_qualified_scope(), function.get_name()));
                }
            }

            for (size_t i = 0; i < params.size(); ++i)
            {
                const auto& param = params[i];
                buffer.append(std::format("param_{}", /*param.type->is_a<Type::CustomStruct>() && !param.type->is_pointer() ? "*" : "",*/ i + 1));

                if (i + 1 < params.size())
                {
                    buffer.append(", ");
                }
            }

            buffer.append(");\n\n");

            for (const auto recursion_resetter : recursion_resetters)
            {
                buffer.append(recursion_resetter);
            }
        };

        if (function.get_overloads().size() == 1)
        {
            const auto& params = function.get_overloads()[0];
            for (int i = 0; i < params.size(); ++i)
            {
                const auto& param = params[i];
                const auto lua_stack_index = i + 1;
                const auto lua_current_param = i + 2;
                //printf_s("name: %s, is_pointer: %s, is_ref: %s\n", param.name.c_str(), param.type->is_pointer() ? "true" : "false", param.type->is_ref() ? "true" : "false");

                if (!param.type->needs_extra_processing())
                {
                    buffer.append(std::format("        luaL_argcheck(lua_state, {}, {}, \"\");\n", param.type->generate_lua_stack_validation_condition(lua_stack_index), lua_current_param));
                }

                if (param.type->needs_extra_processing())
                {
                    buffer.append(std::format("{}", param.type->generate_extra_processing(lua_stack_index, lua_current_param)));
                }
                else if (!param.type->needs_conversion_from_lua())
                {
                    auto is_string = param.type->is_a<Type::CString>() || param.type->is_a<Type::CWString>() || param.type->is_a<Type::String>() || param.type->is_a<Type::WString>() || param.type->is_a<Type::AutoString>();
                    buffer.append(std::format("        auto{} param_{} = {};\n", !is_string && param.type->is_ref() ? "&" : "", lua_stack_index, param.type->generate_lua_stack_retriever(lua_stack_index)));
                }
                else
                {
                    buffer.append(std::format("        auto param_inter_{} = {};\n", lua_stack_index, param.type->generate_lua_stack_retriever(lua_stack_index)));
                    buffer.append(std::format("        {};\n", param.type->generate_converted_type(lua_stack_index, recursion_resetters)));
                }

                // We could check for non-nullptr userdata here for 'CustomStruct' types.
                // If we did that we'd be ignoring functions that don't necessarily care if the param is nullptr.
                // It may or may not be good design but it happens so I think we must support this.
                //if (param.type->is_a<Type::CustomStruct>())
                //{
                //    buffer.append(std::format("    luaL_argcheck(lua_state, param_{}, {}, \"test2\");\n", lua_stack_index, lua_current_param));
                //}

                buffer.append("\n");
            }

            if (generate_call_and_return_code)
            {
                generate_function_tail(params);
            }
        }
        else
        {
            buffer.append("        std::unordered_set<int> matching_overloads{};\n");
            buffer.append("        int num_matching_overloads{};\n");
            buffer.append("        auto num_params_on_stack = lua_gettop(lua_state);\n");
            for (int x = 0; x < function.get_overloads().size(); ++x)
            {
                buffer.append("        if (");
                const auto& param_overloads = function.get_overloads()[x];
                buffer.append(std::format("(num_params_on_stack == {}) &&\n", param_overloads.size()));
                for (int i = 0; i < param_overloads.size(); ++i)
                {
                    const auto& param = param_overloads[i];
                    const auto lua_stack_index = i + 1;
                    const auto lua_current_param = i + 2;

                    buffer.append(std::format("            ({})", param.type->generate_lua_stack_validation_condition(lua_stack_index)));
                    if (i + 1 < param_overloads.size())
                    {
                        buffer.append(" &&\n");
                    }
                }
                buffer.append(")\n");
                buffer.append("        {\n");
                for (int i = 0; i < param_overloads.size(); ++i)
                {
                    const auto& param = param_overloads[i];
                    const auto lua_stack_index = i + 1;
                    const auto lua_current_param = i + 2;
                    buffer.append(std::format("            bool param_overload_resolution_condition_{} = [=]() {{\n", lua_stack_index));
                    buffer.append(std::format("{}\n", param.type->generate_lua_overload_resolution_condition(lua_stack_index)));
                    buffer.append("            }();\n\n");
                }

                buffer.append("            if (");
                for (int i = 0; i < param_overloads.size(); ++i)
                {
                    const auto& param = param_overloads[i];
                    const auto lua_stack_index = i + 1;
                    const auto lua_current_param = i + 2;

                    buffer.append(std::format("{}param_overload_resolution_condition_{}", i == 0 ? "" : "                ", lua_stack_index));
                    if (i + 1 < param_overloads.size())
                    {
                        buffer.append(" &&\n");
                    }
                }

                buffer.append(")\n");
                buffer.append("            {\n");
                buffer.append(std::format("                matching_overloads.emplace({});\n", x));
                buffer.append("            }\n");

                buffer.append("        }\n\n");
            }

            buffer.append("        if (matching_overloads.size() > 1)\n");
            buffer.append("        {\n");
            buffer.append(std::format("            luaL_error(lua_state, \"Ambiguous overload for function '{}' (no overload was specific enough to match the parameters)\");\n", function.get_name()));
            buffer.append("        }\n");
            buffer.append("        else if (matching_overloads.empty())\n");
            buffer.append("        {\n");
            buffer.append(std::format("            luaL_error(lua_state, \"No overload found for function '{}'\");\n", function.get_name()));
            buffer.append("        }\n\n");

            buffer.append("        auto selected_overload = *matching_overloads.begin();\n\n");

            std::string pointer_ref{};
            if (function.get_return_type()->is_pointer())
            {
                pointer_ref.append("*");
            }
            if (function.get_return_type()->is_ref())
            {
                pointer_ref.append("&");
            }
            buffer.append(std::format("        {}[=]() {{\n", !function.get_return_type()->is_a<Type::Void>() || function.get_return_type()->is_pointer() ? std::format("auto{} return_value = ", pointer_ref) : ""));
            for (size_t x = 0; x < function.get_overloads().size(); ++x)
            {
                const auto& param_overloads = function.get_overloads()[x];

                if (x == 0)
                {
                    buffer.append(std::format("            if (selected_overload == {})\n", x));
                }
                else
                {
                    buffer.append(std::format("            else if (selected_overload == {})\n", x));
                }
                buffer.append("            {\n");

                for (int i = 0; i < param_overloads.size(); ++i)
                {
                    const auto& param = param_overloads[i];
                    const auto lua_stack_index = i + 1;
                    const auto lua_current_param = i + 2;

                    if (!param.type->needs_extra_processing())
                    {
                        buffer.append(std::format("                luaL_argcheck(lua_state, {}, {}, \"\");\n", param.type->generate_lua_stack_validation_condition(lua_stack_index), lua_current_param));
                    }

                    if (param.type->needs_extra_processing())
                    {
                        buffer.append(std::format("{}", param.type->generate_extra_processing(lua_stack_index, lua_current_param)));
                    }
                    else if (!param.type->needs_conversion_from_lua())
                    {
                        buffer.append(std::format("                auto{} param_{} = {};\n", param.type->is_ref() ? "&" : "", lua_stack_index, param.type->generate_lua_stack_retriever(lua_stack_index)));
                    }
                    else
                    {
                        buffer.append(std::format("                auto param_inter_{} = {};\n", lua_stack_index, param.type->generate_lua_stack_retriever(lua_stack_index)));
                        buffer.append(std::format("                {};\n", param.type->generate_converted_type(lua_stack_index, recursion_resetters)));
                    }
                }

                if (generate_call_and_return_code)
                {
                    generate_function_tail(param_overloads, IsWrappedInLambda::Yes, !function.get_return_type()->is_a<Type::Void>() || function.get_return_type()->is_pointer() ? GenerateReturnStatement::Yes : GenerateReturnStatement::No);
                }
                buffer.append("            }\n");
            }
            buffer.append("            else\n");
            buffer.append("            {\n");
            buffer.append("                luaL_error(lua_state, \"Overload resolution failed and wasn't caught\");\n");
            // Must throw here otherwise the compiler gives a warning because it can't see the jmp from luaL_error.
            buffer.append("                throw std::runtime_error{\"\"};\n");
            buffer.append("            }\n");
            buffer.append("        }();\n\n");
        }

        return buffer;
    }

    namespace Type
    {
        auto generate_static_class_types(const Container& container) -> void
        {
            BaseTemplate<EmptyBaseType>::static_class = std::make_unique<EmptyBaseType>(container);
            Void::static_class = std::make_unique<Void>(container);
            NumericBase::static_class = std::make_unique<EmptyBaseType>(container);
            Int8::static_class = std::make_unique<Int8>(container);
            Int16::static_class = std::make_unique<Int16>(container);
            Int32::static_class = std::make_unique<Int32>(container);
            Int64::static_class = std::make_unique<Int64>(container);
            UInt8::static_class = std::make_unique<UInt8>(container);
            UInt16::static_class = std::make_unique<UInt16>(container);
            UInt32::static_class = std::make_unique<UInt32>(container);
            UInt64::static_class = std::make_unique<UInt64>(container);
            Float::static_class = std::make_unique<Float>(container);
            Double::static_class = std::make_unique<Double>(container);
            StringBase::static_class = std::make_unique<EmptyBaseType>(container);
            CString::static_class = std::make_unique<CString>(container);
            CWString::static_class = std::make_unique<CWString>(container);
            String::static_class = std::make_unique<String>(container);
            WString::static_class = std::make_unique<WString>(container);
            AutoString::static_class = std::make_unique<AutoString>(container);
            CustomStruct::static_class = std::make_unique<CustomStruct>(container);
            Enum::static_class = std::make_unique<Enum>(container);
            Bool::static_class = std::make_unique<Bool>(container);
            FunctionProto::static_class = std::make_unique<FunctionProto>(container, "Base");
        }

        auto Void::get_fully_qualified_type_name() const -> std::string
        {
            return std::format("void{}{}", is_pointer() ? "*" : "", is_ref() ? "&" : "");
        }

        auto Void::generate_lua_stack_validation_condition(int stack_index) const -> std::string
        {
            if (is_ref())
            {
                return std::format("lua_islightuserdata(lua_state, {}) || lua_isuserdata(lua_state, {})", stack_index, stack_index);
            }
            else if (is_pointer())
            {
                return std::format("lua_islightuserdata(lua_state, {}) || lua_isnil(lua_state, {}) || lua_isuserdata(lua_state, {})", stack_index, stack_index, stack_index);
            }
            else
            {
                return std::format("lua_isnil(lua_state, {})", stack_index);
            }
        }

        auto Void::generate_lua_stack_retriever(int stack_index) const -> std::string
        {
            if (is_ref())
            {
                return std::format("*static_cast<void**>(lua_touserdata(lua_state, {}))", stack_index, stack_index);
            }
            else if (is_pointer())
            {
                return std::format("lua_isnil(lua_state, {}) ? nullptr : lua_touserdata(lua_state, {})", stack_index, stack_index);
            }
            else
            {
                throw std::runtime_error{"Direct call to 'Void::generate_lua_stack_retriever' not allowed"};
            }
        }

        auto Void::generate_lua_stack_pusher(std::string_view variable_to_push, std::string_view param_prefix) const -> std::string
        {
            if (is_pointer())
            {
                return std::format("        lua_pushlightuserdata({}lua_state, {})", param_prefix, variable_to_push);
            }
            else
            {
                throw std::runtime_error{"Direct call to 'Void::generate_lua_stack_pusher' not allowed"};
            }
        }

        auto Int8::get_fully_qualified_type_name() const -> std::string
        {
            return std::format("int8_t{}{}", is_pointer() ? "*" : "", is_ref() ? "&" : "");
        }

        auto Int16::get_fully_qualified_type_name() const -> std::string
        {
            return std::format("int16_t{}{}", is_pointer() ? "*" : "", is_ref() ? "&" : "");
        }

        auto Int32::get_fully_qualified_type_name() const -> std::string
        {
            return std::format("int32_t{}{}", is_pointer() ? "*" : "", is_ref() ? "&" : "");
        }

        auto Int64::get_fully_qualified_type_name() const -> std::string
        {
            return std::format("int64_t{}{}", is_pointer() ? "*" : "", is_ref() ? "&" : "");
        }

        auto UInt8::get_fully_qualified_type_name() const -> std::string
        {
            return std::format("uint8_t{}{}", is_pointer() ? "*" : "", is_ref() ? "&" : "");
        }

        auto UInt16::get_fully_qualified_type_name() const -> std::string
        {
            return std::format("uint16_t{}{}", is_pointer() ? "*" : "", is_ref() ? "&" : "");
        }

        auto UInt32::get_fully_qualified_type_name() const -> std::string
        {
            return std::format("uint32_t{}{}", is_pointer() ? "*" : "", is_ref() ? "&" : "");
        }

        auto UInt64::get_fully_qualified_type_name() const -> std::string
        {
            return std::format("uint64_t{}{}", is_pointer() ? "*" : "", is_ref() ? "&" : "");
        }

        auto Float::get_fully_qualified_type_name() const -> std::string
        {
            return std::format("float{}{}", is_pointer() ? "*" : "", is_ref() ? "&" : "");
        }

        auto Double::get_fully_qualified_type_name() const -> std::string
        {
            return std::format("double{}{}", is_pointer() ? "*" : "", is_ref() ? "&" : "");
        }

        auto CString::get_fully_qualified_type_name() const -> std::string
        {
            return std::format("char{}{}", is_pointer() ? "*" : "", is_ref() ? "&" : "");
        }
        auto CString::generate_lua_stack_retriever(int stack_index) const -> std::string
        {
            return std::format("lua_tostring(lua_state, {})", stack_index);
        }
        auto CString::generate_lua_stack_pusher(std::string_view variable_to_push, std::string_view param_prefix) const -> std::string
        {
            return std::format("        lua_pushstring({}lua_state, {})", param_prefix, variable_to_push);
        }
        auto CString::generate_converted_type(size_t param_num, std::vector<std::string>& recursion_resetters) const -> std::string
        {
            return std::format("auto param_ansi_{}{{param_inter_{}}}; auto param_wide_{} = std::string{{param_ansi_{}}};\n                auto param_{} = {}{{param_wide_{}.begin(), param_wide_{}.end()}}",
                               param_num,
                               param_num,
                               param_num,
                               param_num,
                               param_num,
                               generate_cxx_name(),
                               param_num,
                               param_num);
        }

        auto CWString::get_fully_qualified_type_name() const -> std::string
        {
            return std::format("wchar_t{}{}", is_pointer() ? "*" : "", is_ref() ? "&" : "");
        }
        auto CWString::generate_lua_stack_retriever(int stack_index) const -> std::string
        {
            return std::format("lua_tostring(lua_state, {})", stack_index);
        }
        auto CWString::generate_lua_stack_pusher(std::string_view variable_to_push, std::string_view param_prefix) const -> std::string
        {
            return std::format("        lua_pushstring({}lua_state, ::RC::to_string({}).c_str())", param_prefix, variable_to_push);
        }
        auto CWString::generate_converted_type(size_t param_num, std::vector<std::string>& recursion_resetters) const -> std::string
        {
            return std::format("auto param_ansi_{} = std::string{{param_inter_{}}};\n                auto param_wide_{} = std::wstring{{param_ansi_{}.begin(), param_ansi_{}.end()}};\n                auto param_{} = param_wide_{}.c_str()",
                               param_num,
                               param_num,
                               param_num,
                               param_num,
                               param_num,
                               param_num,
                               param_num);
        }

        auto String::get_fully_qualified_type_name() const -> std::string
        {
            return std::format("std::string{}{}", is_pointer() ? "*" : "", is_ref() ? "&" : "");
        }
        auto String::generate_lua_stack_retriever(int stack_index) const -> std::string
        {
            return std::format("{}{{lua_tostring(lua_state, {})}}", generate_cxx_name(), stack_index);
        }
        auto String::generate_lua_stack_pusher(std::string_view variable_to_push, std::string_view param_prefix) const -> std::string
        {
            return std::format("        lua_pushstring({}lua_state, {}.c_str())", param_prefix, variable_to_push);
        }
        auto String::generate_converted_type(size_t param_num, std::vector<std::string>& recursion_resetters) const -> std::string
        {
            return std::format("auto param_wide_{} = std::string{{param_inter_{}}};\n                auto param_{} = {}{{param_wide_{}.begin(), param_wide_{}.end()}}",
                               param_num,
                               param_num,
                               param_num,
                               generate_cxx_name(),
                               param_num,
                               param_num);
        }

        auto WString::get_fully_qualified_type_name() const -> std::string
        {
            return std::format("std::wstring{}{}", is_pointer() ? "*" : "", is_ref() ? "&" : "");
        }
        auto WString::generate_lua_stack_retriever(int stack_index) const -> std::string
        {
            return std::format("lua_tostring(lua_state, {})", stack_index);
        }
        auto WString::generate_lua_stack_pusher(std::string_view variable_to_push, std::string_view param_prefix) const -> std::string
        {
            return std::format("        lua_pushstring({}lua_state, ::RC::to_string({}).c_str())", param_prefix, variable_to_push);
        }
        auto WString::generate_converted_type(size_t param_num, std::vector<std::string>& recursion_resetters) const -> std::string
        {
            return std::format("auto param_ansi_{} = std::string{{param_inter_{}}};\n                auto param_{} = {}{{param_ansi_{}.begin(), param_ansi_{}.end()}}",
                               param_num,
                               param_num,
                               param_num,
                               generate_cxx_name(),
                               param_num,
                               param_num);
        }

        auto AutoString::get_fully_qualified_type_name() const -> std::string
        {
            return std::format("::RC::File::StringType{}{}", is_pointer() ? "*" : "", is_ref() ? "&" : "");
        }
        auto AutoString::generate_lua_stack_retriever(int stack_index) const -> std::string
        {
            return std::format("lua_tostring(lua_state, {})", stack_index);
        }
        auto AutoString::generate_lua_stack_pusher(std::string_view variable_to_push, std::string_view param_prefix) const -> std::string
        {
            if (m_is_wide_string)
            {
                return std::format("        lua_pushstring({}lua_state, ::RC::to_string({}).c_str())", param_prefix, variable_to_push);
            }
            else
            {
                return std::format("        lua_pushstring({}lua_state, {}.c_str())", this->generate_cxx_name(), param_prefix, variable_to_push);
            }
        }
        auto AutoString::generate_converted_type(size_t param_num, std::vector<std::string>& recursion_resetters) const -> std::string
        {
            // TODO: This doesn't work properly if 'File::StringType' evaluates to std::string instead of std::wstring.
            //       The solution is probably to remove 'AutoString' and instead figure out the underlying type.
            return std::format("auto param_unknown_{} = std::string{{param_inter_{}}};\n                auto param_{} = {}{{param_unknown_{}.begin(), param_unknown_{}.end()}}",
                               param_num,
                               param_num,
                               param_num,
                               generate_cxx_name(),
                               param_num,
                               param_num);
        }

        auto CustomStruct::get_fully_qualified_type_name() const -> std::string
        {
            return std::format("{}::{}{}{}", get_fully_qualified_scope(), m_type_name, is_pointer() ? "*" : "", is_ref() ? "&" : "");
        }
        auto CustomStruct::generate_cxx_name() const -> std::string
        {
            return m_type_name;
        }
        auto CustomStruct::generate_lua_stack_validation_condition(int stack_index) const -> std::string
        {
            if (is_pointer())
            {
                return std::format("lua_isuserdata(lua_state, {}) || lua_isnil(lua_state, {})", stack_index, stack_index);
            }
            else
            {
                return std::format("lua_isuserdata(lua_state, {})", stack_index);
            }
        }
        auto CustomStruct::generate_lua_overload_resolution_condition(int stack_index) const -> std::string
        {
            std::string buffer{"                bool matches_overload{};\n"};

            buffer.append(std::format("                if (lua_isuserdata(lua_state, {}))\n", stack_index));
            buffer.append("                {\n");
            buffer.append(std::format("                    lua_getmetatable(lua_state, {});\n", stack_index));
            buffer.append("                    lua_pushliteral(lua_state, \"__name\");\n");
            buffer.append("                    lua_rawget(lua_state, -2);\n");
            buffer.append("                    auto metatable_name = std::string{lua_tostring(lua_state, -1)};\n");
            buffer.append(std::format("                    if (convertible_to_{}_{}.contains(metatable_name))\n", scope_as_function_name(get_fully_qualified_scope()), m_type_name));
            buffer.append("                    {\n");
            buffer.append("                        matches_overload = true;\n");
            buffer.append("                    }\n");
            buffer.append("                }\n");
            buffer.append("                else // value is nil (treat as nullptr)\n");
            buffer.append("                {\n");
            buffer.append("                    matches_overload = true;\n");
            buffer.append("                }\n");
            buffer.append("                if (matches_overload)\n");
            buffer.append("                {\n");
            buffer.append(std::format("                    return true;\n"));
            buffer.append("                }\n");
            buffer.append("                else\n");
            buffer.append("                {\n");
            buffer.append(std::format("                    return false;\n"));
            buffer.append("                }");

            return buffer;
        }
        auto CustomStruct::generate_lua_stack_retriever(int stack_index) const -> std::string
        {
            auto* the_class = get_container().find_class_by_name(get_fully_qualified_scope(), m_type_name);
            if (!the_class)
            {
                // This happens if the type is forward declared and never actually fully defined.
                // TODO: These have not been forward declared in this file and relies on the forward declaration being included which I haven't confirmed always happens.
                if (is_struct_forward_declaration())
                {
                    if (is_pointer())
                    {
                        return std::format("lua_isnil(lua_state, {}) ? nullptr : static_cast<{}::{}*>(luaL_checkudata(lua_state, {}, \"{}_{}Metatable\"))", stack_index, get_fully_qualified_scope(), m_type_name, stack_index, scope_as_function_name(get_fully_qualified_scope()), m_type_name);
                    }
                    else
                    {
                        return std::format("*static_cast<{}::{}*>(luaL_checkudata(lua_state, {}, \"{}_{}Metatable\"))", get_fully_qualified_scope(), m_type_name, stack_index, scope_as_function_name(get_fully_qualified_scope()), m_type_name);
                    }
                }
                else if (is_class_forward_declaration())
                {
                    if (is_pointer())
                    {
                        return std::format("lua_isnil(lua_state, {}) ? nullptr : static_cast<{}::{}*>(luaL_checkudata(lua_state, {}, \"{}_{}Metatable\"))", stack_index, get_fully_qualified_scope(), m_type_name, stack_index, scope_as_function_name(get_fully_qualified_scope()), m_type_name);
                    }
                    else
                    {
                        return std::format("*static_cast<{}::{}*>(luaL_checkudata(lua_state, {}, \"{}_{}Metatable\"))", get_fully_qualified_scope(), m_type_name, stack_index, scope_as_function_name(get_fully_qualified_scope()), m_type_name);
                    }
                }
                else
                {
                    throw std::runtime_error{std::format("Was unable to find class or struct for type '{}'", m_type_name)};
                }
            }
            else
            {
                if (is_pointer())
                {
                    return std::format("lua_isnil(lua_state, {}) ? nullptr : static_cast<{}::{}*>(luaL_checkudata(lua_state, {}, \"{}_{}Metatable\"))", stack_index, the_class->fully_qualified_scope, m_type_name, stack_index, scope_as_function_name(get_fully_qualified_scope()), m_type_name);
                }
                else
                {
                    return std::format("*static_cast<{}::{}*>(luaL_checkudata(lua_state, {}, \"{}_{}Metatable\"))", the_class->fully_qualified_scope, m_type_name, stack_index, scope_as_function_name(get_fully_qualified_scope()), m_type_name);
                }
            }
        }
        auto CustomStruct::generate_lua_stack_pusher(std::string_view variable_to_push, std::string_view param_prefix) const -> std::string
        {
            auto fully_qualified_scope_and_type_name = m_fully_qualified_scope + "::" + m_type_name;
            auto* owner = get_container().find_class_by_name(m_fully_qualified_scope, m_type_name);
            if (!owner)
            {
                throw std::runtime_error{std::format("[CustomStruct::generate_lua_stack_pusher] Was unable to find an owner to type '{}'", fully_qualified_scope_and_type_name)};
            }

            std::string buffer{};
            if (is_pointer())
            {
                buffer.append(std::format("        auto* userdata = static_cast<{}::{}*>(lua_newuserdatauv({}lua_state, sizeof({}::{}*), 1));\n", owner->fully_qualified_scope, owner->name, param_prefix, owner->fully_qualified_scope, owner->name));
                buffer.append(std::format("        lua_pushinteger({}lua_state, 1);\n", param_prefix));
            }
            else
            {
                buffer.append(std::format("        auto* userdata = static_cast<{}::{}*>(lua_newuserdatauv({}lua_state, sizeof({}::{}), 1));\n", owner->fully_qualified_scope, owner->name, param_prefix, owner->fully_qualified_scope, owner->name));
                buffer.append(std::format("        lua_pushinteger({}lua_state, 0);\n", param_prefix));
            }
            buffer.append(std::format("        lua_setiuservalue({}lua_state, -2, 1);\n", param_prefix));

            std::string variable_to_construct{};
            if (should_move_on_construction())
            {
                variable_to_construct.append(std::format("std::move({})", variable_to_push));
            }
            else
            {
                variable_to_construct.append(std::format("{}", variable_to_push));
            }

            if (is_pointer())
            {
                buffer.append(std::format("        new(userdata) {}{}::{}*{{{}}};\n", is_const() ? "const " : "", owner->fully_qualified_scope, owner->name, variable_to_construct));
            }
            else
            {
                buffer.append(std::format("        new(userdata) {}{}::{}{{{}}};\n", is_const() ? "const " : "", owner->fully_qualified_scope, owner->name, variable_to_construct));
            }

            buffer.append(std::format("        luaL_getmetatable({}lua_state, \"{}_{}Metatable\");\n", param_prefix, scope_as_function_name(get_fully_qualified_scope()), owner->name));
            buffer.append(std::format("        lua_setmetatable({}lua_state, -2)", param_prefix));

            return buffer;
        }

        auto CustomStruct::generate_extra_processing(int stack_index, int current_param) const -> std::string
        {
            /*
            std::string buffer{};

            buffer.append(std::format("        {}::{}** param_{}_container{{}};\n", get_fully_qualified_scope(), m_type_name, stack_index));
            buffer.append(std::format("        {}::{}* param_{}_ptr{{}};\n\n", get_fully_qualified_scope(), m_type_name, stack_index));

            buffer.append(std::format("        if (!lua_isnil(lua_state, {}))\n", stack_index));
            buffer.append("        {\n");

            buffer.append(std::format("            lua_getiuservalue(lua_state, {}, 1);\n", stack_index));
            buffer.append(std::format("            int param_{}_pointer_depth = lua_tointeger(lua_state, -1);\n", stack_index));
            buffer.append(std::format("            bool param_{}_is_pointer = param_{}_pointer_depth > 0;\n\n", stack_index, stack_index));

            buffer.append(std::format("            lua_getmetatable(lua_state, {});\n", stack_index));
            buffer.append("            lua_pushliteral(lua_state, \"__name\");\n");
            buffer.append("            lua_rawget(lua_state, -2);\n");
            buffer.append(std::format("            auto param_{}_metatable_name = std::string{{lua_tostring(lua_state, -1)}};\n", stack_index));
            buffer.append(std::format("            auto param_{}_bad_self_error_message = std::format(\"self was '{{}}', expected '{}_{}Metatable' or derivative\", param_{}_metatable_name);\n", stack_index, scope_as_function_name(get_fully_qualified_scope()), m_type_name, stack_index));
            buffer.append(std::format("            luaL_argcheck(lua_state, convertible_to_{}_{}.contains(param_{}_metatable_name), 1, param_{}_bad_self_error_message.c_str());\n\n", scope_as_function_name(get_fully_qualified_scope()), m_type_name, stack_index, stack_index));

            buffer.append("            lua_pop(lua_state, 3);\n\n");

            buffer.append(std::format("            if (param_{}_is_pointer)\n            {{\n", stack_index));
            buffer.append(std::format("                auto* param_{}_outer_most_container = lua_touserdata(lua_state, 1);\n", stack_index));
            buffer.append(std::format("                param_{}_container = static_cast<{}::{}**>(deref(param_{}_outer_most_container, param_{}_pointer_depth - 1));\n", stack_index, get_fully_qualified_scope(), m_type_name, stack_index, stack_index));
            buffer.append(std::format("                param_{}_ptr = *param_{}_container;\n", stack_index, stack_index));
            buffer.append("            }\n");

            buffer.append("        }\n");
            buffer.append(std::format("        auto{} param_{} = {}param_{}_ptr;\n", is_ref() ? "&" : "", stack_index, !is_pointer() ? "*" : "", stack_index));

            return buffer;
            //*/

            // auto& param_1 = lua_util_userdata_Get<"__RC__Unreal_FFieldClassVariantMetatable", ::RC::Unreal::FFieldClassVariant&, convertible_to___RC__Unreal_FFieldClassVariant>(lua_state, 1);
            std::string buffer{};
            std::string pointer_ref{};
            if (is_pointer())
            {
                pointer_ref.append("*");
            }
            if (is_ref())
            {
                pointer_ref.append("&");
            }
            buffer.append(std::format("        auto{} param_{} = lua_util_userdata_Get<\"{}_{}Metatable\", {}::{}{}, convertible_to_{}_{}>(lua_state, {});\n",
                                      is_ref() ? "&" : "",
                                      stack_index,
                                      scope_as_function_name(get_fully_qualified_scope()),
                                      m_type_name,
                                      get_fully_qualified_scope(),
                                      m_type_name,
                                      pointer_ref,
                                      scope_as_function_name(get_fully_qualified_scope()),
                                      m_type_name,
                                      stack_index));
            return buffer;
        }

        auto FunctionProto::get_fully_qualified_type_name() const -> std::string
        {
            return m_function_proto;
        }
        auto FunctionProto::generate_cxx_name() const -> std::string
        {
            throw std::runtime_error{"FunctionProto::generate_cxx_name: FIXME"};
        }
        auto FunctionProto::generate_lua_stack_validation_condition(int stack_index) const -> std::string
        {
            return std::format("lua_isfunction(lua_state, {}) || lua_isuserdata(lua_state, {}) || lua_isinteger(lua_state, {}) || lua_isnil(lua_state, {})", stack_index, stack_index, stack_index, stack_index);
        }
        auto FunctionProto::generate_lua_stack_retriever(int stack_index) const -> std::string
        {
            std::string buffer{};
            buffer.append("[&]() {\n");
            buffer.append(std::format("        if (lua_isfunction(lua_state, {}))\n", stack_index));
            buffer.append("        {\n");
            buffer.append("            return luaL_ref(lua_state, LUA_REGISTRYINDEX);\n");
            buffer.append("        }\n");
            buffer.append("        else\n");
            buffer.append("        {\n");
            buffer.append("            return 0;\n");
            buffer.append("        }\n");
            buffer.append("}();\n");
            return buffer;
        }
        auto FunctionProto::generate_converted_type(size_t param_num, std::vector<std::string>& recursion_resetters) const -> std::string
        {
            std::string buffer{};
            std::string param_prefix{};

            if (!has_storage())
            {
                param_prefix = std::format("lambda_params_{}.", param_num);

                buffer.append(std::format("struct LambdaParams_{}\n", param_num));
                buffer.append("        {\n");
                buffer.append("            lua_State* lua_state;\n");
                buffer.append(std::format("            int param_inter_{};\n", param_num));
                buffer.append("        };\n");
                buffer.append(std::format("        static std::vector<LambdaParams_{}> static_lambda_params_{}{{}};\n", param_num, param_num));
                buffer.append(std::format("        static LambdaParams_{} lambda_params_{}{{}};\n", param_num, param_num));
                buffer.append(std::format("        lambda_params_{} = static_lambda_params_{}.emplace_back(LambdaParams_{}{{lua_state, param_inter_{}}});\n", param_num, param_num, param_num, param_num));
            }


            buffer.append(std::format("auto param_function_ref_{} = [{}](", param_num, has_storage() ? "=" : "&"));

            for (size_t i = 0; i < m_param_types.size(); ++i)
            {
                const auto& param_type = m_param_types[i];
                std::string pointer_ref{};
                if (param_type->is_pointer())
                {
                    pointer_ref.append("*");
                }
                if (param_type->is_ref())
                {
                    pointer_ref.append("&");
                }
                buffer.append(std::format("{}{}{} lambda_param_{}", param_type->is_const() ? "const " : "", param_type->generate_cxx_name(), pointer_ref, i + 1));
                if (i + 1 < m_param_types.size()) { buffer.append(", "); }
            }

            std::string return_type_pointer_ref{};
            if (m_return_type->is_pointer())
            {
                return_type_pointer_ref.append("*");
            }
            if (m_return_type->is_ref())
            {
                return_type_pointer_ref.append("&");
            }
            buffer.append(std::format(") -> {}{}{} {{\n", m_return_type->is_const() ? "const " : "", m_return_type->generate_cxx_name(), return_type_pointer_ref));
            buffer.append(std::format("            if (lua_rawgeti({}lua_state, LUA_REGISTRYINDEX, {}param_inter_{}) != LUA_TFUNCTION)\n", param_prefix, param_prefix, param_num));
            buffer.append("            {\n");
            buffer.append(std::format("                luaL_error({}lua_state, std::format(\"Expected 'function' got '{{}}'\", lua_typename({}lua_state, -1)).c_str());\n", param_prefix, param_prefix));
            buffer.append("            }\n");
            buffer.append("            \n");
            for (size_t i = 0; i < m_param_types.size(); ++i)
            {
                const auto& param_type = m_param_types[i];
                buffer.append("            {\n");
                buffer.append(std::format("{};\n", param_type->generate_lua_stack_pusher(std::format("lambda_param_{}", i + 1), param_prefix)));
                buffer.append("            }\n\n");
            }
            buffer.append("        \n");
            buffer.append(std::format("            if (int status = lua_pcall({}lua_state, {}, {}, 0); status != LUA_OK)\n", param_prefix, m_param_types.size(), m_return_type->is_a<Void>() && !m_return_type->is_pointer() ? 0 : 1));
            buffer.append("            {\n");
            buffer.append(std::format("                throw std::runtime_error(std::format(\"lua_pcall returned {{}}\", resolve_status_message({}lua_state, status)));\n", param_prefix));
            buffer.append("            }\n");
            buffer.append("        \n");
            if (!m_return_type->is_a<Void>() || m_return_type->is_pointer())
            {
                buffer.append(std::format("            if ({})\n", m_return_type->generate_lua_stack_validation_condition(-1)));
                buffer.append("            {\n");
                buffer.append(std::format("                return {};\n", m_return_type->generate_lua_stack_retriever(-1)));
                buffer.append("            }\n");
                buffer.append("            else\n");
                buffer.append("            {\n");
                buffer.append(std::format("                return {}{{}};\n", m_return_type->generate_cxx_name()));
                buffer.append("            }\n");
                // TODO: Deal with nothing being returned for a non-void function.
                //       This doesn't necessarily need be to supported.
            }
            buffer.append("        };\n");

            if (has_storage())
            {
                buffer.append(std::format("    using CXXFuncSignature = std::function<{}>;\n", generate_function_signature(false)));
            }
            else
            {
                buffer.append(std::format("    using CXXFuncSignature = {};\n", generate_function_signature(true)));
            }
            buffer.append(std::format("    CXXFuncSignature param_{};\n", param_num));
            buffer.append(std::format("    if (lua_isuserdata(lua_state, {}))\n", param_num));
            buffer.append("    {\n");
            buffer.append(std::format("        auto function_proto = static_cast<FunctionProto*>(luaL_checkudata(lua_state, {}, \"FunctionProtoMetatable\"));\n", param_num));
            buffer.append(std::format("        param_{} = std::bit_cast<{}>(function_proto->function_pointer);\n", param_num, generate_function_signature(true)));
            buffer.append("    }\n");
            buffer.append(std::format("    else if (lua_isinteger(lua_state, {}))\n", param_num));
            buffer.append("    {\n");
            buffer.append(std::format("        param_{} = std::bit_cast<{}>(lua_tointeger(lua_state, {}));\n", param_num, generate_function_signature(true), param_num));
            buffer.append("    }\n");
            buffer.append(std::format("    else if (lua_isnil(lua_state, {}))\n", param_num));
            buffer.append("    {\n");
            buffer.append(std::format("        param_{} = std::bit_cast<{}>(nullptr);\n", param_num, generate_function_signature(true), param_num));
            buffer.append("    }\n");
            buffer.append("    else\n");
            buffer.append("    {\n");
            if (has_storage())
            {
                buffer.append(std::format("            param_{} = param_function_ref_{};\n", param_num, param_num));
            }
            else
            {
                buffer.append(std::format("                param_{} = fnptr<{}>(param_function_ref_{});\n", param_num, generate_function_signature(false), param_num));
            }
            buffer.append("    }\n");

            if (!has_storage())
            {
                std::string recursion_resetter{};
                recursion_resetter.append(std::format("        static_lambda_params_{}.pop_back();\n", param_num));
                recursion_resetter.append(std::format("        if (!static_lambda_params_{}.empty())\n", param_num));
                recursion_resetter.append("        {\n");
                recursion_resetter.append(std::format("            lambda_params_{} = static_lambda_params_{}.back();\n", param_num, param_num));
                recursion_resetter.append("        }\n");
                recursion_resetters.emplace_back(std::move(recursion_resetter));
            }

            return buffer;
        }
        auto FunctionProto::generate_lua_stack_pusher(std::string_view variable_to_push, std::string_view param_prefix) const -> std::string
        {
            std::string buffer{};
            buffer.append("        auto userdata = lua_newuserdatauv(lua_state, sizeof(FunctionProto), 0);\n");
            buffer.append(std::format("        new(userdata) FunctionProto{{std::bit_cast<void*>({}), &{}}};\n", variable_to_push, generate_function_signature_as_function_name()));
            buffer.append("        luaL_getmetatable(lua_state, \"FunctionProtoMetatable\");\n");
            buffer.append("        lua_setmetatable(lua_state, -2);\n");
            return buffer;
        }
        auto FunctionProto::generate_function_signature(bool real_function_pointer) const -> std::string
        {
            std::string function_signature{};
            function_signature.append(std::format("{}{}(", m_return_type->generate_cxx_name(), real_function_pointer ? "(*)" : ""));
            for (size_t i = 0; i < m_param_types.size(); ++i)
            {
                const auto& param_type = m_param_types[i];
                function_signature.append(std::format("{}{}", param_type->is_const() ? "const " : "", param_type->get_fully_qualified_type_name()));
                if (i + 1 < m_param_types.size()) { function_signature.append(", "); }
            }
            function_signature.append(")");
            return function_signature;
        }
        auto FunctionProto::generate_function_signature_as_function_name() const -> std::string
        {
            // Format: Ret_ReturnType_Params_ParamType_ParamType_ParamType_...
            std::string function_signature{};
            function_signature.append(std::format("Ret_{}_Params_", scope_as_function_name(m_return_type->generate_cxx_name())));
            for (size_t i = 0; i < m_param_types.size(); ++i)
            {
                const auto& param_type = m_param_types[i];
                std::string pointer_ref{};
                if (param_type->is_pointer())
                {
                    pointer_ref.append("Ptr");
                }
                if (param_type->is_ref())
                {
                    pointer_ref.append("Ref");
                }
                function_signature.append(std::format("{}{}{}", param_type->is_const() ? "Const" : "", scope_as_function_name(param_type->generate_cxx_name()), pointer_ref));
                if (i + 1 < m_param_types.size()) { function_signature.append("_"); }
            }
            return function_signature;
        }
        auto FunctionProto::generate_lua_wrapper_function() const -> std::string
        {
            std::string buffer{};
            buffer.append(std::format("inline auto {}(lua_State* lua_state) -> int\n", generate_function_signature_as_function_name()));
            buffer.append("{\n");
            buffer.append("    try\n    {\n");

            buffer.append("        // Prologue\n");
            buffer.append("        luaL_argcheck(lua_state, lua_isuserdata(lua_state, 1), 1, \"first param for 'FunctionProto' was not userdata\");\n");
            buffer.append("        lua_getmetatable(lua_state, 1);\n");
            buffer.append("        lua_pushliteral(lua_state, \"__name\");\n");
            buffer.append("        lua_rawget(lua_state, -2);\n");
            buffer.append("        auto metatable_name = std::string{lua_tostring(lua_state, -1)};\n");
            buffer.append("        if (metatable_name != \"FunctionProtoMetatable\") { luaL_error(lua_state, \"self was '{}', expected FunctionProtoMetatable\"); }\n");
            buffer.append("        auto function_proto = static_cast<FunctionProto*>(lua_touserdata(lua_state, 1));\n");
            buffer.append("        lua_pop(lua_state, 2);\n");
            buffer.append("        lua_remove(lua_state, 1);\n\n");

            buffer.append("        // Native call\n");
            buffer.append(generate_cxx_call(get_function(), false));
            buffer.append("\n");
            buffer.append(std::format("    std::bit_cast<{}>(function_proto->function_pointer)(", generate_function_signature(true)));
            const auto& params = get_function().get_overloads()[0];
            for (size_t i = 0; i < params.size(); ++i)
            {
                const auto& param = params[i];
                buffer.append(std::format("param_{}", i + 1));

                if (i + 1 < params.size())
                {
                    buffer.append(", ");
                }
            }
            buffer.append(");\n");
            // TODO: Implement return value.
            buffer.append("return 0;\n");
            buffer.append("    }\n");
            buffer.append("    catch (std::exception& e)\n");
            buffer.append("    {\n");
            buffer.append("        luaL_error(lua_state, e.what());\n");
            buffer.append("        return 0;\n");
            buffer.append("    }\n");
            buffer.append("\n");
            buffer.append("}\n");
            return buffer;
        }
    }

    auto Class::add_class(const std::string& class_name, const std::string& full_path_to_file, const std::string& fully_qualified_scope) -> Class&
    {
        return code_generator.add_class_to_container(class_name, container.classes, full_path_to_file, fully_qualified_scope);
    }

    auto Class::find_static_function_by_name(std::string_view function_name) const -> const Function*
    {
        for (const auto& static_function : static_functions)
        {
            if (static_function.second.get_name() == function_name) { return &static_function.second; }
        }

        for (const auto& base : get_bases())
        {
            for (const auto& static_function : base->static_functions)
            {
                if (static_function.second.get_name() == function_name) { return &static_function.second; }
            }
        }

        return nullptr;
    }

    auto Class::get_bases() const -> std::vector<const Class*>
    {
        std::vector<const Class*> all_bases{};

        for (const auto& base : bases)
        {
            all_bases.emplace_back(base);
            auto bases_bases = base->get_bases();
            all_bases.insert(all_bases.end(), std::make_move_iterator(bases_bases.begin()), std::make_move_iterator(bases_bases.end()));
        }

        return all_bases;
    }

    auto Class::get_metamethod_by_name(const std::string& metamethod_name) const -> const Function*
    {
        if (auto index = metamethods.find(metamethod_name); index != metamethods.end())
        {
            return &index->second;
        }
        else
        {
            return nullptr;
        }
    }

    auto Class::get_mutable_bases() -> std::unordered_set<const Class*>&
    {
        return bases;
    }

    auto is_name_an_operator_overload(std::string_view name) -> bool
    {
        if (!name.starts_with("operator")) { return false; }
        if (name == "operator&&" ||
            name == "operator||" ||
            name == "operator++" ||
            name == "operator--" ||
            name == "operator()" ||
            name == "operator[]" ||
            name == "operator+" ||
            name == "operator-" ||
            name == "operator*" ||
            name == "operator->" ||
            name == "operator/" ||
            name == "operator%" ||
            name == "operator^" ||
            name == "operator&" ||
            name == "operator|" ||
            name == "operator~" ||
            name == "operator!" ||
            name == "operator=" ||
            name == "operator<" ||
            name == "operator>" ||
            name == "operator+=" ||
            name == "operator-=" ||
            name == "operator*=" ||
            name == "operator/=" ||
            name == "operator%=" ||
            name == "operator^=" ||
            name == "operator&=" ||
            name == "operator|=" ||
            name == "operator<<" ||
            name == "operator>>" ||
            name == "operator>>=" ||
            name == "operator<<=" ||
            name == "operator==" ||
            name == "operator!=" ||
            name == "operator<=" ||
            name == "operator>=" ||
            name == "operator<=>" ||
            name == "operator," ||
            name == "operator->*")
        {
            return true;
        }
        else
        {
            return false;
        }
    }

    auto Class::generate_metamethods_map_contents() const -> std::string
    {
        std::string buffer{};

        for (const auto& metamethod_name : s_valid_metamethod_names)
        {
            auto metamethod_impl = get_metamethod_by_name(metamethod_name);
            if (!metamethod_impl)
            {
                for (const auto& inherited_class : get_bases())
                {
                    auto metamethod_inherited_impl = inherited_class->get_metamethod_by_name(metamethod_name);
                    if (metamethod_inherited_impl)
                    {
                        metamethod_impl = metamethod_inherited_impl;
                        break;
                    }
                }
            }

            if (metamethod_impl)
            {
                buffer.append(std::format("    {{\"{}\", &{}}},\n", metamethod_impl->get_name(), metamethod_impl->get_wrapper_name()));
            }
        }

        return buffer;
    }

    auto Class::generate_member_functions_map_contents() const -> std::string
    {
        std::string buffer{};

        std::unordered_map<std::string_view, bool> reserved_function_name_collision{
                {"Set", false},
                {"set", false},
                {"Get", false},
                {"get", false},
                {"IsValid", false},
                {"GetAddress", false},
        };

        auto generate_map_contents_from_container = [&](const FunctionContainer& functions, const Class* the_class) {
            for (const auto&[_, function] : functions)
            {
                auto function_name = function.get_name();
                if (auto it = reserved_function_name_collision.find(function_name); it != reserved_function_name_collision.end())
                {
                    it->second = true;
                }
                if (is_name_an_operator_overload(function_name))
                {
                    // TODO: Properly implement operator overloading by redirecting as many as possible to the Lua equivalent.
                    continue;
                }
                if (function.is_custom_redirector())
                {
                    buffer.append(std::format("    {{\"{}\", &{}}},\n", function_name, function.get_wrapper_name()));
                }
                else
                {
                    buffer.append(std::format("    {{\"{}\", &{}_{}_member_function_wrapper_{}}},\n", function_name, scope_as_function_name(the_class->fully_qualified_scope), the_class->name, function_name));
                }
            }
        };

        generate_map_contents_from_container(container.functions, this);

        for (const auto& inherited_class : get_bases())
        {
            generate_map_contents_from_container(inherited_class->container.functions, inherited_class);
        }

        buffer.append("\n    // Generic utility\n");
        if (!reserved_function_name_collision.at("Set") && !reserved_function_name_collision.at("set"))
        {
            buffer.append(std::format("    {{\"Set\", &lua_util_userdata_member_function_wrapper_Set<\"{}_{}Metatable\", {}::{}, convertible_to_{}_{}, decltype(internal_{}__{}_get_self<{}::{}**, true>), internal_{}__{}_get_self<{}::{}**, true>>}},\n", scope_as_function_name(fully_qualified_scope), name, fully_qualified_scope, name, scope_as_function_name(fully_qualified_scope), name, scope_as_function_name(fully_qualified_scope), name, fully_qualified_scope, name, scope_as_function_name(fully_qualified_scope), name, fully_qualified_scope, name));
            buffer.append(std::format("    {{\"set\", &lua_util_userdata_member_function_wrapper_Set<\"{}_{}Metatable\", {}::{}, convertible_to_{}_{}, decltype(internal_{}__{}_get_self<{}::{}**, true>), internal_{}__{}_get_self<{}::{}**, true>>}},\n", scope_as_function_name(fully_qualified_scope), name, fully_qualified_scope, name, scope_as_function_name(fully_qualified_scope), name, scope_as_function_name(fully_qualified_scope), name, fully_qualified_scope, name, scope_as_function_name(fully_qualified_scope), name, fully_qualified_scope, name));
        }
        if (!reserved_function_name_collision.at("Get") && !reserved_function_name_collision.at("get"))
        {
            buffer.append("    {\"Get\", &lua_util_userdata_member_function_wrapper_Get},\n");
            buffer.append("    {\"get\", &lua_util_userdata_member_function_wrapper_Get},\n");
        }
        if (!reserved_function_name_collision.at("IsValid"))
        {
            buffer.append(std::format("    {{\"IsValid\", &lua_util_userdata_member_function_wrapper_IsValid<decltype(internal_{}__{}_get_self<{}::{}**, true>), internal_{}__{}_get_self<{}::{}**, true>>}},\n", scope_as_function_name(fully_qualified_scope), name, fully_qualified_scope, name, scope_as_function_name(fully_qualified_scope), name, fully_qualified_scope, name));
        }
        if (!reserved_function_name_collision.at("GetAddress"))
        {
            buffer.append(std::format("    {{\"GetAddress\", &lua_util_userdata_member_function_wrapper_GetAddress<decltype(internal_{}__{}_get_self<{}::{}**, true>), internal_{}__{}_get_self<{}::{}**, true>>}},\n", scope_as_function_name(fully_qualified_scope), name, fully_qualified_scope, name, scope_as_function_name(fully_qualified_scope), name, fully_qualified_scope, name));
        }

        return buffer;
    }

    auto Class::generate_member_functions() const -> std::string
    {
        std::string buffer{};

        for (const auto&[_, member_function] : container.functions)
        {
            if (member_function.is_custom_redirector()) { continue; }

            auto function_name = member_function.get_name();
            if (is_name_an_operator_overload(function_name))
            {
                // TODO: Properly implement operator overloading by redirecting as many as possible to the Lua equivalent.
                continue;
            }
            buffer.append(std::format("inline auto {}_{}_member_function_wrapper_{}(lua_State* lua_state) -> int\n{{\n", scope_as_function_name(fully_qualified_scope), name, member_function.get_name()));
            buffer.append(member_function.generate_lua_wrapper_function_body());
            buffer.append("}\n\n");
        }

        for (const auto&[_, static_member_function] : static_functions)
        {
            if (static_member_function.is_custom_redirector()) { continue; }

            auto function_name = static_member_function.get_name();
            buffer.append(std::format("inline auto {}_{}_member_function_wrapper_{}(lua_State* lua_state) -> int\n{{\n", scope_as_function_name(fully_qualified_scope), name, static_member_function.get_name()));
            buffer.append(static_member_function.generate_lua_wrapper_function_body());
            buffer.append("}\n\n");
        }

        return buffer;
    }

    auto Class::generate_constructor() const -> std::string
    {
        std::string buffer{};

        if (auto constructor_it = constructors.find(fully_qualified_scope + "::" + name + "::" + name); constructor_it != constructors.end())
        {
            const auto& constructor = constructor_it->second;

            buffer.append(std::format("inline auto lua_setup_{}_{}_constructor_dispatch(lua_State* lua_state) -> void\n", scope_as_function_name(fully_qualified_scope), name));
            buffer.append("{\n");
            buffer.append("    lua_newtable(lua_state);\n");
            buffer.append("    lua_pushliteral(lua_state, \"__call\");\n");
            buffer.append("    lua_pushcfunction(lua_state, ([](lua_State* lua_state) -> int {\n");
            buffer.append("        lua_remove(lua_state, 1);\n");
            auto function_name = constructor.get_name();
            if (has_parameterless_constructor)
            {
                buffer.append("        if (lua_gettop(lua_state) == 0)\n");
                buffer.append("        {\n");
                buffer.append(std::format("            auto constructed_object = {}::{}{{}};\n", fully_qualified_scope, name));
                buffer.append(std::format("            auto* userdata = static_cast<{}::{}*>(lua_newuserdatauv(lua_state, sizeof({}::{}), 1));\n", fully_qualified_scope, name, fully_qualified_scope, name));
                buffer.append("            lua_pushinteger(lua_state, 0);\n");
                buffer.append("            lua_setiuservalue(lua_state, -2, 1);\n");
                buffer.append(std::format("            new(userdata) {}::{}{{std::move(constructed_object)}};\n", fully_qualified_scope, name));
                // Is 'fully_qualified_scope' right ?
                buffer.append(std::format("            luaL_getmetatable(lua_state, \"{}_{}Metatable\");\n", scope_as_function_name(fully_qualified_scope), name));
                buffer.append("            lua_setmetatable(lua_state, -2);\n");
                buffer.append("            return 1;\n");
                buffer.append("        }\n");
            }
            buffer.append(constructor.generate_lua_wrapper_function_body());
            buffer.append("    }));\n");
            buffer.append("    lua_rawset(lua_state, -3);\n");
            buffer.append("    lua_setmetatable(lua_state, -2);\n");
            buffer.append("}");
        }

        return buffer;
    }

    auto Class::generate_metamethods_map() const -> std::string
    {
        std::string buffer{};

        buffer.append(std::format("inline static std::unordered_map<std::string, int (*)(lua_State*, void*)> {}_{}_metamethods = {{\n", scope_as_function_name(fully_qualified_scope), name));
        buffer.append(generate_metamethods_map_contents());
        buffer.append("};\n");

        return buffer;
    }

    auto Class::generate_member_functions_map() const -> std::string
    {
        std::string buffer{};

        buffer.append(std::format("inline static std::unordered_map<std::string, int (*)(lua_State*)> {}_{}_member_functions = {{\n", scope_as_function_name(fully_qualified_scope), name));
        buffer.append(generate_member_functions_map_contents());
        buffer.append("};\n");

        return buffer;
    }

    auto Class::generate_setup_function() const -> std::string
    {
        std::string buffer{};

        buffer.append(std::format("inline auto lua_setup_{}_{}(lua_State* lua_state) -> void\n{{\n", scope_as_function_name(fully_qualified_scope), name));

        buffer.append("    // Metatable For Userdata -> START\n");
        buffer.append(std::format("    luaL_newmetatable(lua_state, \"{}_{}Metatable\");\n\n", scope_as_function_name(fully_qualified_scope), name));

        buffer.append("    lua_pushliteral(lua_state, \"__cxx_name\");\n");
        buffer.append(std::format("    lua_pushliteral(lua_state, \"{}::{}\");\n", fully_qualified_scope, name));
        buffer.append("    lua_rawset(lua_state, -3);\n\n");

        for (const auto& type_patch : code_generator.get_type_patches())
        {
            if (type_patch.generate_per_class_static_functions)
            {
                buffer.append(type_patch.generate_per_class_static_functions(*this));
            }
        }

        buffer.append("    lua_pushliteral(lua_state, \"__index\");\n");
        buffer.append("    lua_pushcclosure(lua_state, [](lua_State* lua_state) -> int {\n");
        buffer.append("        if (!lua_isuserdata(lua_state, -2))\n");
        buffer.append("        {\n");
        buffer.append("                lua_remove(lua_state, -1);\n");
        buffer.append("                lua_remove(lua_state, -2);\n");
        buffer.append(std::format("                luaL_error(lua_state, \"{} member accessed without self context\");\n", name));
        buffer.append("        }\n\n");

        buffer.append("        luaL_argcheck(lua_state, lua_isstring(lua_state, -1), 2, \"accessing __index must be done with a string\");\n\n");

        buffer.append("        auto index = std::string_view{lua_tostring(lua_state, -1)};\n");
        buffer.append(std::format("        if (auto it = {}_{}_member_functions.find(index.data()); it != {}_{}_member_functions.end())\n", scope_as_function_name(fully_qualified_scope), name, scope_as_function_name(fully_qualified_scope), name));
        buffer.append("        {\n");
        buffer.append("            lua_pop(lua_state, 1);\n");
        buffer.append("            lua_pushcfunction(lua_state, it->second);\n");
        buffer.append("            return 1;\n");
        buffer.append("        }\n\n");

        buffer.append(std::format("        auto [_, self] = internal_{}__{}_get_self<{}::{}*, false, false>(lua_state);\n", scope_as_function_name(fully_qualified_scope), name, fully_qualified_scope, name));

        buffer.append(std::format("        if (!self) {{ luaL_error(lua_state, \"{} member accessed with self == nullptr\"); }};\n", name));

        buffer.append(std::format("        auto index_it = {}_{}_metamethods.find(\"__index\");\n", scope_as_function_name(fully_qualified_scope), name));
        buffer.append(std::format("        if (index_it == {}_{}_metamethods.end()) {{ return 0; }}\n", scope_as_function_name(fully_qualified_scope), name));
        buffer.append("        return index_it->second(lua_state, self);\n");

        buffer.append("    }, 0);\n");
        buffer.append("    lua_rawset(lua_state, -3);\n\n");

        for (const auto& metamethod_name : s_valid_metamethod_names)
        {
            if (metamethod_name == "__index") { continue; }
            auto metamethod_impl = get_metamethod_by_name(metamethod_name);
            if (!metamethod_impl)
            {
                for (const auto& inherited_class : get_bases())
                {
                    auto metamethod_inherited_impl = inherited_class->get_metamethod_by_name(metamethod_name);
                    if (metamethod_inherited_impl)
                    {
                        metamethod_impl = metamethod_inherited_impl;
                        break;
                    }
                }
            }

            if (metamethod_impl)
            {
                //buffer.append(std::format("    lua_pushliteral(lua_state, \"{}\");\n", metamethod_impl->get_name()));
                //buffer.append(std::format("    lua_pushcfunction(lua_state, &{});\n", metamethod_impl->get_wrapper_name()));
                //buffer.append("    lua_rawset(lua_state, -3);\n\n");

                buffer.append(std::format("    lua_pushliteral(lua_state, \"{}\");\n", metamethod_impl->get_name()));
                buffer.append("    lua_pushcclosure(lua_state, [](lua_State* lua_state) -> int {\n");
                buffer.append("        if (!lua_isuserdata(lua_state, 1))\n");
                buffer.append("        {\n");
                buffer.append("                lua_remove(lua_state, 1);\n");
                buffer.append(std::format("                luaL_error(lua_state, \"metamethod '{}' for '{}' accessed without self context\");\n", metamethod_impl->get_name(), name));
                buffer.append("        }\n\n");

                buffer.append(std::format("        auto [_, self] = internal_{}__{}_get_self<{}::{}*, false, false>(lua_state);\n", scope_as_function_name(fully_qualified_scope), name, fully_qualified_scope, name));

                buffer.append(std::format("        if (!self) {{ luaL_error(lua_state, \"{} member accessed with self == nullptr\"); }};\n", name));

                buffer.append(std::format("        auto it = {}_{}_metamethods.find(\"{}\");\n", scope_as_function_name(fully_qualified_scope), name, metamethod_impl->get_name()));
                buffer.append(std::format("        if (it == {}_{}_metamethods.end()) {{ return 0; }}\n", scope_as_function_name(fully_qualified_scope), name));
                buffer.append("        return it->second(lua_state, self);\n");

                buffer.append("    }, 0);\n");
                buffer.append("    lua_rawset(lua_state, -3);\n\n");
            }
        }

        buffer.append("    // Remove table from the stack now that we're done with it.\n");
        buffer.append("    lua_remove(lua_state, -1);\n");
        buffer.append("    // Metatable For Userdata -> END\n\n");

        std::vector<std::string> scope_parts{};
        auto fully_qualified_scope = scope_override.empty() ? this->fully_qualified_scope : scope_override;
        get_scope_parts(fully_qualified_scope, scope_parts);

        if (scope_parts.empty())
        {
            buffer.append("    // Scopes -> START\n");
            buffer.append("    lua_newtable(lua_state);\n");
            buffer.append(std::format("    lua_setglobal(lua_state, \"{}\");\n", name));
        }
        else
        {
            auto global_table_name = scope_parts[0];

            bool put_in_global_table = fully_qualified_scope == "::";
            if (!put_in_global_table)
            {
                // Remove the first scope which always becomes the global table.
                scope_parts.erase(scope_parts.begin());

                buffer.append("    // Scopes -> START\n");
                buffer.append(std::format("    bool global_table_exists = lua_getglobal(lua_state, \"{}\") == LUA_TTABLE;\n", global_table_name));
                buffer.append("    if (!global_table_exists)\n");
                buffer.append("    {\n");
                buffer.append("        lua_pop(lua_state, 1);\n");
                buffer.append("        lua_newtable(lua_state);\n");
                buffer.append("    }\n\n");

                for (const auto& scope_part : scope_parts)
                {
                    buffer.append(std::format("    lua_pushliteral(lua_state, \"{}\");\n", scope_part));
                    buffer.append("    if (lua_rawget(lua_state, -2) != LUA_TTABLE)\n");
                    buffer.append("    {\n");
                    buffer.append("        lua_pop(lua_state, 1);\n");
                    buffer.append(std::format("        lua_pushliteral(lua_state, \"{}\");\n", scope_part));
                    buffer.append("        lua_newtable(lua_state);\n");
                    buffer.append("        lua_rawset(lua_state, -3);\n");
                    buffer.append(std::format("        lua_pushliteral(lua_state, \"{}\");\n", scope_part));
                    buffer.append("        lua_rawget(lua_state, -2);\n");
                    buffer.append("    }\n\n");
                }
                buffer.append(std::format("    lua_pushliteral(lua_state, \"{}\");\n", name));
            }

            buffer.append("    lua_newtable(lua_state);\n\n");

            for (const auto&[_, static_function] : static_functions)
            {
                buffer.append(std::format("    lua_pushliteral(lua_state, \"{}\");\n", static_function.get_name()));
                if (static_function.is_custom_redirector())
                {
                    buffer.append(std::format("    lua_pushcfunction(lua_state, &{});\n", static_function.get_wrapper_name()));
                }
                else
                {
                    buffer.append(std::format("    lua_pushcfunction(lua_state, &{}_{}_member_function_wrapper_{});\n", scope_as_function_name(static_function.get_containing_class()->fully_qualified_scope), static_function.get_containing_class()->name, static_function.get_name()));
                }

                buffer.append("    lua_rawset(lua_state, -3);\n\n");
            }

            if (constructors.contains(fully_qualified_scope + "::" + name + "::" + name))
            {
                buffer.append("    // Metatable For Table -> START\n");
                buffer.append(std::format("    lua_setup_{}_{}_constructor_dispatch(lua_state);\n", scope_as_function_name(fully_qualified_scope), name));
                buffer.append("    // Metatable For Table -> END\n\n");
            }

            if (!put_in_global_table)
            {
                buffer.append("    lua_rawset(lua_state, -3);\n");

                if (!scope_parts.empty())
                {
                    //buffer.append("    lua_pop(lua_state, 1);\n");
                    buffer.append(std::format("    lua_pop(lua_state, {});\n", scope_parts.size()));
                }
                buffer.append("\n    if (global_table_exists)\n");
                buffer.append("    {\n");
                buffer.append("        lua_pop(lua_state, 1);\n");
                buffer.append("    }\n");
                buffer.append("    else\n");
                buffer.append("    {\n");
                buffer.append(std::format("        lua_setglobal(lua_state, \"{}\");\n", global_table_name));
                buffer.append("    }\n");
            }
            else
            {
                buffer.append(std::format("    lua_setglobal(lua_state, \"{}\");\n", name));
            }
        }

        buffer.append("    // Scopes -> END\n");

        buffer.append("}\n");

        return buffer;
    }

    auto Class::generate_create_instance_of_function() const -> std::string
    {
        std::string buffer{};

        buffer.append(std::format("auto lua_create_instance_of_{}(lua_State* lua_state) -> void\n{{\n", name));

        buffer.append(std::format("    auto* userdata = static_cast<{}::{}*>(lua_newuserdatauv(lua_state, sizeof({}::{}), 0));\n", fully_qualified_scope, name, scope_as_function_name(fully_qualified_scope), name));
        buffer.append(std::format("    new(userdata) {}::{}{{}};\n\n", fully_qualified_scope, name));

        buffer.append(std::format("    luaL_getmetatable(lua_state, \"{}_{}Metatable\");\n", fully_qualified_scope, name));
        buffer.append("    lua_setmetatable(lua_state, -2);\n");

        buffer.append("}\n");

        return buffer;
    }

    //auto Class::generate_lua_wrapper_function_body_prologue() const -> std::string
    //{
    //    std::string buffer{};
//
    //    buffer.append("    luaL_argcheck(lua_state, lua_isuserdata(lua_state, 1), 1, \"first param was not userdata\");\n");
//
    //    buffer.append("    bool is_pointer{};\n");
    //    buffer.append("    lua_getiuservalue(lua_state, 1, 1);\n");
    //    buffer.append("    if (!lua_isboolean(lua_state, -1))\n    {\n");
    //    buffer.append("        lua_pop(lua_state, 1);\n");
    //    buffer.append("    }\n    else\n    {\n");
    //    buffer.append("        is_pointer = lua_toboolean(lua_state, -1);\n");
    //    buffer.append("    }\n");
//
    //    buffer.append(std::format("    {}::{}* self{{}};\n", get_parent_scope(), get_parent_name()));
    //    buffer.append(std::format("    if (is_pointer)\n    {{\n"));
    //    buffer.append(std::format("        self = *static_cast<{}::{}**>(luaL_checkudata(lua_state, 1, \"{}Metatable\"));\n", get_parent_scope(), get_parent_name(), get_parent_name()));
    //    buffer.append(std::format("    }}\n    else\n    {{\n"));
    //    buffer.append(std::format("        self = static_cast<{}::{}*>(luaL_checkudata(lua_state, 1, \"{}Metatable\"));\n", get_parent_scope(), get_parent_name(), get_parent_name()));
    //    buffer.append(std::format("    }}\n"));
    //    buffer.append(std::format("    luaL_argcheck(lua_state, self, 1, \"self was nullptr\");\n"));
    //    buffer.append(std::format("    lua_remove(lua_state, 1);\n"));
//
    //    return buffer;
    //}

    auto Class::generate_internal_get_self_function() const -> std::string
    {
        std::string buffer{};

        buffer.append(std::format("template<typename ReturnType = {}::{}*, bool return_container_or_nullptr = false, bool pop_userdata = true>\n", fully_qualified_scope, name));
        buffer.append(std::format("inline auto internal_{}__{}_get_self(lua_State* lua_state) -> std::pair<bool, ReturnType>\n", scope_as_function_name(fully_qualified_scope), name));
        buffer.append("{\n");
        //buffer.append(std::format("    {}\n", generate_lua_wrapper_function_body_prologue()));
        buffer.append(std::format("    luaL_argcheck(lua_state, lua_isuserdata(lua_state, 1), 1, \"first param was not userdata of type '{}'\");\n", name));

        buffer.append("    lua_getiuservalue(lua_state, 1, 1);\n");
        buffer.append("    int pointer_depth = lua_tointeger(lua_state, -1);\n");
        buffer.append("    bool is_pointer = pointer_depth > 0;\n");

        buffer.append("    lua_getmetatable(lua_state, 1);\n");
        buffer.append("    lua_pushliteral(lua_state, \"__name\");\n");
        buffer.append("    lua_rawget(lua_state, -2);\n");
        buffer.append("    auto metatable_name = std::string{lua_tostring(lua_state, -1)};\n");
        buffer.append(std::format("    auto bad_self_error_message = std::format(\"self was '{{}}', expected '{}_{}Metatable' or derivative\", metatable_name);\n", scope_as_function_name(fully_qualified_scope), name));
        buffer.append(std::format("    luaL_argcheck(lua_state, convertible_to_{}_{}.contains(metatable_name), 1, bad_self_error_message.c_str());\n", scope_as_function_name(fully_qualified_scope), name));

        buffer.append("    lua_pop(lua_state, 3);\n");

        buffer.append(std::format("    {}::{}** self_container{{}};\n", fully_qualified_scope, name));
        buffer.append(std::format("    {}::{}* self{{}};\n", fully_qualified_scope, name));
        buffer.append(std::format("    if (is_pointer)\n    {{\n"));
        buffer.append("        auto* outer_most_container = lua_touserdata(lua_state, 1);\n");
        buffer.append("        if (outer_most_container)\n");
        buffer.append("        {\n");
        buffer.append(std::format("            self_container = static_cast<{}::{}**>(deref(outer_most_container, pointer_depth - 1));\n", fully_qualified_scope, name));
        buffer.append("            if (self_container) { self = *self_container; }\n");
        buffer.append("        }\n");
        buffer.append(std::format("    }}\n    else\n    {{\n"));
        buffer.append(std::format("        self = static_cast<{}::{}*>(lua_touserdata(lua_state, 1));\n", fully_qualified_scope, name));
        buffer.append(std::format("    }}\n"));
        buffer.append("    if constexpr (pop_userdata)\n");
        buffer.append("    {\n");
        buffer.append("        lua_remove(lua_state, 1);\n");
        buffer.append("    }\n");

        buffer.append("    if constexpr (return_container_or_nullptr)\n");
        buffer.append("    {\n");
        buffer.append("        if (is_pointer)\n");
        buffer.append("        {\n");
        buffer.append("            return {is_pointer, self_container};\n");
        buffer.append("        }\n");
        buffer.append("        else\n");
        buffer.append("        {\n");
        // This bit_cast is safe only when the caller checks 'is_pointer' before usage.
        buffer.append("            luaL_argcheck(lua_state, self, 1, \"self was nullptr\");\n");
        buffer.append("            return {is_pointer, std::bit_cast<ReturnType>(self)};\n");
        buffer.append("        }\n");
        buffer.append("    }\n");
        buffer.append("    else\n");
        buffer.append("    {\n");
        buffer.append(std::format("        luaL_argcheck(lua_state, self, 1, \"self was nullptr\");\n"));
        buffer.append("        return {is_pointer, self};\n");
        buffer.append("    }\n");

        buffer.append("}");

        return buffer;
    }

    auto Enum::add_key_value_pair(std::string key, uint64_t value) -> void
    {
        m_keys_and_values.emplace_back(std::move(key), value);
    }

    auto Function::generate_lua_wrapper_function_body() const -> std::string
    {
        std::string buffer{};

        buffer.append("    try\n    {\n");

        if (get_containing_class() && !is_static())
        {
            buffer.append("        // Prologue\n");
            buffer.append(std::format("        auto [_, self] = internal_{}_get_self(lua_state);\n\n", scope_as_function_name(get_fully_qualified_scope())));
        }

        buffer.append("        // Native call\n");
        buffer.append(generate_cxx_call(*this));
        buffer.append(generate_lua_return_statement());
        buffer.append("\n");
        buffer.append("    }\n");
        buffer.append("    catch (std::exception& e)\n");
        buffer.append("    {\n");
        buffer.append("        luaL_error(lua_state, e.what());\n");
        buffer.append("        return 0;\n");
        buffer.append("    }\n");
        buffer.append("\n");

        return buffer;
    }

    auto Function::generate_lua_return_statement() const -> std::string
    {
        if (get_return_type()->is_a<Type::Void>() && !get_return_type()->is_pointer())
        {
            return "        return 0;";
        }
        else
        {
            std::string buffer{};

            buffer.append(std::format("{}", get_return_type()->generate_lua_stack_pusher("return_value", "")));
            // TODO: We don't support multiple return values.
            buffer.append(";\n        return 1;");

            return buffer;
        }
    }

    auto CodeGenerator::add_class(const std::string& class_name, const std::string& full_path_to_file, const std::string& fully_qualified_scope) -> Class&
    {
        return add_class_to_container(class_name, m_container.classes, full_path_to_file, fully_qualified_scope);
    }

    auto CodeGenerator::add_thin_class(const std::string& class_name, const std::string& full_path_to_file, const std::string& fully_qualified_scope) -> Class&
    {
        return add_class_to_container(class_name, m_container.thin_classes, full_path_to_file, fully_qualified_scope);
    }

    auto CodeGenerator::generate_setup_functions_map() const -> std::string
    {
        std::string buffer{"static std::unordered_map<std::string, void (*)(lua_State*)> s_state_setup_functions{\n"};

        for (const auto& lua_state_type : m_container.lua_state_types)
        {
            buffer.append(std::format("    {{\"{}\", &lua_setup_state_{}}},\n", lua_state_type, lua_state_type));
        }

        buffer.append("};\n");
        return buffer;
    }

    auto CodeGenerator::generate_lua_dynamic_setup_state_function() const -> std::string
    {
        return {R"(auto lua_setup_state(lua_State* lua_state, const std::string& state_name) -> void
{
    if (auto it = s_state_setup_functions.find(state_name); it != s_state_setup_functions.end())
    {
        it->second(lua_state);
    }
    else
    {
        luaL_error(lua_state, std::format("Was unable to find lua state type '{}'", state_name).c_str());
    }
})"};
    }

    static auto generate_function_proto_metatable() -> std::string
    {
        std::string buffer{};
        buffer.append("auto inline setup_FunctionProto(lua_State* lua_state) -> void\n");
        buffer.append("{\n");
        buffer.append("    luaL_newmetatable(lua_state, \"FunctionProtoMetatable\");\n");
        buffer.append("    lua_pushliteral(lua_state, \"__call\");\n");
        buffer.append("    lua_pushcfunction(lua_state, [](lua_State* lua_state) -> int {\n");
        buffer.append("        luaL_argcheck(lua_state, lua_isuserdata(lua_state, 1), 1, \"first param for 'FunctionProto' was not userdata\");\n");
        buffer.append("        lua_getmetatable(lua_state, 1);\n");
        buffer.append("        lua_pushliteral(lua_state, \"__name\");\n");
        buffer.append("        lua_rawget(lua_state, -2);\n");
        buffer.append("        auto metatable_name = std::string{lua_tostring(lua_state, -1)};\n");
        buffer.append("        if (metatable_name != \"FunctionProtoMetatable\") { luaL_error(lua_state, \"self was '{}', expected FunctionProtoMetatable\"); }\n");
        buffer.append("        auto function_proto = static_cast<FunctionProto*>(lua_touserdata(lua_state, 1));\n");
        buffer.append("        lua_pop(lua_state, 2);\n");
        buffer.append("        return function_proto->lua_wrapper_function_function_pointer(lua_state);\n");
        buffer.append("    });\n");
        buffer.append("    lua_rawset(lua_state, -3);\n");
        buffer.append("    lua_pushliteral(lua_state, \"__index\");\n");
        buffer.append("    lua_pushcfunction(lua_state, [](lua_State* lua_state) -> int {\n");
        buffer.append("        luaL_argcheck(lua_state, lua_isuserdata(lua_state, 1), 1, \"first param for 'FunctionProto' was not userdata\");\n");
        buffer.append("        lua_getmetatable(lua_state, 1);\n");
        buffer.append("        lua_pushliteral(lua_state, \"__name\");\n");
        buffer.append("        lua_rawget(lua_state, -2);\n");
        buffer.append("        auto metatable_name = std::string{lua_tostring(lua_state, -1)};\n");
        buffer.append("        if (metatable_name != \"FunctionProtoMetatable\") { luaL_error(lua_state, \"self was '{}', expected FunctionProtoMetatable\"); }\n");
        buffer.append("        auto function_proto = static_cast<FunctionProto*>(lua_touserdata(lua_state, 1));\n");
        buffer.append("        lua_pop(lua_state, 2);\n");
        buffer.append("        lua_remove(lua_state, 1);\n");
        buffer.append("        if (lua_isstring(lua_state, 1))\n");
        buffer.append("        {\n");
        buffer.append("            auto member_name = std::string{lua_tostring(lua_state, 1)};\n");
        buffer.append("            if (member_name == \"GetFunctionAddress\")\n");
        buffer.append("            {\n");
        buffer.append("                lua_pushcfunction(lua_state, [](lua_State* lua_state) -> int {\n");
        buffer.append("                    luaL_argcheck(lua_state, lua_isuserdata(lua_state, 1), 1, \"first param for 'FunctionProto' was not userdata\");\n");
        buffer.append("                    lua_getmetatable(lua_state, 1);\n");
        buffer.append("                    lua_pushliteral(lua_state, \"__name\");\n");
        buffer.append("                    lua_rawget(lua_state, -2);\n");
        buffer.append("                    auto metatable_name = std::string{lua_tostring(lua_state, -1)};\n");
        buffer.append("                    if (metatable_name != \"FunctionProtoMetatable\") { luaL_error(lua_state, \"self was '{}', expected FunctionProtoMetatable\"); }\n");
        buffer.append("                    auto function_proto = static_cast<FunctionProto*>(lua_touserdata(lua_state, 1));\n");
        buffer.append("                    lua_pop(lua_state, 3);\n");
        buffer.append("                    lua_pushinteger(lua_state, std::bit_cast<lua_Integer>(function_proto->function_pointer));\n");
        buffer.append("                    return 1;\n");
        buffer.append("                });\n");
        buffer.append("                return 1;\n");
        buffer.append("            }\n");
        buffer.append("            else\n");
        buffer.append("            {\n");
        buffer.append("                return 0;\n");
        buffer.append("            }\n");
        buffer.append("        }\n");
        buffer.append("        else\n");
        buffer.append("        {\n");
        buffer.append("            return 0;\n");
        buffer.append("        }\n");
        buffer.append("    });\n");
        buffer.append("    lua_rawset(lua_state, -3);\n");
        buffer.append("    lua_pop(lua_state, 1);\n");
        buffer.append("}\n");
        return buffer;
    }

    auto CodeGenerator::generate_lua_setup_state_functions() const -> std::string
    {
        std::string buffer{};

        for (const auto& lua_state_type : m_container.lua_state_types)
        {
            buffer.append(std::format("inline auto lua_setup_state_{}(lua_State* lua_state) -> void\n", lua_state_type));
            buffer.append("{\n");

            buffer.append("setup_FunctionProto(lua_state);");

            for (const auto&[_, the_class] : m_container.classes)
            {
                buffer.append(std::format("    lua_setup_{}_{}(lua_state);\n", scope_as_function_name(the_class.fully_qualified_scope), the_class.name));
            }

            buffer.append("\n");
            buffer.append(std::format("    lua_setup_global_free_functions_{}(lua_state);\n", lua_state_type));
            buffer.append(std::format("    lua_setup_enums_{}(lua_state);\n", lua_state_type));

            for (const auto& type_patch : m_type_patches)
            {
                buffer.append(type_patch.generate_lua_setup_state_function_post());
            }

            buffer.append("}\n\n");
        }

        return buffer;
    }

    auto CodeGenerator::generate_lua_setup_file() const -> void
    {
        auto file = File::open(m_output_path / "include/LuaBindings/LuaSetup.hpp", File::OpenFor::Writing, File::OverwriteExistingFile::Yes, File::CreateIfNonExistent::Yes);

        std::string file_contents = "#ifndef LUAWRAPPERGENERATOR_LUASETUP_HPP\n#define LUAWRAPPERGENERATOR_LUASETUP_HPP\n\n";

        file_contents.append("#include <atomic>\n");
        file_contents.append("#include <format>\n");
        file_contents.append("#include <functional>\n");
        file_contents.append("\n");

        for (const auto& lua_state_type : m_container.lua_state_types)
        {
            file_contents.append(std::format("#include <LuaBindings/States/{}/Main.hpp>\n", lua_state_type));
        }

        file_contents.append("\nnamespace RC::LuaBindings\n{\n");
        file_contents.append(generate_setup_functions_map());
        file_contents.append("\n");
        file_contents.append(generate_lua_dynamic_setup_state_function());
        file_contents.append("\n} // RC::LuaBindings\n\n#endif //LUAWRAPPERGENERATOR_LUASETUP_HPP\n");

        file.write_string_to_file(File::StringType{file_contents.begin(), file_contents.end()});
        file.close();
    }

    auto CodeGenerator::generate_free_functions() const -> std::string
    {
        std::string buffer{};
        for (const auto&[_, free_function] : m_container.functions)
        {
            if (free_function.is_custom_redirector()) { continue; }
            if (free_function.is_alias()) { continue; }
            if (!free_function.get_wrapper_name().empty()) { continue; }

            buffer.append(std::format("inline auto lua_{}_wrapper(lua_State* lua_state) -> int\n{{\n", free_function.get_name()));
            //buffer.append(std::format("    \n", free_function.get_name()));
            buffer.append(free_function.generate_lua_wrapper_function_body());
            buffer.append("}\n");
        }
        return buffer;
    }

    static auto resolve_to_scope(std::vector<std::string>& scope_parts, std::string& buffer, auto callable) -> void
    {
        auto global_table_name = scope_parts[0];

        // Remove the first scope which always becomes the global table.
        scope_parts.erase(scope_parts.begin());

        buffer.append(std::format("        bool global_table_exists = lua_getglobal(lua_state, \"{}\") == LUA_TTABLE;\n", global_table_name));
        buffer.append("        if (!global_table_exists)\n");
        buffer.append("        {\n");
        buffer.append("            lua_newtable(lua_state);\n");
        buffer.append("        }\n\n");

        for (const auto& scope_part : scope_parts)
        {
            buffer.append(std::format("        lua_pushliteral(lua_state, \"{}\");\n", scope_part));
            buffer.append("        if (lua_rawget(lua_state, -2) != LUA_TTABLE)\n");
            buffer.append("        {\n");
            buffer.append(std::format("            lua_pushliteral(lua_state, \"{}\");\n", scope_part));
            buffer.append("            lua_newtable(lua_state);\n");
            buffer.append("            lua_rawset(lua_state, -4);\n");
            buffer.append(std::format("            lua_pushliteral(lua_state, \"{}\");\n", scope_part));
            buffer.append("            lua_rawget(lua_state, -3);\n");
            buffer.append("        }\n\n");
        }

        callable();

        if (!scope_parts.empty())
        {
            buffer.append("        lua_pop(lua_state, 2);\n");
        }
        buffer.append("\n        if (!global_table_exists)\n");
        buffer.append("        {\n");
        buffer.append(std::format("            lua_setglobal(lua_state, \"{}\");\n", global_table_name));
        buffer.append("        }\n");
        buffer.append("        else\n");
        buffer.append("        {\n");
        buffer.append("            lua_pop(lua_state, 1);\n");
        buffer.append("        }\n");

    }

    auto CodeGenerator::generate_lua_setup_global_free_functions() const -> std::string
    {
        std::string buffer{};
        for (const auto& lua_state_type : m_container.lua_state_types)
        {
            buffer.append(std::format("inline auto lua_setup_global_free_functions_{}(lua_State* lua_state) -> void\n{{\n", lua_state_type));
            for (const auto&[_, free_function] : m_container.functions)
            {
                auto fully_qualified_scope = free_function.get_scope_override().empty() ? free_function.get_fully_qualified_scope() : free_function.get_scope_override();
                auto wrapper_function = free_function.get_wrapper_name().empty() ? std::format("lua_{}_wrapper", free_function.get_name()) : std::string{free_function.get_wrapper_name()};

                buffer.append("    {\n");

                if (fully_qualified_scope.empty() || fully_qualified_scope == "::")
                {
                    buffer.append(std::format("        lua_pushcfunction(lua_state, &{});\n", wrapper_function));
                    buffer.append(std::format("        lua_setglobal(lua_state, \"{}\");\n", free_function.get_lua_name()));
                }
                else
                {
                    std::vector<std::string> scope_parts{};
                    get_scope_parts(fully_qualified_scope, scope_parts);
                    resolve_to_scope(scope_parts, buffer, [&]() {
                        buffer.append(std::format("        lua_pushliteral(lua_state, \"{}\");\n", free_function.get_lua_name()));
                        buffer.append(std::format("        lua_pushcfunction(lua_state, &{});\n", wrapper_function));
                        buffer.append("        lua_rawset(lua_state, -3);\n");
                    });
                }
                buffer.append("    }\n");
                buffer.append("\n");
            }
            buffer.append("}\n");
        }
        return buffer;
    }

    auto CodeGenerator::generate_lua_setup_enums() const -> std::string
    {
        std::string buffer{};
        for (const auto& lua_state_type : m_container.lua_state_types)
        {
            buffer.append(std::format("inline auto lua_setup_enums_{}(lua_State* lua_state) -> void\n{{\n", lua_state_type));
            for (const auto&[_, the_enum] : m_container.enums)
            {
                auto fully_qualified_scope = the_enum.get_fully_qualified_scope();

                buffer.append("    {\n");

                if (fully_qualified_scope.empty() || fully_qualified_scope == "::")
                {
                    //buffer.append(std::format("        lua_pushcfunction(lua_state, &{});\n", wrapper_function));
                    //buffer.append(std::format("        lua_setglobal(lua_state, \"{}\");\n", the_enum.get_name()));

                    buffer.append("        lua_newtable(lua_state);\n");
                    for (const auto&[enum_key, enum_value] : the_enum.get_key_value_pairs())
                    {
                        buffer.append(std::format("        lua_pushliteral(lua_state, \"{}\");\n", enum_key));
                        buffer.append(std::format("        lua_pushinteger(lua_state, {});\n", enum_value));
                        buffer.append("lua_rawset(lua_state, -3);\n");
                    }
                    buffer.append(std::format("        lua_setglobal(lua_state, \"{}\");\n", the_enum.get_name()));
                }
                else
                {
                    std::vector<std::string> scope_parts{};
                    get_scope_parts(fully_qualified_scope, scope_parts);
                    resolve_to_scope(scope_parts, buffer, [&]() {
                        //buffer.append(std::format("        lua_pushliteral(lua_state, \"{}\");\n", the_enum.get_name()));
                        //buffer.append(std::format("        lua_pushcfunction(lua_state, &{});\n", wrapper_function));
                        //buffer.append("        lua_rawset(lua_state, -3);\n");

                        buffer.append(std::format("        lua_pushliteral(lua_state, \"{}\");\n", the_enum.get_name()));
                        buffer.append("        lua_newtable(lua_state);\n");
                        for (const auto&[enum_key, enum_value] : the_enum.get_key_value_pairs())
                        {
                            buffer.append(std::format("        lua_pushliteral(lua_state, \"{}\");\n", enum_key));
                            buffer.append(std::format("        lua_pushinteger(lua_state, {});\n", enum_value));
                            buffer.append("        lua_rawset(lua_state, -3);\n");
                        }
                        buffer.append("        lua_rawset(lua_state, -3);\n");
                    });
                }
                buffer.append("    }\n");
                buffer.append("\n");
            }
            buffer.append("}\n");
        }
        return buffer;
    }

    auto CodeGenerator::generate_convertible_to_set() const -> std::string
    {
        std::string buffer{};
        std::unordered_map<std::string, std::unordered_set<std::string>> class_buffer{};

        for (const auto&[_, the_class] : m_container.classes)
        {
            class_buffer[scope_as_function_name(the_class.fully_qualified_scope) + "_" + the_class.name].emplace(scope_as_function_name(the_class.fully_qualified_scope) + "_" + the_class.name);
            for (const auto& base : the_class.get_bases())
            {
                class_buffer[scope_as_function_name(base->fully_qualified_scope) + "_" + base->name].emplace(scope_as_function_name(the_class.fully_qualified_scope) + "_" + the_class.name);
            }
        }

        // Generate 'set' for all types that have another type that inherits from it.
        for (const auto&[the_class, convertible_from_classes] : class_buffer)
        {
            buffer.append(std::format("inline std::unordered_set<std::string> convertible_to_{} {{\n", the_class));
            for (const auto& convertible_from_class : convertible_from_classes)
            {
                buffer.append(std::format("        {{\"{}Metatable\"}},\n", convertible_from_class));
            }
            buffer.append("};\n\n");
        }

        // Generate empty 'set' for all types that are final.
        for (const auto&[_, the_class] : m_container.classes)
        {
            if (!class_buffer.contains(scope_as_function_name(the_class.fully_qualified_scope) + "_" + the_class.name))
            {
                buffer.append(std::format("    inline std::unordered_set<std::string> convertible_to_{}_{} {{ }};\n\n", scope_as_function_name(the_class.fully_qualified_scope), the_class.name));
            }
        }

        // Removing the last newlines that were appended by the loop.
        // This is because surrounding newlines is not local to this scope and should be taken care of by whatever function calls this function.
        buffer.pop_back();
        buffer.pop_back();

        return buffer;
    }

    auto CodeGenerator::generate_builtin_to_lua_from_heap_functions() const -> std::string
    {
        return R"(#define GenerateBuiltinToLuaFromHeapFunction(BuiltinType) \
inline auto lua_##BuiltinType##_to_lua_from_heap(lua_State* lua_state, void* item, uint32_t pointer_depth) -> void \
{ \
    auto* userdata = static_cast<BuiltinType*>(lua_newuserdatauv(lua_state, sizeof(BuiltinType*), 1)); \
    lua_pushinteger(lua_state, pointer_depth); \
    lua_setiuservalue(lua_state, -2, 1); \
    new(userdata) BuiltinType*{static_cast<BuiltinType*>(item)}; \
    luaL_getmetatable(lua_state, #BuiltinType"Metatable"); \
    lua_setmetatable(lua_state, -2); \
}

    GenerateBuiltinToLuaFromHeapFunction(int8_t)
    GenerateBuiltinToLuaFromHeapFunction(int16_t)
    GenerateBuiltinToLuaFromHeapFunction(int32_t)
    GenerateBuiltinToLuaFromHeapFunction(int64_t)
    GenerateBuiltinToLuaFromHeapFunction(uint8_t)
    GenerateBuiltinToLuaFromHeapFunction(uint16_t)
    GenerateBuiltinToLuaFromHeapFunction(uint32_t)
    GenerateBuiltinToLuaFromHeapFunction(uint64_t)
    GenerateBuiltinToLuaFromHeapFunction(float)
    GenerateBuiltinToLuaFromHeapFunction(double)
#undef GenerateBuiltinToLuaFromHeapFunction

)";
    }

    auto CodeGenerator::generate_utility_member_functions() const -> std::string
    {
        return R"(inline auto deref(void* ptr, uint32_t num) -> void*
{
    if (num == 0) { return ptr; }
    void* final_ptr{ptr};
    for (uint32_t i = 0; i < num; ++i)
    {
        final_ptr = *static_cast<void**>(final_ptr);
    }
    return final_ptr;
}

template<StringLiteral self_target_metatable_name, typename UserdataFullType, std::unordered_set<std::string>& ConvertibleToMap>
inline auto lua_util_userdata_Get(lua_State* lua_state, int param_stack_index) -> UserdataFullType
{
    using UserdataType = std::remove_pointer_t<std::remove_reference_t<UserdataFullType>>;

    UserdataType* obj_ptr{};
    if (lua_isuserdata(lua_state, param_stack_index) || lua_isnil(lua_state, param_stack_index))
    {
        if (lua_isnil(lua_state, param_stack_index))
        {
            if constexpr (!std::is_pointer_v<UserdataFullType>)
            {
                auto error_message = std::format("userdata '{}' is a reference and therefore cannot be nil", std::string_view{self_target_metatable_name.value});
                luaL_argerror(lua_state, param_stack_index + 1, error_message.c_str());
            }
            else
            {
                return nullptr;
            }
        }

        lua_getmetatable(lua_state, param_stack_index);
        lua_pushliteral(lua_state, "__name");
        lua_rawget(lua_state, -2);
        auto metatable_name = std::string{lua_tostring(lua_state, -1)};
        lua_pop(lua_state, 2);
        if (ConvertibleToMap.contains(metatable_name))
        {
            lua_getiuservalue(lua_state, param_stack_index, 1);
            int pointer_depth = lua_tointeger(lua_state, -1);
            bool is_pointer = pointer_depth > 0;
            if (is_pointer)
            {
                auto* outer_most_container = lua_touserdata(lua_state, param_stack_index);
                obj_ptr = *static_cast<UserdataType**>(deref(outer_most_container, pointer_depth - 1));
            }
            else
            {
                obj_ptr = static_cast<UserdataType*>(lua_touserdata(lua_state, param_stack_index));
            }
            lua_pop(lua_state, 1);
        }
        else
        {
            auto error_message = std::format("userdata was '{}', expected '{}' or derivative", metatable_name, std::string_view{self_target_metatable_name.value});
            luaL_argerror(lua_state, param_stack_index + 1, error_message.c_str());
        }
    }
    else
    {
        auto error_message = std::format("expected userdata '{}', got non-userdata '{}'", std::string_view{self_target_metatable_name.value}, lua_typename(lua_state, lua_type(lua_state, param_stack_index)));
        luaL_argerror(lua_state, param_stack_index + 1, error_message.c_str());
    }

    if constexpr (!std::is_pointer_v<UserdataFullType>)
    {
        return *obj_ptr;
    }
    else
    {
        return obj_ptr;
    }
}

template<StringLiteral self_target_metatable_name, typename SelfType, std::unordered_set<std::string>& ConvertibleToMap, typename GetSelfFunctionT, GetSelfFunctionT GetSelfFunction>
inline auto lua_util_userdata_member_function_wrapper_Set(lua_State* lua_state) -> int
{
    try
    {
        auto [_, self] = GetSelfFunction(lua_state);

        if (lua_isuserdata(lua_state, 1) || lua_isnil(lua_state, 1))
        {
            if (lua_isuserdata(lua_state, 1))
            {
                lua_getmetatable(lua_state, 1);
                lua_pushliteral(lua_state, "__name");
                lua_rawget(lua_state, -2);
                auto metatable_name = std::string{lua_tostring(lua_state, -1)};
                lua_pop(lua_state, 1);
                if (ConvertibleToMap.contains(metatable_name))
                {
                    lua_getiuservalue(lua_state, 1, 1);
                    int pointer_depth = lua_tointeger(lua_state, -1);
                    bool is_pointer = pointer_depth > 0;
                    if (is_pointer)
                    {
                        auto* outer_most_container = lua_touserdata(lua_state, 1);
                        *self = *static_cast<SelfType**>(deref(outer_most_container, pointer_depth - 1));
                    }
                    else
                    {
                        *self = static_cast<SelfType*>(lua_touserdata(lua_state, 1));
                    }
                    lua_pop(lua_state, 3);
                }
                else
                {
                    auto error_message = std::format("self was '{}', expected '{}' or derivative", metatable_name, std::string_view{self_target_metatable_name.value});
                    luaL_argerror(lua_state, 2, error_message.c_str());
                }
            }
            else // value is nil (treat as nullptr)
            {
                *self = nullptr;
            }
        }

        return 0;
    }
    catch (std::exception& e)
    {
        luaL_error(lua_state, e.what());
        return 0;
    }
}

inline auto lua_util_userdata_member_function_wrapper_Get(lua_State* lua_state) -> int
{
    try
    {
        return 1;
    }
    catch (std::exception& e)
    {
        luaL_error(lua_state, e.what());
        return 0;
    }
}

template<typename GetSelfFunctionT, GetSelfFunctionT GetSelfFunction>
inline auto lua_util_userdata_member_function_wrapper_IsValid(lua_State* lua_state) -> int
{
    try
    {
        auto [_, container_self] = GetSelfFunction(lua_state);
        lua_pushboolean(lua_state, container_self && *container_self ? true : false);
        return 1;
    }
    catch (std::exception& e)
    {
        luaL_error(lua_state, e.what());
        return 0;
    }
}

template<typename GetSelfFunctionT, GetSelfFunctionT GetSelfFunction>
inline auto lua_util_userdata_member_function_wrapper_GetAddress(lua_State* lua_state) -> int
{
    try
    {
        auto [is_pointer, container_self] = GetSelfFunction(lua_state);
        auto pointer = is_pointer ? (container_self ? *container_self : nullptr) : static_cast<void*>(container_self);
        lua_pushinteger(lua_state, std::bit_cast<lua_Integer>(pointer));
        return 1;
    }
    catch (std::exception& e)
    {
        luaL_error(lua_state, e.what());
        return 0;
    }
}

template<StringLiteral MetatableName, typename ItemValueType, bool IsItemPointer = true, bool MoveConstruct = false>
inline auto lua_Userdata_to_lua_from_heap(lua_State* lua_state, std::conditional_t<IsItemPointer, void*, ItemValueType&> item, uint32_t pointer_depth = 1) -> void
{
    auto* userdata = static_cast<ItemValueType*>(lua_newuserdatauv(lua_state, sizeof(std::conditional_t<IsItemPointer, ItemValueType*, ItemValueType>), 1));
    lua_pushinteger(lua_state, pointer_depth);
    lua_setiuservalue(lua_state, -2, 1);
    if constexpr (IsItemPointer)
    {
        new(userdata) ItemValueType*{static_cast<ItemValueType*>(item)};
    }
    else
    {
        if constexpr (MoveConstruct)
        {
            new(userdata) ItemValueType{std::move(item)};
        }
        else
        {
            new(userdata) ItemValueType{item};
        }
    }
    luaL_getmetatable(lua_state, MetatableName.value);
    lua_setmetatable(lua_state, -2);
})";
    }

    auto CodeGenerator::generate_state_file_pre() const -> std::string
    {
        return R"(// https://stackoverflow.com/a/45365798
template<typename Callable>
union storage
{
    storage() {}
    std::decay_t<Callable> callable;
};

template<int, typename Callable, typename Ret, typename... Args>
auto fnptr_(Callable&& c, Ret (*)(Args...))
{
    static bool used = false;
    static storage<Callable> s;
    using type = decltype(s.callable);

    if (used)
    {
        s.callable.~type();
    }
    new(&s.callable) type(std::forward<Callable>(c));
    used = true;

    return [](Args... args) -> Ret {
        return Ret(s.callable(std::forward<Args>(args)...));
    };
}

template<typename Fn, int N = 0, typename Callable>
Fn* fnptr(Callable&& c)
{
    return fnptr_<N>(std::forward<Callable>(c), (Fn*)nullptr);
}

auto inline resolve_status_message(lua_State* lua_state, int status) -> std::string
{
    auto status_to_string = [](int status) -> std::string {
        switch (status)
        {
            case LUA_YIELD:
                return "LUA_YIELD";
            case LUA_ERRRUN:
                return "LUA_ERRRUN";
            case LUA_ERRSYNTAX:
                return "LUA_ERRSYNTAX";
            case LUA_ERRMEM:
                return "LUA_ERRMEM";
            case LUA_ERRERR:
                return "LUA_ERRERR";
            case LUA_ERRFILE:
                return "LUA_ERRFILE";
        }

        return "Unknown error";
    };

    if (lua_isstring(lua_state, -1))
    {
        const char* status_message = lua_tostring(lua_state, -1);
        lua_pop(lua_state, 1);
        return std::format("{} => {}", status_to_string(status), status_message);
    }
    else
    {
        return std::format("{} => No message", status_to_string(status));
    }
};

// https://ctrpeach.io/posts/cpp20-string-literal-template-parameters/
template<size_t N>
struct StringLiteral
{
    constexpr StringLiteral(const char (& str)[N])
    {
        std::copy_n(str, N, value);
    }

    char value[N];
};

struct FunctionProto
{
    void* function_pointer{};
    using LuaWrapperFunctionPointer = int(*)(lua_State*);
    LuaWrapperFunctionPointer lua_wrapper_function_function_pointer{};
};)";
    }

    auto CodeGenerator::generate_state_file() const -> void
    {
        for (const auto& lua_state_type : m_container.lua_state_types)
        {
            std::filesystem::path state_file = m_output_path / std::format("include/LuaBindings/States/{}/Main.hpp", lua_state_type);
            auto file = File::open(m_output_path / state_file, File::OpenFor::Writing, File::OverwriteExistingFile::Yes, File::CreateIfNonExistent::Yes);

            std::string file_contents = std::format("#ifndef LUAWRAPPERGENERATOR_{}_MAIN_HPP\n#define LUAWRAPPERGENERATOR_{}_MAIN_HPP\n\n", lua_state_type, lua_state_type);
            file_contents.append("#include <string>\n");
            file_contents.append("#include <format>\n");
            file_contents.append("\n");
            file_contents.append("#include <lua.hpp>\n");

            std::unordered_set<std::string> includes{};
            for (const auto&[_, the_class] : m_container.classes)
            {
                includes.emplace(the_class.full_path_to_file);

                for (const auto&[_, member_function] : the_class.container.functions)
                {
                    if (member_function.shares_file_with_containing_class()) { continue; }
                    includes.emplace(member_function.get_full_path_to_file());
                }
            }
            for (const auto&[_, free_function] : m_container.functions)
            {
                includes.emplace(free_function.get_full_path_to_file());
            }
            for (const auto& extra_include : get_container().extra_includes)
            {
                includes.emplace(extra_include);
            }

            for (const auto& file_to_include : includes)
            {
                file_contents.append(std::format("#include \"{}\"\n", file_to_include));
            }

            file_contents.append("\nnamespace RC::LuaBindings\n{\n");

            file_contents.append(generate_state_file_pre());

            file_contents.append("\n\n");

            file_contents.append(generate_utility_member_functions());

            file_contents.append("\n\n");

            bool state_file_pre_type_patchs_applied{};
            for (const auto& type_patch : m_type_patches)
            {
                auto type_patch_contents = type_patch.generate_state_file_pre(m_container);
                if (!type_patch_contents.empty())
                {
                    file_contents.append(type_patch_contents);
                    state_file_pre_type_patchs_applied = true;
                }
            }

            if (state_file_pre_type_patchs_applied)
            {
                file_contents.append("\n\n");
            }

            file_contents.append(generate_convertible_to_set());

            file_contents.append("\n\n");

            file_contents.append(generate_function_proto_metatable());

            for (const auto&[_, func_proto] : m_container.function_proto_container)
            {
                file_contents.append(func_proto->generate_lua_wrapper_function());
            }

            file_contents.append("\n\n");

            for (const auto&[_, the_class] : m_container.classes)
            {
                if (auto constructor_contents = the_class.generate_constructor(); !constructor_contents.empty())
                {
                    file_contents.append(constructor_contents);
                    file_contents.append("\n\n");
                }
                file_contents.append(the_class.generate_internal_get_self_function());
                file_contents.append("\n\n");
                file_contents.append(the_class.generate_member_functions());
                file_contents.append("\n\n");
            }


            for (const auto&[_, the_class] : m_container.classes)
            {
                file_contents.append(the_class.generate_member_functions_map());
                file_contents.append("\n");
                file_contents.append(the_class.generate_metamethods_map());
                file_contents.append("\n");
                file_contents.append(the_class.generate_setup_function());
                file_contents.append("\n");

                // This is commented out until I implement constructor support.
                // Right now, there's no support for them at all which means it's impossible to construct the object if it can't be default constructed.
                // I also don't have the possibility to check whether it can be default constructed either so for now we just can't generate this helper function.
                //file_contents.append(the_class.generate_create_instance_of_function());
                //file_contents.append("\n");

                file_contents.append("\n");
            }

            file_contents.append(generate_builtin_to_lua_from_heap_functions());

            file_contents.append(std::format("{}", generate_free_functions()));
            file_contents.append(std::format("\n{}", generate_lua_setup_global_free_functions()));
            file_contents.append(std::format("\n{}", generate_lua_setup_enums()));

            file_contents.append(std::format("\n{}", generate_lua_setup_state_functions()));

            for (const auto& type_patch : m_type_patches)
            {
                file_contents.append(type_patch.generate_state_file_post(m_container));
            }

            file_contents.append(std::format("}} // RC::LuaBindings\n\n#endif //LUAWRAPPERGENERATOR_{}_MAIN_HPP\n", lua_state_type));

            file.write_string_to_file(File::StringType{file_contents.begin(), file_contents.end()});
            file.close();
        }
    }
}