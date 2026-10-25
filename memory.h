// ============================================================
//  memory.h — typed field read/write + pointer chase
// ============================================================
#pragma once
#include <cstdint>
#include <cstring>

template<typename T>
static inline T ReadField(void* obj,uint32_t off){
    T v; memcpy(&v,reinterpret_cast<char*>(obj)+off,sizeof(T)); return v;
}
template<typename T>
static inline void WriteField(void* obj,uint32_t off,T val){
    memcpy(reinterpret_cast<char*>(obj)+off,&val,sizeof(T));
}
static inline uintptr_t Chase(uintptr_t base,uint32_t off){
    if(!base)return 0;
    uintptr_t v; memcpy(&v,reinterpret_cast<void*>(base+off),sizeof(v)); return v;
}
