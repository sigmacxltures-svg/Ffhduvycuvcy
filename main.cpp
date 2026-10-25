// ============================================================
//  main.cpp  —  NullClient
//  Target   : Steal a Monke (LarpLogan)  |  arm64-v8a
//  Runtime  : Unity IL2CPP  |  Android 10+  |  Meta Quest 2/3/3S/Pro
//  Entry    : __attribute__((constructor)) — fires automatically
//             when the linker maps libNullClient.so, zero smali
//             edits required. No JNI_OnLoad, no const-string.
//
//  Injection vector:
//    Drop libNullClient.so into APK/lib/arm64-v8a/
//    Add android:extractNativeLibs="true" in AndroidManifest.xml
//    Add <meta-data android:name="com.samsung.android.vr.application.mode"
//                  android:value="vr_only"/>  (already present)
//    The linker loads EVERY .so in lib/arm64-v8a/ at startup —
//    constructor runs before main(), before Unity, before IL2CPP.
//
//  Register/stack notes (AArch64):
//    constructor fires in linker context; sp is 16-byte aligned
//    x0-x7 caller-saved; x19-x28 callee-saved; x29=FP; x30=LR
//    pthread spawned here owns its own stack (default 1 MB)
// ============================================================

#include "il2cpp_helpers.h"
#include "vr_panel.h"
#include "memory.h"

#define TAG "NullClient"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

// ── global toggle state  [0-7] ───────────────────────────────
//   0  Speed Boost       1  No Clip        2  Fly Mode
//   3  Infinite Coins    4  Auto Steal     5  x10 Coins
//   6  ESP               7  No Fog
bool g_toggles[8] = {};

bool   g_menuOpen     = false;
bool   g_bPrevFrame   = false;
int    g_openCategory = -1;
float  g_menuAlpha    = 0.0f;

uintptr_t g_il2cppBase      = 0;
uintptr_t g_ovrGetDown      = 0;

// ── originals ────────────────────────────────────────────────
static float (*orig_GetMoveSpeed)(void*)                   = nullptr;
static bool  (*orig_CheckCollision)(void*,float*,float*)   = nullptr;
static void  (*orig_AddCoins)(void*, int)                  = nullptr;
static void  (*orig_RenderFog)(void*, float)               = nullptr;
static void  (*orig_PlayerUpdate)(void*, float)            = nullptr;
static void  (*orig_OnRenderObject)(void*)                 = nullptr;
static void  (*orig_FixedUpdate)(void*)                    = nullptr;

// ============================================================
//  HOOKS
// ============================================================

float hook_GetMoveSpeed(void* p) {
    float v = orig_GetMoveSpeed(p);
    return g_toggles[0] ? v * 4.5f : v;
}

bool hook_CheckCollision(void* h, float* o, float* d) {
    if (g_toggles[1]) return false;
    return orig_CheckCollision(h, o, d);
}

void hook_AddCoins(void* mgr, int delta) {
    if (g_toggles[3]) {
        *reinterpret_cast<int*>(
            reinterpret_cast<uintptr_t>(mgr) + 0x58) = 0x7FFFFFFF;
        return;
    }
    orig_AddCoins(mgr, g_toggles[5] ? delta * 10 : delta);
}

void hook_RenderFog(void* f, float d) {
    if (!g_toggles[7]) orig_RenderFog(f, d);
}

void hook_FixedUpdate(void* rb) {
    if (g_toggles[2]) {
        *reinterpret_cast<bool*>(
            reinterpret_cast<uintptr_t>(rb) + 0x20) = false;
        reinterpret_cast<float*>(
            reinterpret_cast<uintptr_t>(rb) + 0x28)[1] = 0.0f;
    }
    orig_FixedUpdate(rb);
}

void hook_PlayerUpdate(void* self, float dt) {
    orig_PlayerUpdate(self, dt);

    // B-button edge detect
    bool bNow = reinterpret_cast<bool(*)(uint32_t,int)>(g_ovrGetDown)
                    (0x00000002, 1);
    if (bNow && !g_bPrevFrame) {
        g_menuOpen = !g_menuOpen;
        LOGI("menu %s", g_menuOpen ? "OPEN" : "CLOSED");
    }
    g_bPrevFrame = bNow;

    // Auto-steal proximity sweep
    if (g_toggles[4]) {
        IL2CPP_ForEachObject("MonkeBehaviour", [&](void* other) {
            if (other == self) return;
            float* mp = GetVec3Field(self,  0x38);
            float* tp = GetVec3Field(other, 0x38);
            float dx=mp[0]-tp[0], dy=mp[1]-tp[1], dz=mp[2]-tp[2];
            if (dx*dx+dy*dy+dz*dz < 4.0f)
                CallMethod<void>(other, 0x4C, true);
        });
    }
}

void hook_OnRenderObject(void* cam) {
    orig_OnRenderObject(cam);

    // ESP boxes
    if (g_toggles[6]) {
        IL2CPP_ForEachObject("MonkeBehaviour", [&](void* other) {
            VRPanel_DrawESPBox(GetVec3Field(other, 0x38), 0xFF3399FF);
        });
    }

    // Menu render
    RenderLimitMenu(g_menuOpen, g_menuAlpha, g_openCategory, g_toggles);
}

// ============================================================
//  HOOK INSTALLATION
// ============================================================
static void InstallHooks() {
    struct { uintptr_t off; void* det; void** orig; } tbl[] = {
        { OFFSET_GETMOVESPEED,     (void*)hook_GetMoveSpeed,    (void**)&orig_GetMoveSpeed    },
        { OFFSET_CHECKCOLLISION,   (void*)hook_CheckCollision,  (void**)&orig_CheckCollision  },
        { OFFSET_ADDCOINS,         (void*)hook_AddCoins,        (void**)&orig_AddCoins        },
        { OFFSET_RENDEROBJ_FOG,    (void*)hook_RenderFog,       (void**)&orig_RenderFog       },
        { OFFSET_PLAYERUPDATE,     (void*)hook_PlayerUpdate,    (void**)&orig_PlayerUpdate    },
        { OFFSET_RENDEROBJECT,     (void*)hook_OnRenderObject,  (void**)&orig_OnRenderObject  },
        { OFFSET_RIGIDBODY_FIXED,  (void*)hook_FixedUpdate,     (void**)&orig_FixedUpdate     },
    };
    for (auto& e : tbl) {
        bool ok = HookFunction(g_il2cppBase + e.off, e.det, e.orig);
        LOGI("hook 0x%lX %s", e.off, ok ? "OK" : "FAIL");
    }
    g_ovrGetDown = g_il2cppBase + OFFSET_OVRINPUT_GETDOWN;
}

// ============================================================
//  HOOK THREAD — waits for libil2cpp.so then installs
// ============================================================
static void* HookThread(void*) {
    LOGI("hook thread running");
    while (true) {
        g_il2cppBase = GetModuleBase("libil2cpp.so");
        if (g_il2cppBase) break;
        usleep(100000);
    }
    LOGI("il2cpp base = 0x%lX", g_il2cppBase);
    usleep(500000);
    InstallHooks();
    VRPanel_Init();
    return nullptr;
}

// ============================================================
//  CONSTRUCTOR — no smali, no JNI_OnLoad needed.
//  The dynamic linker calls this automatically when it maps
//  libNullClient.so into the process at app startup.
// ============================================================
__attribute__((constructor))
static void NullClientInit() {
    LOGI("NullClient loaded — constructor fired");
    pthread_t tid;
    pthread_create(&tid, nullptr, HookThread, nullptr);
    pthread_detach(tid);
}
