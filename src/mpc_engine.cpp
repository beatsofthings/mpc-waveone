#include "WaveOne.h"
#include "engine.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <cstdint>

namespace {
struct Instance {
    waveone::Synth synth;
    float position = 0.0f, attack = 0.01f, decay = 0.12f;
    float sustain = 0.7f, release = 0.25f, gain = 0.2f;
    Instance() { synth.prepare(44100.0); }
};
void* create(const char*) { return new (std::nothrow) Instance(); }
void destroy(void* p) { delete static_cast<Instance*>(p); }
void midi(void* p, const uint8_t* msg, int len) {
    if (!p || !msg || len < 1) return;
    auto& s = static_cast<Instance*>(p)->synth;
    const int type = msg[0] & 0xf0;
    if (type == 0x90 && len >= 3) {
        if (msg[2]) s.noteOn(msg[1] & 127, (msg[2] & 127) / 127.0f);
        else s.noteOff(msg[1] & 127);
    } else if (type == 0x80 && len >= 2) {
        s.noteOff(msg[1] & 127);
    } else if (type == 0xb0 && len >= 3 && (msg[1] == 120 || msg[1] == 123)) {
        s.allNotesOff();
    }
}
float* param(Instance& i, const char* key) {
    if (!std::strcmp(key,"position")) return &i.position;
    if (!std::strcmp(key,"attack")) return &i.attack;
    if (!std::strcmp(key,"decay")) return &i.decay;
    if (!std::strcmp(key,"sustain")) return &i.sustain;
    if (!std::strcmp(key,"release")) return &i.release;
    if (!std::strcmp(key,"gain")) return &i.gain;
    return nullptr;
}
void set_param(void* p, const char* key, const char* val) {
    if (!p || !key || !val) return;
    auto& i = *static_cast<Instance*>(p);
    float* dst = param(i,key);
    if (!dst) return;
    char* end = nullptr;
    const float value = std::strtof(val,&end);
    if (end == val || !std::isfinite(value)) return;
    *dst = value;
    i.synth.setPosition(i.position);
    i.synth.setAttack(i.attack);
    i.synth.setDecay(i.decay);
    i.synth.setSustain(i.sustain);
    i.synth.setRelease(i.release);
    i.synth.setGain(i.gain);
}
int get_param(void* p, const char* key, char* buf, int len) {
    if (!p || !key || !buf || len < 2) return 0;
    auto& i = *static_cast<Instance*>(p);
    float* value = param(i,key);
    if (!value) return 0;
    const int n = std::snprintf(buf,static_cast<size_t>(len),"%.6g",*value);
    return n > 0 && n < len ? n : 0;
}
void render(void* p, int16_t* out, int frames) {
    if (!out || frames <= 0) return;
    if (!p) { std::memset(out,0,static_cast<size_t>(frames)*2*sizeof(int16_t)); return; }
    auto& s = static_cast<Instance*>(p)->synth;
    // MPC normally requests 128 frames; handle any block size without heap allocation.
    float l[128], r[128];
    for (int offset = 0; offset < frames; offset += 128) {
        const int n = std::min(128, frames - offset);
        s.render(l,r,static_cast<size_t>(n));
        for (int j = 0; j < n; ++j) {
            out[2*(offset+j)] = static_cast<int16_t>(std::lrint(std::clamp(l[j],-1.0f,1.0f)*32767.0f));
            out[2*(offset+j)+1] = static_cast<int16_t>(std::lrint(std::clamp(r[j],-1.0f,1.0f)*32767.0f));
        }
    }
}
const mpc_engine_t engine = {create,destroy,midi,set_param,get_param,render,nullptr,nullptr};
}
extern "C" const mpc_engine_t* mpc_engine(void) { return &engine; }
