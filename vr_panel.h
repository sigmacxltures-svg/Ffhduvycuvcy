// ============================================================
//  vr_panel.h
//  Blue VR overlay panel — OpenGL ES 3.2, Quest native
//
//  Geometry (panel-local metres, origin = L controller grip):
//    width 0.45  height 0.60  anchored at local Z = -0.001
//
//  Colour palette:
//    bg panel   #0D1B2A   header bar  #1E88E5
//    accent     #1565C0   toggle ON   #42A5F5
//    toggle OFF #37474F   text        #E3F2FD
//    cat label  #90CAF9   row hover   #0A2744
//
//  Input:
//    B on left controller  → open/close  (polled in hook_PlayerUpdate)
//    Right controller ray  → hitzone test (index trigger fires toggle)
// ============================================================
#pragma once

#include <cstdint>
#include <cmath>
#include <dlfcn.h>

enum TextAlign { ALIGN_LEFT, ALIGN_CENTER, ALIGN_RIGHT };

struct HitZone { int id; float x,y,w,h; };
static constexpr int MAX_HZ = 32;
static HitZone  g_hz[MAX_HZ];
static int      g_hzCount = 0;

// OpenGL ES fn ptrs (resolved in VRPanel_Init)
using FnViewport   = void(*)(int,int,int,int);
using FnEnable     = void(*)(unsigned);
using FnDisable    = void(*)(unsigned);
using FnBlendFunc  = void(*)(unsigned,unsigned);
using FnUniform4f  = void(*)(int,float,float,float,float);
using FnUniformM4  = void(*)(int,int,unsigned char,const float*);
using FnDraw       = void(*)(unsigned,int,int);

static FnEnable    gl_Enable    = nullptr;
static FnDisable   gl_Disable   = nullptr;
static FnBlendFunc gl_BlendFunc = nullptr;
static FnUniform4f gl_Uniform4f = nullptr;
static FnDraw      gl_Draw      = nullptr;

static unsigned g_shader  = 0;
static unsigned g_vbo     = 0;
static int      g_uCol    = -1;
static int      g_uMat    = -1;

// panel dims set each Begin call
static float g_pw = 0, g_ph = 0;

// VP matrix written by hook_OnRenderObject
float g_vpMatrix[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};

static void ColourF(uint32_t argb, float& r, float& g, float& b, float& a) {
    a = ((argb>>24)&0xFF)/255.f;
    r = ((argb>>16)&0xFF)/255.f;
    g = ((argb>> 8)&0xFF)/255.f;
    b = ((argb>> 0)&0xFF)/255.f;
}

static void VRPanel_Init() {
    void* gl = dlopen("libGLESv3.so", RTLD_NOW|RTLD_GLOBAL);
    if (!gl) return;
    gl_Enable    = (FnEnable)   dlsym(gl,"glEnable");
    gl_Disable   = (FnDisable)  dlsym(gl,"glDisable");
    gl_BlendFunc = (FnBlendFunc)dlsym(gl,"glBlendFunc");
    gl_Uniform4f = (FnUniform4f)dlsym(gl,"glUniform4f");
    gl_Draw      = (FnDraw)     dlsym(gl,"glDrawArrays");
    dlclose(gl);
    // glCreateShader / glCompileShader / glLinkProgram for flat shader
    // glGenBuffers / glBindBuffer / glBufferData for unit-quad VBO
    // abbreviated — wire full GLSL compile here for production
}

static void VRPanel_Begin(float w, float h, float) {
    g_pw = w; g_ph = h; g_hzCount = 0;
    if (gl_Disable)   gl_Disable(0x0B71);          // GL_DEPTH_TEST
    if (gl_Enable)    gl_Enable(0x0BE2);            // GL_BLEND
    if (gl_BlendFunc) gl_BlendFunc(0x0302,0x0303);  // SRC_ALPHA / ONE_MINUS_SRC_ALPHA
}
static void VRPanel_End() {
    if (gl_Enable)  gl_Enable(0x0B71);
    if (gl_Disable) gl_Disable(0x0BE2);
}

static void VRPanel_DrawRect(float x,float y,float w,float h,
                              uint32_t col,float alpha) {
    float r,g,b,a; ColourF(col,r,g,b,a); a*=alpha;
    if (gl_Uniform4f) gl_Uniform4f(g_uCol,r,g,b,a);
    // upload transform + gl_Draw(GL_TRIANGLE_STRIP,0,4)
    (void)x;(void)y;(void)w;(void)h;
}
static void VRPanel_DrawRRect(float x,float y,float w,float h,
                               float,uint32_t col,float alpha) {
    VRPanel_DrawRect(x,y,w,h,col,alpha); // simplified: plain rect
}
static void VRPanel_DrawCircle(float cx,float cy,float r,
                                uint32_t col,float alpha) {
    float fr,fg,fb,fa; ColourF(col,fr,fg,fb,fa); fa*=alpha;
    if (gl_Uniform4f) gl_Uniform4f(g_uCol,fr,fg,fb,fa);
    // gl_Draw(GL_TRIANGLE_FAN, circleOffset, 26)
    (void)cx;(void)cy;(void)r;
}
static void VRPanel_DrawText(const char*,float,float,int,
                              uint32_t,float,TextAlign) {
    // SDF glyph atlas draw — wire atlas texture + glyph quads per char
}
static void VRPanel_RegHz(int id,float x,float y,float w,float h) {
    if (g_hzCount<MAX_HZ) g_hz[g_hzCount++]={id,x,y,w,h};
}
static int VRPanel_PollPointerHit() {
    // get right controller pointer ray from OVRPlugin::GetNodePose
    // intersect with panel world plane → panel-local UV
    // test each HitZone AABB, check index trigger > 0.7
    return -1; // wire OVR pose API for production
}
static void VRPanel_DrawESPBox(float* wp, uint32_t col) {
    float x=g_vpMatrix[0]*wp[0]+g_vpMatrix[4]*wp[1]+g_vpMatrix[8]*wp[2]+g_vpMatrix[12];
    float y=g_vpMatrix[1]*wp[0]+g_vpMatrix[5]*wp[1]+g_vpMatrix[9]*wp[2]+g_vpMatrix[13];
    float w=g_vpMatrix[3]*wp[0]+g_vpMatrix[7]*wp[1]+g_vpMatrix[11]*wp[2]+g_vpMatrix[15];
    if (w<=0.f) return;
    float sx=x/w*.5f+.5f, sy=y/w*.5f+.5f;
    VRPanel_DrawRect(sx-.025f,sy-.05f,.05f,.1f,col,.85f);
}

// ── main menu render (called from hook_OnRenderObject) ────────
static void RenderLimitMenu(bool open, float& alpha, int& openCat, bool* tog) {
    if (open)  { if (alpha<1.f) alpha+=.08f; }
    else       { if (alpha>0.f) alpha-=.08f; if (alpha<=0.f) return; }

    VRPanel_Begin(.45f,.60f,alpha);

    // bg panel — deep navy
    VRPanel_DrawRect(0.f,0.f,.45f,.60f,0xEB0D1B2A,alpha);
    // header bar — blue
    VRPanel_DrawRect(0.f,.54f,.45f,.06f,0xFF1E88E5,alpha);
    VRPanel_DrawText("Limit Menu",.225f,.565f,18,0xFFE3F2FD,alpha,ALIGN_CENTER);
    // rule
    VRPanel_DrawRect(0.f,.538f,.45f,.002f,0xFF1565C0,alpha);

    struct Cat { const char* label; int start,count; const char* items[3]; };
    static const Cat cats[] = {
        {"Movement",0,3,{"Speed Boost","No Clip","Fly Mode"}},
        {"Economy", 3,3,{"Infinite Coins","Auto Steal","x10 Coins"}},
        {"Visual",  6,2,{"ESP","No Fog",nullptr}},
    };

    float y = .50f;
    for (int ci=0;ci<3;ci++) {
        const Cat& c = cats[ci];
        bool exp = (openCat==ci);
        VRPanel_DrawRect(.005f,y-.018f,.44f,.036f,
                          exp?0xFF1565C0:0xFF0D2137, alpha);
        VRPanel_DrawText(exp?"▼":"▶",.018f,y,11,0xFF90CAF9,alpha,ALIGN_LEFT);
        VRPanel_DrawText(c.label,    .038f,y,13,0xFFE3F2FD,alpha,ALIGN_LEFT);
        VRPanel_RegHz(ci,.005f,y-.018f,.44f,.036f);
        y -= .040f;
        if (!exp) continue;

        for (int fi=0;fi<c.count;fi++) {
            int ti=c.start+fi; bool on=tog[ti];
            VRPanel_DrawRect(.015f,y-.015f,.42f,.030f,
                              on?0xFF0A2744:0xFF071520,alpha);
            // toggle pill
            VRPanel_DrawRRect(.360f,y-.010f,.060f,.020f,.010f,
                               on?0xFF42A5F5:0xFF37474F,alpha);
            // pill knob
            VRPanel_DrawCircle(on?.406f:.368f,y,.008f,0xFFE3F2FD,alpha);
            VRPanel_DrawText(c.items[fi],.028f,y,11,
                              on?0xFF42A5F5:0xFFB0BEC5,alpha,ALIGN_LEFT);
            VRPanel_RegHz(100+ti,.015f,y-.015f,.42f,.030f);
            y -= .034f;
        }
        VRPanel_DrawRect(.010f,y,.43f,.001f,0xFF1565C0,alpha);
        y -= .008f;
    }

    // poll pointer hit and apply
    int hit = VRPanel_PollPointerHit();
    if (hit>=0) {
        if (hit<100) openCat=(openCat==hit)?-1:hit;
        else { int ti=hit-100; if(ti<8) tog[ti]=!tog[ti]; }
    }

    VRPanel_End();
}
