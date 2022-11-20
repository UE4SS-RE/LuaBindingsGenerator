#include <iostream>
#include <vector>
#include <format>

#include <ArgsParser/ArgsParser.hpp>
#include <Timer/ScopedTimer.hpp>
#include <Helpers/String.hpp>
#include <LuaWrapperGenerator/CodeParser.hpp>
#include <LuaWrapperGenerator/CodeGenerator.hpp>

#include <LuaWrapperGenerator/Patches/Unreal.hpp>

using namespace RC;
using namespace RC::LuaWrapperGenerator;

auto parse_cxx(const std::filesystem::path& output_path, std::vector<std::string>& files2, std::vector<const char*>& compiler_flags2) -> void
{
    static const std::filesystem::path project_code_root{"D:\\VisualStudio\\source\\repos\\RC\\ue4ss_rewritten"};
    static const std::filesystem::path code_output_path{"D:\\VisualStudio\\source\\repos\\RC\\LuaWrapperGenerator_Testing\\LuaBindings"};
    //static constexpr File::StringViewType file{STR("D:\\VisualStudio\\source\\repos\\RC\\LuaWrapperGenerator_Testing\\src\\LuaWrapperGenerator_Testing\\FileToParse.cpp")};
    //static constexpr File::StringViewType file{STR("D:\\VisualStudio\\source\\repos\\RC\\LuaWrapperGenerator_Testing\\include\\LuaWrapperGenerator_Testing\\MyStruct.hpp")};
    static std::vector<std::string> files{
            "D:\\VisualStudio\\source\\repos\\RC\\ue4ss_rewritten\\Dependencies\\Unreal\\src\\FOutputDevice.cpp",
            "D:\\VisualStudio\\source\\repos\\RC\\ue4ss_rewritten\\src\\main_ue4ss_rewritten.cpp",
            "D:\\VisualStudio\\source\\repos\\RC\\ue4ss_rewritten\\src\\UE4SSProgram.cpp",
            "D:\\VisualStudio\\source\\repos\\RC\\ue4ss_rewritten\\src\\Mod.cpp",
            "D:\\VisualStudio\\source\\repos\\RC\\ue4ss_rewritten\\src\\LuaCustomMemberFunctions.cpp",
            "D:\\VisualStudio\\source\\repos\\RC\\ue4ss_rewritten\\src\\LuaScriptMemoryAccess.cpp",
            "D:\\VisualStudio\\source\\repos\\RC\\ue4ss_rewritten\\src\\LuaTests.cpp",
            "D:\\VisualStudio\\source\\repos\\RC\\ue4ss_rewritten\\Dependencies\\Unreal\\src\\UObjectGlobals.cpp",
            "D:\\VisualStudio\\source\\repos\\RC\\ue4ss_rewritten\\Dependencies\\Unreal\\src\\UObject.cpp",
            "D:\\VisualStudio\\source\\repos\\RC\\ue4ss_rewritten\\Dependencies\\Unreal\\src\\UScriptStruct.cpp",
            "D:\\VisualStudio\\source\\repos\\RC\\ue4ss_rewritten\\Dependencies\\Unreal\\src\\UStruct.cpp",
            "D:\\VisualStudio\\source\\repos\\RC\\ue4ss_rewritten\\Dependencies\\Unreal\\src\\World.cpp",
    };
    static std::vector<const char*> compiler_flags{
            "-std=c++20",
            //"-ID:\\VisualStudio\\source\\repos\\RC\\ue4ss_rewritten\\Dependencies\\PolyHook_2_0",
            //"-ID:\\VisualStudio\\source\\repos\\RC\\ue4ss_rewritten\\Dependencies\\PolyHook_2_0\\capstone\\include",
            //"-ID:\\VisualStudio\\source\\repos\\RC\\ue4ss_rewritten\\Dependencies\\PolyHook_2_0\\zydis\\include",
            //"-ID:\\VisualStudio\\source\\repos\\RC\\ue4ss_rewritten\\Dependencies\\PolyHook_2_0\\zydis\\dependencies\\zycore\\include",
            //"-ID:\\VisualStudio\\source\\repos\\RC\\ue4ss_rewritten\\Dependencies\\PolyHook_2_0\\zydis\\msvc",
            "-ID:\\VisualStudio\\source\\repos\\RC\\ue4ss_rewritten\\include",
            "-ID:\\VisualStudio\\source\\repos\\RC\\ue4ss_rewritten\\generated_include",
            "-ID:\\VisualStudio\\source\\repos\\RC\\ue4ss_rewritten\\Dependencies\\File\\include",
            "-ID:\\VisualStudio\\source\\repos\\RC\\ue4ss_rewritten\\Dependencies\\DynamicOutput\\include",
            "-ID:\\VisualStudio\\source\\repos\\RC\\ue4ss_rewritten\\Dependencies\\Unreal\\include",
            "-ID:\\VisualStudio\\source\\repos\\RC\\ue4ss_rewritten\\Dependencies\\Unreal\\generated_include",
            "-ID:\\VisualStudio\\source\\repos\\RC\\ue4ss_rewritten\\Dependencies\\SinglePassSigScanner\\include",
            "-ID:\\VisualStudio\\source\\repos\\RC\\ue4ss_rewritten\\Dependencies\\Constructs\\include",
            "-ID:\\VisualStudio\\source\\repos\\RC\\ue4ss_rewritten\\Dependencies\\Helpers\\include",
            "-ID:\\VisualStudio\\source\\repos\\RC\\ue4ss_rewritten\\Dependencies\\Function\\include",
            //"-ID:\\VisualStudio\\source\\repos\\RC\\ue4ss_rewritten\\Output\\Inter\\x64\\Release\\Dependencies\\PolyHook_2_0\\zydis",
            //"-ID:\\VisualStudio\\source\\repos\\RC\\ue4ss_rewritten\\Output\\Inter\\x64\\Release\\Dependencies\\PolyHook_2_0\\zydis\\dependencies\\zycore",
            "-ID:\\VisualStudio\\source\\repos\\RC\\ue4ss_rewritten\\Dependencies\\LuaMadeSimple\\include",
            "-ID:\\VisualStudio\\source\\repos\\RC\\ue4ss_rewritten\\Dependencies\\LuaRaw\\include",
            "-ID:\\VisualStudio\\source\\repos\\RC\\ue4ss_rewritten\\Dependencies\\FunctionTimer\\include",
            "-ID:\\VisualStudio\\source\\repos\\RC\\ue4ss_rewritten\\Dependencies\\IniParser\\include",
            "-ID:\\VisualStudio\\source\\repos\\RC\\ue4ss_rewritten\\Dependencies\\ParserBase\\include",
            //"-ID:\\VisualStudio\\source\\repos\\RC\\ue4ss_rewritten\\Dependencies\\JSON\\include",
            "-ID:\\VisualStudio\\source\\repos\\RC\\ue4ss_rewritten\\Dependencies\\Input\\include",
            "-ID:\\VisualStudio\\source\\repos\\RC\\ue4ss_rewritten\\Dependencies\\fmt\\include",
            "-ID:\\VisualStudio\\source\\repos\\RC\\ue4ss_rewritten\\Dependencies\\MProgram\\include",
            "-ID:\\VisualStudio\\source\\repos\\RC\\ue4ss_rewritten\\Dependencies\\ScopedTimer\\include",
            "-ID:\\VisualStudio\\source\\repos\\RC\\ue4ss_rewritten\\Dependencies\\imgui",
            "-ID:\\VisualStudio\\source\\repos\\RC\\ue4ss_rewritten\\Dependencies\\imgui-ImGuiColorTextEdit",
            "-DRC_DYNAMIC_OUTPUT_BUILD_STATIC",
            "-DRC_FILE_BUILD_STATIC",
            "-DRC_FUNCTION_TIMER_BUILD_STATIC",
            "-DRC_INI_PARSER_BUILD_STATIC",
            "-DRC_INPUT_BUILD_STATIC",
            "-DRC_JSON_BUILD_STATIC",
            "-DRC_JSON_PARSER_BUILD_STATIC",
            "-DRC_LUA_MADE_SIMPLE_BUILD_STATIC",
            "-DRC_LUA_WRAPPER_GENERATOR_BUILD_STATIC",
            "-DRC_PARSER_BASE_BUILD_STATIC",
            "-DRC_SINGLE_PASS_SIG_SCANNER_BUILD_STATIC",
            "-DRC_SINGLE_PASS_SIG_SCANNER_STATIC",
            "-DRC_UNREAL_BUILD_STATIC",
            "-Dxinput1_3_EXPORTS",
    };

    for (const auto& file : files)
    {
        printf_s("src: %s\n", file.c_str());
    }

    for (const auto& compiler_flag : compiler_flags)
    {
        printf_s("flag: %s\n", compiler_flag);
    }

    LuaWrapperGenerator::CodeParser code_parser{files, compiler_flags.data(), static_cast<int>(compiler_flags.size()), output_path, project_code_root};
    code_parser.add_type_patch(TypePatch{
        .generate_state_file_pre = &TypePatches::Unreal::generate_state_file_pre,
        .generate_state_file_post = &TypePatches::Unreal::generate_state_file_post,
        .cxtype_to_type = &TypePatches::Unreal::cxtype_to_type,
        .cxtype_to_type_post = &TypePatches::Unreal::cxtype_to_type_post,
        .generate_lua_setup_state_function_post = &TypePatches::Unreal::generate_lua_setup_state_function_post,
        .generate_per_class_static_functions = &TypePatches::Unreal::generate_per_class_static_functions,
    });
    const auto& parser_output = code_parser.parse();
    printf_s("Generating code\n");
    double timer_dur{};
    {
        ScopedTimer timer(&timer_dur);
        parser_output.generate_lua_setup_file();
        parser_output.generate_state_file();
    }
    printf_s("Code generation took %f seconds.\n", timer_dur);
}

auto main(int argc, char* argv[]) -> int
{
    try
    {
        ArgsParser args_parser{argc, argv, {
            "output",
            "sources",
            "compiler_flags",
        }};
        auto output_path = args_parser.get_arg("output");
        auto sources = args_parser.get_arg_as_vector("sources");
        auto compiler_flags = args_parser.get_arg_as_vector("compiler_flags");
        
        std::vector<const char*> compiler_flags_raw{};

        for (const auto& compiler_flag : compiler_flags)
        {
            compiler_flags_raw.emplace_back(compiler_flag.c_str());
        }

        //if (output_path.empty()) { throw std::runtime_error{"The output path cannot be empty"}; }
        printf_s("Output Path: %s\n", output_path.c_str());
        printf_s("Generating Lua bindings...\n");

        parse_cxx(output_path, sources, compiler_flags_raw);
    }
    catch (std::runtime_error& e)
    {
        printf_s("Error: %s\n", e.what());
    }

    printf_s("Done.\n");

    return 0;
}
