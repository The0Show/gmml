// thanks to Adaf for helping me figure out the proxy dll loading stuff

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <filesystem>

#include "../include/sigscan.h"

#include "../lib/minhook/include/MinHook.h"

#include <fstream>
#include <iterator>
#include <vector>

#include <iostream>
#include <assert.h>

// .NET CLR hosting
// https://github.com/dotnet/docs/blob/main/docs/core/tutorials/netcore-hosting.md
#include "../lib/nethost/nethost.h"
#include "../lib/nethost/coreclr_delegates.h"
#include "../lib/nethost/hostfxr.h"

constexpr auto PROXY_DLL = TEXT("version.dll");
constexpr auto PROXY_MAX_PATH = 260;

#define DLL_PROXY_ORIGINAL(name) original_##name

#define DLL_NAME(name)                \
    FARPROC DLL_PROXY_ORIGINAL(name); \
    void _##name() {                  \
        DLL_PROXY_ORIGINAL(name)();   \
    }
#include "../include/proxy.h"
#undef DLL_NAME

std::filesystem::path getSystemDirectory() {
    const auto system_directory(std::make_unique<TCHAR[]>(PROXY_MAX_PATH));
    ::GetSystemDirectory(system_directory.get(), PROXY_MAX_PATH);
    return std::filesystem::path(system_directory.get());
}

bool loadProxy() {
    const auto libPath = getSystemDirectory() / PROXY_DLL;
    const auto lib = LoadLibrary(libPath.c_str());
    if(!lib) return false;

    #define DLL_NAME(name) DLL_PROXY_ORIGINAL(name) = GetProcAddress(lib, ###name);
    #include "../include/proxy.h"
    #undef DLL_NAME

    return true;
}

#include "../include/gmml.h"
auto settings = gmmlSettings();

void loadSettings(const char* path) {
    std::ifstream input(path);
    auto size = input.gcount();
    auto setting = std::string();

    while(input >> setting) {
        if(setting == "debug") settings.debug = true;
        else if(setting == "console") settings.showConsole = true;
    }

    input.close();
}

uintptr_t mmAllocAddress = 0x0;
uintptr_t mmFreeAddress = 0x0;
uintptr_t p_gGameFileNameAddress = 0x0;
uintptr_t LoadSave_ReadBundleFileAddress = 0x0;

uintptr_t InitGMLFunctionsAddress = 0x0;
uintptr_t Function_AddAddress = 0x0;
uintptr_t YYErrorAddress = 0x0;
uintptr_t YYGetBoolAddress = 0x0;
uintptr_t YYGetFloatAddress = 0x0;
uintptr_t YYGetInt32Address = 0x0;
uintptr_t YYGetInt64Address = 0x0;
uintptr_t YYGetPtrAddress = 0x0;
uintptr_t YYGetRealAddress = 0x0;
uintptr_t YYGetStringAddress = 0x0;
uintptr_t YYGetUint32Address = 0x0;
uintptr_t YYCreateStringAddress = 0x0;
uintptr_t ARRAY_RefAllocAddress = 0x0;
uintptr_t SET_RValue_ArrayAddress = 0x0;
uintptr_t GET_RValueAddress = 0x0;

#include <Psapi.h>
#include <processthreadsapi.h>

bool findAddresses() {
    const auto base = reinterpret_cast<uintptr_t>(GetModuleHandle(0));
    MODULEINFO info;
    GetModuleInformation(GetCurrentProcess(), GetModuleHandle(0), &info, sizeof(MODULEINFO));

#define find(pattern) (uintptr_t)findPattern((PBYTE)base, info.SizeOfImage, pattern)

    // somebody please save my code from this monstrosity
    mmAllocAddress = find("40 53 56 57 48 81 ec 50 04 00 00 48 8b ?? ?? ?? ?? ?? 48 33 c4 48 89 ?? ?? ?? ?? ?? ?? 41 ?? ?? ?? 48 8b ?? 48 85 ?? 75 ?? 33 c0 e9 ?? ?? ?? ?? e8 ?? ?? ?? ?? 48 8b d8 48 85 c0");

    mmFreeAddress = find("48 85 c9 0f 84 ?? ?? ?? ?? 53 48 83 ec 30 48 8b d9 48 8b ?? ?? ?? ?? ?? 48 85 c9 75 ?? b9 08 00 00 00 e8 ?? ?? ?? ?? 48 89 ?? ?? ?? ?? ?? 48 8d ?? ?? ?? ?? ?? 48 8b c8 e8 ?? ?? ?? ?? 48 8b ?? ?? ?? ?? ??");

    auto p_gGameFileNameAddressTemp = find("8d ?? ?? 03 f1 48 63 ce 45 0f b6 cd 41 b8 ?? ?? ?? ?? 48 8d ?? ?? ?? ?? ?? e8 ?? ?? ?? ?? 48 8b f8 48 8b ?? ?? ?? ?? ?? 48 89 ?? ?? ?? ?? ?? e8 ?? ?? ?? ?? 48 8b ?? ?? ?? ?? ?? e8 ?? ?? ?? ?? 4c 8b ?? ?? ?? ?? ?? 8b d6 48 8b cf");

    p_gGameFileNameAddressTemp += 47;
    auto p_gGameFileNameAddressTemp2 =
        (uintptr_t)*(unsigned char*)(p_gGameFileNameAddressTemp - 4) << 0x0 |
        (uintptr_t)*(unsigned char*)(p_gGameFileNameAddressTemp - 3) << 0x8 |
        (uintptr_t)*(unsigned char*)(p_gGameFileNameAddressTemp - 2) << 0x10 |
        (uintptr_t)*(unsigned char*)(p_gGameFileNameAddressTemp - 1) << 0x18;
    p_gGameFileNameAddress = p_gGameFileNameAddressTemp + p_gGameFileNameAddressTemp2;

    LoadSave_ReadBundleFileAddress = find("40 53 48 81 ec 30 08 00 00 48 8b ?? ?? ?? ?? ?? 48 33 c4 48 89 ?? ?? ?? ?? ?? ?? 48 8b da 4c 8b c1 ba 00 08 00 00 48 8d ?? ?? ?? e8 ?? ?? ?? ?? 48 8b d3 48 8d ?? ?? ?? e8 ?? ?? ?? ?? 48 8b ?? ?? ?? ?? ?? ?? 48 33 cc e8 ?? ?? ?? ?? 48 81 c4 30 08 00 00 5b c3");

    // TODO: use sigscanning
    // current addresses are for 2022.3.0.497 and probably only work on Will you Snail
    InitGMLFunctionsAddress = base + 0x1e3400;
    Function_AddAddress = base + 0x19f960;
    YYErrorAddress = base + 0x1bc4d0;
    YYGetBoolAddress = base + 0x1a3a10;
    YYGetFloatAddress = base + 0x1a3b00;
    YYGetInt32Address = base + 0x1a3c60;
    YYGetInt64Address = base + 0x1a3d70;
    YYGetPtrAddress = base + 0x1a3e90;
    YYGetRealAddress = base + 0x1a3f70;
    YYGetStringAddress = base + 0x1a40a0;
    YYGetUint32Address = base + 0x1a4270;
    YYCreateStringAddress = base + 0x1bc450;
    ARRAY_RefAllocAddress = base + 0x1a02b0;
    SET_RValue_ArrayAddress = base + 0x1a24a0;
    GET_RValueAddress = base + 0x1a0fc0;

#undef find

    return mmAllocAddress != 0x0 &&
        mmFreeAddress != 0x0 &&
        p_gGameFileNameAddressTemp != 0x0 &&
        LoadSave_ReadBundleFileAddress != 0x0 &&
        InitGMLFunctionsAddress != 0x0 &&
        Function_AddAddress != 0x0 &&
        YYErrorAddress != 0x0 &&
        YYGetBoolAddress != 0x0 &&
        YYGetFloatAddress != 0x0 &&
        YYGetInt32Address != 0x0 &&
        YYGetInt64Address != 0x0 &&
        YYGetPtrAddress != 0x0 &&
        YYGetRealAddress != 0x0 &&
        YYGetStringAddress != 0x0 &&
        YYGetUint32Address != 0x0 &&
        YYCreateStringAddress != 0x0 &&
        ARRAY_RefAllocAddress != 0x0 &&
        SET_RValue_ArrayAddress != 0x0 &&
        GET_RValueAddress != 0x0;
}

void* __cdecl mmAlloc(unsigned long long size, char const* why, int unk2, bool unk3) {
    return ((void* (__cdecl*)(unsigned long long, char const*, int, bool))mmAllocAddress)(size, why, unk2, unk3);
}

static void __cdecl mmFree(void const* block) {
    ((void (__cdecl*)(void const*))mmFreeAddress)(block);
}

#include "../include/gmrunner.h"
void __cdecl Function_Add(char const* name, GML_Call function, int argCount, bool unk) {
    ((Function_AddType*)Function_AddAddress)(name, function, argCount, unk);
}
void __cdecl YYError(char* error) {
    ((void (__cdecl*)(char*, ...))YYErrorAddress)(error);
}

bool __cdecl YYGetBool(RValue* arg, int argindex) {
    return ((bool (__cdecl*)(RValue*, int))YYGetBoolAddress)(arg, argindex);
}
float_t __cdecl YYGetFloat(RValue* arg, int argindex) {
    return ((float_t (__cdecl*)(RValue*, int))YYGetFloatAddress)(arg, argindex);
}
int32_t __cdecl YYGetInt32(RValue* arg, int argindex) {
    return ((int32_t (__cdecl*)(RValue*, int))YYGetInt32Address)(arg, argindex);
}
int64_t __cdecl YYGetInt64(RValue* arg, int argindex) {
    return ((int64_t (__cdecl*)(RValue*, int))YYGetInt64Address)(arg, argindex);
}
void* __cdecl YYGetPtr(RValue* arg, int argindex) {
    return ((void* (__cdecl*)(RValue*, int))YYGetPtrAddress)(arg, argindex);
}
double_t __cdecl YYGetReal(RValue* arg, int argindex) {
    return ((double (__cdecl*)(RValue*, int))YYGetRealAddress)(arg, argindex);
}
char* __cdecl YYGetString(RValue* arg, int argindex) {
    return ((char* (__cdecl*)(RValue*, int))YYGetStringAddress)(arg, argindex);
}
uint32_t __cdecl YYGetUint32(RValue* arg, int argindex) {
    return ((uint32_t (__cdecl*)(RValue*, int))YYGetUint32Address)(arg, argindex);
}
void __cdecl YYCreateString(RValue* value, char* str) {
    ((void (__cdecl*)(RValue*, char*))YYCreateStringAddress)(value, str);
}
RefDynamicArrayOfRValue* __cdecl ARRAY_RefAlloc() {
    return ((RefDynamicArrayOfRValue* (__cdecl*)())ARRAY_RefAllocAddress)();
}
void __cdecl SET_RValue_Array(RValue* arr, RValue* value, YYObjectBase* unk, int index) {
    ((void (__cdecl*)(RValue*, RValue*, YYObjectBase*, int))SET_RValue_ArrayAddress)(arr, value, unk, index);
}
bool __cdecl GET_RValue(RValue* value, RValue* arr, YYObjectBase* unk, int index, bool unk1, bool unk2) {
    return ((bool (__cdecl*)(RValue*, RValue*, YYObjectBase*, int, bool, bool))GET_RValueAddress)(value, arr, unk, index, unk1, unk2);
}

using string_t = std::basic_string<char_t>;

namespace {
    // Globals to hold hostfxr exports
    hostfxr_initialize_for_runtime_config_fn init_fptr;
    hostfxr_get_runtime_delegate_fn get_delegate_fptr;
    hostfxr_close_fn close_fptr;

    // Forward declarations
    bool load_hostfxr();
    load_assembly_and_get_function_pointer_fn get_dotnet_load_assembly(const char_t* assembly);
}

unsigned char* (CORECLR_DELEGATE_CALLTYPE* modifyDataManaged)(int, unsigned char*, int*);
void (CORECLR_DELEGATE_CALLTYPE* InitGMLFunctionsManaged)();
bool startClrHost() {
    if(!load_hostfxr()) {
        MessageBoxA(NULL, "Error when loading hostfxr", NULL, MB_OK);
        return false;
    }

    const string_t config_path = TEXT("gmml\\patcher\\GmmlPatcher.runtimeconfig.json");
    load_assembly_and_get_function_pointer_fn load_assembly_and_get_function_pointer = nullptr;
    load_assembly_and_get_function_pointer = get_dotnet_load_assembly(config_path.c_str());
    if(load_assembly_and_get_function_pointer == nullptr) {
        MessageBoxA(NULL, "Error when starting .NET CLR", NULL, MB_OK);
        return false;
    }

    const string_t dotnetlib_path = TEXT("gmml\\patcher\\GmmlPatcher.dll");

    int rc = load_assembly_and_get_function_pointer(
        dotnetlib_path.c_str(),
        TEXT("GmmlPatcher.Patcher, GmmlPatcher"), // type
        TEXT("ModifyData"), // method
        UNMANAGEDCALLERSONLY_METHOD,
        nullptr,
        (void**)&modifyDataManaged);
    if(rc != 0 || modifyDataManaged == nullptr) {
        MessageBoxA(NULL, "Error when getting GmmlPatcher.Patcher.ModifyData", NULL, MB_OK);
        return false;
    }

    rc = load_assembly_and_get_function_pointer(
        dotnetlib_path.c_str(),
        TEXT("GmmlPatcher.Interop, GmmlPatcher"), // type
        TEXT("InitGmlFunctions"), // method
        UNMANAGEDCALLERSONLY_METHOD,
        nullptr,
        (void**)&InitGMLFunctionsManaged);
    if(rc != 0 || InitGMLFunctionsManaged == nullptr) {
        MessageBoxA(NULL, "Error when getting GmmlPatcher.Interop.InitGmlFunctions", NULL, MB_OK);
        return false;
    }

    if(settings.debug)
        MessageBoxA(NULL, "CLR host loaded", "Info", MB_OK);

    return true;
}

unsigned char* modifyData(int audioGroup, unsigned char* orig, int* size) {
    if(settings.debug) {
        if(audioGroup == 0) MessageBoxA(NULL, "Loading game data", "Info", MB_OK);
        else MessageBoxA(NULL, (std::string("Loading audio group ") + std::to_string(audioGroup)).c_str(), "Info", MB_OK);
    }

    if(modifyDataManaged == nullptr && !startClrHost())
        return orig;

#pragma warning(push)
// startClrHost guarantees not null
#pragma warning(disable : 6011)
    auto bytes = modifyDataManaged(audioGroup, orig, size);
#pragma warning(pop)

    if(bytes != orig) mmFree(orig);
    return bytes;
}

unsigned char* (__cdecl* LoadSave_ReadBundleFile_orig)(char*, int*);
unsigned char* __cdecl LoadSave_ReadBundleFile_hook(char* path, int* size) {
    auto base = reinterpret_cast<uintptr_t>(GetModuleHandle(0));
    auto g_pGameFileName = (char**)p_gGameFileNameAddress;

    auto fsPath = std::filesystem::path(path);
    auto fsPathStem = fsPath.stem().string();
    auto audioGroupName = "audiogroup";

    int* modifySize = size;
    if(size == nullptr) {
        struct stat stat_buf;
        int rc = stat(path, &stat_buf);
        modifySize = (int*)&stat_buf.st_size;
    }

    if(strcmp(path, *g_pGameFileName) == 0) {
        return modifyData(0, LoadSave_ReadBundleFile_orig(path, size), modifySize);
    }
    else if(fsPath.extension() == ".dat" && fsPathStem.starts_with(audioGroupName)) {
        try {
            auto audioGroup = std::stoi(fsPathStem.substr(strlen(audioGroupName)));
            return modifyData(audioGroup, LoadSave_ReadBundleFile_orig(path, size), modifySize);
        }
        catch(std::invalid_argument) { }
    }

    return LoadSave_ReadBundleFile_orig(path, size);
}

void (__cdecl* InitGMLFunctions_orig)();
void __cdecl InitGMLFunctions_hook() {
    if(InitGMLFunctionsManaged != nullptr || startClrHost())
#pragma warning(push)
// startClrHost guarantees not null
#pragma warning(disable : 6011)
        InitGMLFunctionsManaged();
#pragma warning(pop)

    InitGMLFunctions_orig();
}

#pragma warning(push)
#pragma warning(disable : 26812)
bool loadModLoader() {
    loadSettings("gmml.cfg");
    if(settings.showConsole) AllocConsole();

    if(settings.debug) MessageBoxA(NULL, "Loading", "Info", MB_OK);

    if(!findAddresses()) return false;

    if(MH_Initialize() != MH_OK) return false;

    if(MH_CreateHook(reinterpret_cast<void*>(LoadSave_ReadBundleFileAddress), LoadSave_ReadBundleFile_hook,
        reinterpret_cast<void**>(&LoadSave_ReadBundleFile_orig)) != MH_OK)
        return false;

    if (MH_CreateHook(reinterpret_cast<void*>(InitGMLFunctionsAddress), InitGMLFunctions_hook,
        reinterpret_cast<void**>(&InitGMLFunctions_orig)) != MH_OK)
        return false;

    if(MH_EnableHook(MH_ALL_HOOKS) != MH_OK) return false;

    return true;
}
#pragma warning(pop)

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch(ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            if(!loadProxy()) {
                MessageBoxA(NULL, "Failed to load original dll", NULL, MB_OK);
                return FALSE;
            }
            if(!loadModLoader()) {
                MessageBoxA(NULL, "Failed to load mod loader", NULL, MB_OK);
                return FALSE;
            }
            //MessageBoxA(NULL, "ml loaded", "succ", MB_OK);
            break;
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
        case DLL_PROCESS_DETACH:
            break;
    }

    return TRUE;
}

/********************************************************************************************
 * Function used to load and activate .NET Core
 ********************************************************************************************/

namespace {
    // Forward declarations
    void* load_library(const char_t*);
    void* get_export(void*, const char*);

    void* load_library(const char_t* path) {
        HMODULE h = ::LoadLibraryW(path);
        assert(h != nullptr);
        return (void*)h;
    }
    void* get_export(void* h, const char* name) {
        void* f = ::GetProcAddress((HMODULE)h, name);
        assert(f != nullptr);
        return f;
    }

    // <SnippetLoadHostFxr>
    // Using the nethost library, discover the location of hostfxr and get exports
    bool load_hostfxr() {
        // Pre-allocate a large buffer for the path to hostfxr
        char_t buffer[MAX_PATH];
        size_t buffer_size = sizeof(buffer) / sizeof(char_t);
        int rc = get_hostfxr_path(buffer, &buffer_size, nullptr);
        if(rc != 0)
            return false;

        // Load hostfxr and get desired exports
        void* lib = load_library(buffer);
        init_fptr = (hostfxr_initialize_for_runtime_config_fn)get_export(lib, "hostfxr_initialize_for_runtime_config");
        get_delegate_fptr = (hostfxr_get_runtime_delegate_fn)get_export(lib, "hostfxr_get_runtime_delegate");
        close_fptr = (hostfxr_close_fn)get_export(lib, "hostfxr_close");

        return (init_fptr && get_delegate_fptr && close_fptr);
    }
    // </SnippetLoadHostFxr>

    // <SnippetInitialize>
    // Load and initialize .NET Core and get desired function pointer for scenario
    load_assembly_and_get_function_pointer_fn get_dotnet_load_assembly(const char_t* config_path) {
        // Load .NET Core
        void* load_assembly_and_get_function_pointer = nullptr;
        hostfxr_handle cxt = nullptr;
        int rc = init_fptr(config_path, nullptr, &cxt);
        if(rc != 0 || cxt == nullptr) {
            std::cerr << "Init failed: " << std::hex << std::showbase << rc << std::endl;
            close_fptr(cxt);
            return nullptr;
        }

        // Get the load assembly function pointer
        rc = get_delegate_fptr(
            cxt,
            hdt_load_assembly_and_get_function_pointer,
            &load_assembly_and_get_function_pointer);
        if(rc != 0 || load_assembly_and_get_function_pointer == nullptr)
            std::cerr << "Get delegate failed: " << std::hex << std::showbase << rc << std::endl;

        close_fptr(cxt);
        return (load_assembly_and_get_function_pointer_fn)load_assembly_and_get_function_pointer;
    }
    // </SnippetInitialize>
}
