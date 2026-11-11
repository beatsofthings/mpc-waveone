#include "WaveOne.h"
#include <cstdint>
// Open-MPC engine.h declares mpc_engine with C++ linkage; include it inside a C linkage block.
extern "C" {
#include "engine.h"
}
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
    float osc2_position = 0.0f, osc2_level = 0.0f, osc2_tune = 0.0f;
    float osc3_position = 0.0f, osc3_level = 0.0f, osc3_tune = 0.0f;
    float osc1_on = 1.0f, osc2_on = 0.0f, osc3_on = 0.0f;
    float osc1_wave = 0.0f, osc2_wave = 0.0f, osc3_wave = 0.0f;
    float sub_level = 0.0f, cutoff = 18000.0f, resonance = 0.0f;
    float lfo1_rate = 2.0f, lfo1_depth = 0.0f;
    float lfo2_rate = 0.5f, lfo2_depth = 0.0f, lfo2_target = 0.0f;
    float lfo3_rate = 0.25f, lfo3_depth = 0.0f, lfo4_rate = 1.0f, lfo4_depth = 0.0f;
    float lfo4_steps[16] = {0,.38f,.7f,.92f,1,.92f,.7f,.38f,0,-.38f,-.7f,-.92f,-1,-.92f,-.7f,-.38f};
    float filter_env_amount = 0.0f, filter_env_attack = 0.01f, filter_env_decay = 0.2f;
    float filter_env_sustain = 0.0f, filter_env_release = 0.2f;
    float osc1_mode = 0.0f, osc2_mode = 0.0f, osc3_mode = 0.0f;
    float engine_type[3] = {0,0,0};
    float matrix_source[16] = {}, matrix_dest[16] = {}, matrix_amount[16] = {};
    float va_pw[3] = {0.5f,0.5f,0.5f};
    float va_detune[3] = {0.08f,0.08f,0.08f};
    float fm_ratio[3] = {2,2,2};
    float fm_index[3] = {1,1,1};
    float grain_size = 100.0f, grain_density = 12.0f, grain_position = 0.5f;
    float grain_spray = 0.0f, grain_pitch = 0.0f, grain_reverse = 0.0f, grain_mix = 1.0f;
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
    if (!std::strcmp(key,"osc2_position")) return &i.osc2_position;
    if (!std::strcmp(key,"osc2_level")) return &i.osc2_level;
    if (!std::strcmp(key,"osc2_tune")) return &i.osc2_tune;
    if (!std::strcmp(key,"osc3_position")) return &i.osc3_position;
    if (!std::strcmp(key,"osc3_level")) return &i.osc3_level;
    if (!std::strcmp(key,"osc3_tune")) return &i.osc3_tune;
    if (!std::strcmp(key,"osc1_on")) return &i.osc1_on;
    if (!std::strcmp(key,"osc1_wave")) return &i.osc1_wave;
    if (!std::strcmp(key,"osc2_wave")) return &i.osc2_wave;
    if (!std::strcmp(key,"osc3_wave")) return &i.osc3_wave;
    if (!std::strcmp(key,"osc2_on")) return &i.osc2_on;
    if (!std::strcmp(key,"osc3_on")) return &i.osc3_on;
    if (!std::strcmp(key,"sub_level")) return &i.sub_level;
    if (!std::strcmp(key,"cutoff")) return &i.cutoff;
    if (!std::strcmp(key,"resonance")) return &i.resonance;
    if (!std::strcmp(key,"lfo1_rate")) return &i.lfo1_rate;
    if (!std::strcmp(key,"lfo1_depth")) return &i.lfo1_depth;
    for (int n=0;n<16;++n) { char name[24]; std::snprintf(name,sizeof(name),"lfo4_step_%02d",n+1); if (!std::strcmp(key,name)) return &i.lfo4_steps[n]; }
    if (!std::strcmp(key,"lfo3_rate")) return &i.lfo3_rate;
    if (!std::strcmp(key,"lfo3_depth")) return &i.lfo3_depth;
    if (!std::strcmp(key,"lfo4_rate")) return &i.lfo4_rate;
    if (!std::strcmp(key,"lfo4_depth")) return &i.lfo4_depth;
    if (!std::strcmp(key,"lfo2_rate")) return &i.lfo2_rate;
    if (!std::strcmp(key,"lfo2_depth")) return &i.lfo2_depth;
    if (!std::strcmp(key,"lfo2_target")) return &i.lfo2_target;
    if (!std::strcmp(key,"filter_env_amount")) return &i.filter_env_amount;
    if (!std::strcmp(key,"filter_env_attack")) return &i.filter_env_attack;
    if (!std::strcmp(key,"filter_env_decay")) return &i.filter_env_decay;
    if (!std::strcmp(key,"filter_env_sustain")) return &i.filter_env_sustain;
    if (!std::strcmp(key,"filter_env_release")) return &i.filter_env_release;
    for (int n=0;n<16;++n) { char name[32];
        std::snprintf(name,sizeof(name),"matrix_%02d_source",n+1); if (!std::strcmp(key,name)) return &i.matrix_source[n];
        std::snprintf(name,sizeof(name),"matrix_%02d_dest",n+1); if (!std::strcmp(key,name)) return &i.matrix_dest[n];
        std::snprintf(name,sizeof(name),"matrix_%02d_amount",n+1); if (!std::strcmp(key,name)) return &i.matrix_amount[n];
    }
    for (int n=0;n<3;++n) { char name[32];
        std::snprintf(name,sizeof(name),"osc%d_engine",n+1); if (!std::strcmp(key,name)) return &i.engine_type[n];
        std::snprintf(name,sizeof(name),"osc%d_va_pw",n+1); if (!std::strcmp(key,name)) return &i.va_pw[n];
        std::snprintf(name,sizeof(name),"osc%d_va_detune",n+1); if (!std::strcmp(key,name)) return &i.va_detune[n];
        std::snprintf(name,sizeof(name),"osc%d_fm_ratio",n+1); if (!std::strcmp(key,name)) return &i.fm_ratio[n];
        std::snprintf(name,sizeof(name),"osc%d_fm_index",n+1); if (!std::strcmp(key,name)) return &i.fm_index[n];
    }
    if (!std::strcmp(key,"osc1_mode")) return &i.osc1_mode;
    if (!std::strcmp(key,"osc2_mode")) return &i.osc2_mode;
    if (!std::strcmp(key,"osc3_mode")) return &i.osc3_mode;
    if (!std::strcmp(key,"grain_size")) return &i.grain_size;
    if (!std::strcmp(key,"grain_density")) return &i.grain_density;
    if (!std::strcmp(key,"grain_position")) return &i.grain_position;
    if (!std::strcmp(key,"grain_spray")) return &i.grain_spray;
    if (!std::strcmp(key,"grain_pitch")) return &i.grain_pitch;
    if (!std::strcmp(key,"grain_reverse")) return &i.grain_reverse;
    if (!std::strcmp(key,"grain_mix")) return &i.grain_mix;
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
    i.synth.setOscPosition(1,i.osc2_position);
    i.synth.setOscLevel(1,i.osc2_level);
    i.synth.setOscTune(1,i.osc2_tune);
    i.synth.setOscPosition(2,i.osc3_position);
    i.synth.setOscLevel(2,i.osc3_level);
    i.synth.setOscTune(2,i.osc3_tune);
    i.synth.setOscEnabled(0,i.osc1_on);
    i.synth.setOscEnabled(1,i.osc2_on);
    i.synth.setOscEnabled(2,i.osc3_on);
    i.synth.setOscWave(0,i.osc1_wave);
    i.synth.setOscWave(1,i.osc2_wave);
    i.synth.setOscWave(2,i.osc3_wave);
    i.synth.setSubLevel(i.sub_level);
    i.synth.setFilterCutoff(i.cutoff);
    i.synth.setFilterResonance(i.resonance);
    i.synth.setLfoRate(i.lfo1_rate);
    i.synth.setLfoDepth(i.lfo1_depth);
    i.synth.setLfo3Rate(i.lfo3_rate);
    i.synth.setLfo3Depth(i.lfo3_depth);
    i.synth.setLfo4Rate(i.lfo4_rate);
    i.synth.setLfo4Depth(i.lfo4_depth);
    for (int n=0;n<16;++n) i.synth.setLfo4Step(n,i.lfo4_steps[n]);
    i.synth.setLfo2Rate(i.lfo2_rate);
    i.synth.setLfo2Depth(i.lfo2_depth);
    i.synth.setLfo2Target(i.lfo2_target);
    i.synth.setFilterEnvAmount(i.filter_env_amount);
    i.synth.setFilterEnvAttack(i.filter_env_attack);
    i.synth.setFilterEnvDecay(i.filter_env_decay);
    i.synth.setFilterEnvSustain(i.filter_env_sustain);
    i.synth.setFilterEnvRelease(i.filter_env_release);
    for (int n=0;n<16;++n) {
        i.synth.setMatrixSource(n,i.matrix_source[n]);
        i.synth.setMatrixDestination(n,i.matrix_dest[n]);
        i.synth.setMatrixAmount(n,i.matrix_amount[n]);
    }
    for (int n=0;n<3;++n) {
        i.synth.setEngineType(n,i.engine_type[n]);
        i.synth.setVaPulseWidth(n,i.va_pw[n]);
        i.synth.setVaDetune(n,i.va_detune[n]);
        i.synth.setFmRatio(n,i.fm_ratio[n]);
        i.synth.setFmIndex(n,i.fm_index[n]);
    }
    i.synth.setGranularMode(0,i.osc1_mode);
    i.synth.setGranularMode(1,i.osc2_mode);
    i.synth.setGranularMode(2,i.osc3_mode);
    i.synth.setGrainSize(i.grain_size);
    i.synth.setGrainDensity(i.grain_density);
    i.synth.setGrainPosition(i.grain_position);
    i.synth.setGrainSpray(i.grain_spray);
    i.synth.setGrainPitch(i.grain_pitch);
    i.synth.setGrainReverse(i.grain_reverse);
    i.synth.setGrainMix(i.grain_mix);
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
            out[2*(offset+j)] = static_cast<int16_t>(std::lrint(std::max(-1.0f, std::min(l[j], 1.0f))*32767.0f));
            out[2*(offset+j)+1] = static_cast<int16_t>(std::lrint(std::max(-1.0f, std::min(r[j], 1.0f))*32767.0f));
        }
    }
}
const mpc_engine_t engine = {create,destroy,midi,set_param,get_param,render,nullptr,nullptr};
}
extern "C" const mpc_engine_t* mpc_engine(void) { return &engine; }
