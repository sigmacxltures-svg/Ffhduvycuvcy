// ============================================================
//  il2cpp_helpers.h
//  AArch64 inline detour engine + IL2CPP runtime helpers
//  Unity 2021.3 LTS IL2CPP  |  Android  |  arm64-v8a
//
//  Trampoline layout (16 bytes written at hook site):
//    +0x00  LDR x17, #8      ; 58000051 — load abs addr 8 bytes ahead
//    +0x04  BR  x17          ; D61F0220 — jump to detour
//    +0x08  <8-byte abs address of detour>
//
//  Original stub (mmap'd, executable):
//    +0x00  <4 bytes: original instruction relocated from hook site>
//    +0x04  LDR x17, #8      ; branch back trampoline
//    +0x08  BR  x17
//    +0x0C  <8-byte abs address: hook_site + 4>
// ============================================================
#pragma once

#include <cstdint>
#include <cstring>
#include <cstdio>
#include <dlfcn.h>
#include <sys/mman.h>
#include <android/log.h>
#include <pthread.h>
#include <unistd.h>
#include <functional>
#include <cmath>

// ── IL2CPP offset table ──────────────────────────────────────
// Source: il2cppdumper on Steal a Monke APK v1.x
// Run il2cppdumper again after each update and patch these.
constexpr uintptr_t OFFSET_GETMOVESPEED       = 0x00B4A210;
constexpr uintptr_t OFFSET_CHECKCOLLISION     = 0x00B4C880;
constexpr uintptr_t OFFSET_ADDCOINS           = 0x00B51340;
constexpr uintptr_t OFFSET_RENDEROBJ_FOG      = 0x00B5F480;
constexpr uintptr_t OFFSET_PLAYERUPDATE       = 0x00B49900;
constexpr uintptr_t OFFSET_RENDEROBJECT       = 0x00B5F100;
constexpr uintptr_t OFFSET_RIGIDBODY_FIXED    = 0x00B4B200;
constexpr uintptr_t OFFSET_OVRINPUT_GETDOWN   = 0x00B60010;

// ── page helpers ─────────────────────────────────────────────
static inline uintptr_t PageOf(uintptr_t a) { return a & ~uintptr_t(4095); }

static inline bool WriteExec(uintptr_t addr, const void* data, size_t len) {
    uintptr_t pg = PageOf(addr);
    if (mprotect(reinterpret_cast<void*>(pg), 4096,
                 PROT_READ|PROT_WRITE|PROT_EXEC) != 0) return false;
    memcpy(reinterpret_cast<void*>(addr), data, len);
    __builtin___clear_cache(reinterpret_cast<char*>(addr),
                            reinterpret_cast<char*>(addr+len));
    mprotect(reinterpret_cast<void*>(pg), 4096, PROT_READ|PROT_EXEC);
    return true;
}

// ── inline hook ───────────────────────────────────────────────
static bool HookFunction(uintptr_t target, void* detour, void** original) {
    // allocate executable stub for relocated original bytes
    uint8_t* stub = reinterpret_cast<uint8_t*>(
        mmap(nullptr, 64, PROT_READ|PROT_WRITE|PROT_EXEC,
             MAP_PRIVATE|MAP_ANONYMOUS, -1, 0));
    if (!stub || stub == MAP_FAILED) return false;

    // copy original instruction (4 bytes, one AArch64 insn)
    memcpy(stub, reinterpret_cast<void*>(target), 4);

    // append LDR x17,#8 / BR x17 / <return addr>
    const uint32_t ldr = 0x58000051u;
    const uint32_t br  = 0xD61F0220u;
    memcpy(stub+4, &ldr, 4);
    memcpy(stub+8, &br,  4);
    uintptr_t ret = target + 4;
    memcpy(stub+12, &ret, 8);
    __builtin___clear_cache(reinterpret_cast<char*>(stub),
                            reinterpret_cast<char*>(stub+64));
    *original = stub;

    // write detour trampoline at target
    uint8_t tramp[16];
    memcpy(tramp+0, &ldr, 4);
    memcpy(tramp+4, &br,  4);
    uintptr_t da = reinterpret_cast<uintptr_t>(detour);
    memcpy(tramp+8, &da, 8);
    return WriteExec(target, tramp, 16);
}

// ── module base via /proc/self/maps ──────────────────────────
static uintptr_t GetModuleBase(const char* name) {
    FILE* f = fopen("/proc/self/maps", "r");
    if (!f) return 0;
    char line[512];
    uintptr_t base = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, name) && strstr(line, "r-xp")) {
            base = strtoull(line, nullptr, 16);
            break;
        }
    }
    fclose(f);
    return base;
}

// ── field accessors ───────────────────────────────────────────
static inline float* GetVec3Field(void* obj, uint32_t off) {
    return reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(obj)+off);
}

template<typename T>
static inline T ReadField(void* obj, uint32_t off) {
    T v; memcpy(&v, reinterpret_cast<char*>(obj)+off, sizeof(T)); return v;
}

template<typename T>
static inline void WriteField(void* obj, uint32_t off, T val) {
    memcpy(reinterpret_cast<char*>(obj)+off, &val, sizeof(T));
}

// ── vtable call ───────────────────────────────────────────────
template<typename Ret, typename... Args>
static Ret CallMethod(void* obj, uint32_t slot, Args... args) {
    uintptr_t* vt = *reinterpret_cast<uintptr_t**>(
        reinterpret_cast<uintptr_t>(obj)+0x10);
    return reinterpret_cast<Ret(*)(void*,Args...)>(vt[slot])(obj, args...);
}

// ── object iterator (stub — wire GC walk for production) ─────
using ObjectCallback = std::function<void(void*)>;
static void IL2CPP_ForEachObject(const char*, ObjectCallback) {
    // replace with il2cpp_gchandle_foreach_get_target scan
    // using your dump's TypeIndex for each class
}
