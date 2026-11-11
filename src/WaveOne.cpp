#include "WaveOne.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <cstdio>
#include <vector>
#include <cstring>


// Minimal bounded PCM WAV reader: RIFF/WAVE, mono/stereo, PCM16, PCM24 or IEEE float32.
// Decode only during initialization, never from render().
namespace {
unsigned rd16(const unsigned char* p) { return unsigned(p[0])|(unsigned(p[1])<<8); }
unsigned rd32(const unsigned char* p) { return rd16(p)|(rd16(p+2)<<16); }
bool readWav(const char* path, std::array<float, waveone::Synth::kTableSize>& output) {
    if (!path) return false;
    FILE* f=std::fopen(path,"rb"); if(!f) return false;
    unsigned char header[12];
    if(std::fread(header,1,12,f)!=12 || std::memcmp(header,"RIFF",4)!=0 || std::memcmp(header+8,"WAVE",4)!=0) {std::fclose(f);return false;}
    unsigned fmt=0,ch=0,bits=0; std::vector<unsigned char> bytes;
    for(int chunk=0;chunk<128;++chunk) {
        unsigned char h[8]; if(std::fread(h,1,8,f)!=8) break;
        const unsigned size=rd32(h+4);
        if(size>16*1024*1024) break;
        if(std::memcmp(h,"fmt ",4)==0 && size>=16 && size<=256) {
            unsigned char b[256]; if(std::fread(b,1,size,f)!=size) break;
            fmt=rd16(b); ch=rd16(b+2); bits=rd16(b+14);
        } else if(std::memcmp(h,"data",4)==0 && size>0) {
            bytes.resize(size); if(std::fread(bytes.data(),1,size,f)!=size) bytes.clear();
        } else if(std::fseek(f,static_cast<long>(size),SEEK_CUR)!=0) break;
        if(size&1u) std::fseek(f,1,SEEK_CUR);
        if(!bytes.empty() && fmt) break;
    }
    std::fclose(f);
    const unsigned bps=bits/8;
    if(!((fmt==1 && (bits==16||bits==24))||(fmt==3&&bits==32)) || (ch!=1&&ch!=2) || bps==0) return false;
    const size_t frames=bytes.size()/(bps*ch);
    if(frames<16) return false;
    auto sample=[&](size_t i) -> float {
        const unsigned char* p=bytes.data()+i*bps*ch;
        float v=0;
        if(fmt==3) { std::memcpy(&v,p,4); if(!std::isfinite(v)) v=0; }
        else if(bits==16) { const int raw=static_cast<int>(rd16(p)); v=static_cast<float>(raw>=32768?raw-65536:raw)/32768.0f; }
        else { int raw=int(p[0])|(int(p[1])<<8)|(int(p[2])<<16); if(raw&0x800000) raw-=0x1000000; v=raw/8388608.0f; }
        return std::max(-1.0f,std::min(v,1.0f));
    };
    float peak=0;
    for(int i=0;i<waveone::Synth::kTableSize;++i) {
        const double src=static_cast<double>(i)*(frames-1)/(waveone::Synth::kTableSize-1);
        const size_t a=static_cast<size_t>(src),b=std::min(a+1,frames-1);
        const float frac=static_cast<float>(src-a);
        output[i]=sample(a)*(1-frac)+sample(b)*frac;
        peak=std::max(peak,std::abs(output[i]));
    }
    if(peak>0.0001f) for(auto& v:output) v/=peak;
    return true;
}
}

namespace waveone {
namespace {
constexpr double pi = 3.14159265358979323846;
float clamp01(float value) noexcept { return std::max(0.0f, std::min(value, 1.0f)); }
}
Synth::Synth() {
    // Basic prototype waveforms. Band-limited oscillator is an upcoming improvement.
    for (int i = 0; i < kTableSize; ++i) {
        const double phase = static_cast<double>(i) / kTableSize;
        tables_[0][i] = static_cast<float>(std::sin(2.0 * pi * phase));
        tables_[1][i] = static_cast<float>(2.0 * phase - 1.0);
        tables_[2][i] = phase < 0.5 ? 1.0f : -1.0f;
        tables_[3][i] = static_cast<float>(1.0 - 4.0 * std::abs(phase - 0.5)); // triangle
        tables_[4][i] = phase < 0.25 ? 1.0f : -1.0f; // pulse 25%
        tables_[5][i] = static_cast<float>(0.6 * std::sin(2.0*pi*phase) + 0.3 * std::sin(6.0*pi*phase) + 0.1 * std::sin(10.0*pi*phase)); // organ
        grainSource_[i] = static_cast<float>(0.5*std::sin(2.0*pi*phase*7.0) + 0.25*std::sin(2.0*pi*phase*11.0) + 0.2*std::sin(2.0*pi*phase*19.0));
    }
}
void Synth::prepare(double sampleRate) noexcept {
    if (std::isfinite(sampleRate) && sampleRate >= 8000.0 && sampleRate <= 384000.0) {
        sampleRate_ = sampleRate;
        for (auto& v : voices_) if (v.stage != Voice::Off)
            v.phaseStep = std::pow(2.0, (v.note - 69.0) / 12.0) * 440.0 / sampleRate_;
    }
}
void Synth::noteOn(int midiNote, float velocity) noexcept {
    if (midiNote < 0 || midiNote > 127 || !std::isfinite(velocity)) return;
    if (velocity <= 0.0f) { noteOff(midiNote); return; }
    Voice* slot = nullptr;
    for (auto& v : voices_) if (v.stage == Voice::Off) { slot = &v; break; }
    if (!slot) slot = &*std::min_element(voices_.begin(), voices_.end(), [](const Voice& a, const Voice& b) { return a.started < b.started; });
    *slot = {};
    slot->note = midiNote;
    slot->phaseStep = 440.0 * std::pow(2.0, (midiNote - 69.0) / 12.0) / sampleRate_;
    slot->velocity = clamp01(velocity);
    slot->stage = Voice::Attack;
    slot->started = ++counter_;
    slot->rng = 0x9e3779b9u ^ static_cast<std::uint32_t>(midiNote*7919 + counter_);
}
void Synth::noteOff(int midiNote) noexcept {
    for (auto& v : voices_) if (v.note == midiNote && v.stage != Voice::Off && v.stage != Voice::Release) {
        v.stage = Voice::Release;
        v.releaseStep = v.env / std::max(1.0f, release_ * static_cast<float>(sampleRate_));
        v.filterReleaseStep = v.filterEnv / std::max(1.0f, filterEnvRelease_ * static_cast<float>(sampleRate_));
    }
}
void Synth::allNotesOff() noexcept { for (auto& v : voices_) v = {}; }
void Synth::setPosition(float x) noexcept { if (std::isfinite(x)) oscPosition_[0] = std::max(0.0f, std::min(x, 2.0f)); }
void Synth::setAttack(float x) noexcept { if (std::isfinite(x)) attack_ = std::max(0.001f, std::min(x, 10.0f)); }
void Synth::setDecay(float x) noexcept { if (std::isfinite(x)) decay_ = std::max(0.001f, std::min(x, 10.0f)); }
void Synth::setSustain(float x) noexcept { if (std::isfinite(x)) sustain_ = clamp01(x); }
void Synth::setRelease(float x) noexcept { if (std::isfinite(x)) release_ = std::max(0.001f, std::min(x, 10.0f)); }
void Synth::setGain(float x) noexcept { if (std::isfinite(x)) gain_ = clamp01(x); }
void Synth::setOscPosition(int osc, float x) noexcept { if (osc >= 0 && osc < 3 && std::isfinite(x)) oscPosition_[osc] = std::max(0.0f, std::min(x, 2.0f)); }
void Synth::setOscLevel(int osc, float x) noexcept { if (osc >= 0 && osc < 3 && std::isfinite(x)) oscLevel_[osc] = clamp01(x); }
void Synth::setOscTune(int osc, float x) noexcept { if (osc >= 0 && osc < 3 && std::isfinite(x)) oscTune_[osc] = std::max(-24.0f, std::min(x, 24.0f)); }
void Synth::setOscEnabled(int osc, float x) noexcept { if (osc >= 0 && osc < 3 && std::isfinite(x)) oscEnabled_[osc] = x >= 0.5f ? 1.0f : 0.0f; }
void Synth::setSubLevel(float x) noexcept { if (std::isfinite(x)) subLevel_ = clamp01(x); }
void Synth::setFilterMode(float x) noexcept { if(std::isfinite(x)) filterMode_=std::max(0,std::min(4,static_cast<int>(std::lround(x)))); }
void Synth::setFilterQ(float x) noexcept { if(std::isfinite(x)) filterQ_=std::max(0.5f,std::min(x,12.0f)); }
void Synth::setFilterCutoff(float x) noexcept { if (std::isfinite(x)) cutoff_ = std::max(30.0f, std::min(x, 18000.0f)); }
void Synth::setFilterResonance(float x) noexcept { if (std::isfinite(x)) resonance_ = clamp01(x); }
void Synth::setLfoRate(float x) noexcept { if (std::isfinite(x)) lfoRate_ = std::max(0.01f, std::min(x, 20.0f)); }
void Synth::setLfoDepth(float x) noexcept { if (std::isfinite(x)) lfoDepth_ = std::max(0.0f, std::min(x, 12.0f)); }
void Synth::setWtWarpMode(int osc, float x) noexcept { if(osc>=0&&osc<3&&std::isfinite(x)) wtWarpMode_[osc]=std::max(0,std::min(5,static_cast<int>(std::lround(x)))); }
void Synth::setWtWarpAmount(int osc, float x) noexcept { if(osc>=0&&osc<3&&std::isfinite(x)) wtWarpAmount_[osc]=clamp01(x); }
void Synth::setVaSync(int osc, float x) noexcept { if(osc>=0&&osc<3&&std::isfinite(x)) vaSync_[osc]=clamp01(x); }
bool Synth::loadUserWave(int osc,const char* path) noexcept { if(osc<0||osc>=3)return false; hasUserWave_[osc]=readWav(path,userWave_[osc]);return hasUserWave_[osc]; }
bool Synth::loadUserGrain(int osc,const char* path) noexcept { if(osc<0||osc>=3)return false; hasUserGrain_[osc]=readWav(path,userGrain_[osc]);return hasUserGrain_[osc]; }
void Synth::setGrainSourceMode(int osc,float x) noexcept { if(osc>=0&&osc<3&&std::isfinite(x)) grainSourceMode_[osc]=x>=0.5f?1:0; }
void Synth::setOscWave(int osc, float x) noexcept { if (osc >= 0 && osc < 3 && std::isfinite(x)) oscWave_[osc] = std::max(0, std::min(static_cast<int>(std::lround(x)), 7)); }
void Synth::setLfoShape(int lfo, float x) noexcept { if (lfo>=0 && lfo<3 && std::isfinite(x)) lfoShape_[lfo] = std::max(0,std::min(9,static_cast<int>(std::lround(x)))); }
float Synth::lfoValue(int lfo, double phase) const noexcept {
    const double p = phase - std::floor(phase);
    switch (lfoShape_[lfo]) {
      case 1: return static_cast<float>(4.0*std::abs(p-0.5)-1.0); // triangle
      case 2: return static_cast<float>(2.0*p-1.0); // ramp up
      case 3: return static_cast<float>(1.0-2.0*p); // ramp down
      case 4: return p<0.5?1.0f:-1.0f; // square
      case 5: return p<0.2?1.0f:-1.0f; // pulse
      case 6: return static_cast<float>(std::sin(2*pi*p)*std::sin(4*pi*p)); // complex
      case 7: return static_cast<float>(std::sin(2*pi*p)+0.3*std::sin(6*pi*p))/1.3f; // harmonic
      case 8: return static_cast<float>(2.0*std::abs(std::sin(2*pi*p))-1.0); // rectified
      case 9: return static_cast<float>(std::sin(2*pi*p)*std::sin(2*pi*p)*std::sin(2*pi*p)); // soft pulse
      default: return static_cast<float>(std::sin(2*pi*p));
    }
}
void Synth::setLfo3Rate(float x) noexcept { if (std::isfinite(x)) lfo3Rate_ = std::max(0.01f,std::min(x,20.0f)); }
void Synth::setLfo3Depth(float x) noexcept { if (std::isfinite(x)) lfo3Depth_ = clamp01(x); }
void Synth::setLfo4Rate(float x) noexcept { if (std::isfinite(x)) lfo4Rate_ = std::max(0.01f,std::min(x,20.0f)); }
void Synth::setLfo4Depth(float x) noexcept { if (std::isfinite(x)) lfo4Depth_ = clamp01(x); }
void Synth::setLfo4Step(int i, float x) noexcept { if (i >= 0 && i < 16 && std::isfinite(x)) lfo4Steps_[i] = std::max(-1.0f,std::min(x,1.0f)); }
void Synth::setLfo2Rate(float x) noexcept { if (std::isfinite(x)) lfo2Rate_ = std::max(0.01f, std::min(x, 20.0f)); }
void Synth::setLfo2Depth(float x) noexcept { if (std::isfinite(x)) lfo2Depth_ = clamp01(x); }
void Synth::setLfo2Target(float x) noexcept { if (std::isfinite(x)) lfo2Target_ = std::max(0, std::min(static_cast<int>(std::lround(x)), 2)); }
void Synth::setFilterEnvAmount(float x) noexcept { if (std::isfinite(x)) filterEnvAmount_ = std::max(-1.0f, std::min(x, 1.0f)); }
void Synth::setFilterEnvAttack(float x) noexcept { if (std::isfinite(x)) filterEnvAttack_ = std::max(0.001f, std::min(x, 10.0f)); }
void Synth::setFilterEnvDecay(float x) noexcept { if (std::isfinite(x)) filterEnvDecay_ = std::max(0.001f, std::min(x, 10.0f)); }
void Synth::setFilterEnvSustain(float x) noexcept { if (std::isfinite(x)) filterEnvSustain_ = clamp01(x); }
void Synth::setFilterEnvRelease(float x) noexcept { if (std::isfinite(x)) filterEnvRelease_ = std::max(0.001f, std::min(x, 10.0f)); }
void Synth::setMatrixSource(int slot, float x) noexcept { if(slot>=0 && slot<16 && std::isfinite(x)) matrixSource_[slot]=std::max(0,std::min(6,static_cast<int>(std::lround(x)))); }
void Synth::setMatrixDestination(int slot, float x) noexcept { if(slot>=0 && slot<16 && std::isfinite(x)) matrixDestination_[slot]=std::max(0,std::min(15,static_cast<int>(std::lround(x)))); }
void Synth::setMatrixAmount(int slot, float x) noexcept { if(slot>=0 && slot<16 && std::isfinite(x)) matrixAmount_[slot]=std::max(-1.0f,std::min(x,1.0f)); }
void Synth::setEngineType(int osc, float x) noexcept { if (osc >= 0 && osc < 3 && std::isfinite(x)) engineType_[osc] = std::max(0,std::min(3,static_cast<int>(std::lround(x)))); }
void Synth::setVaWaveform(int osc, float x) noexcept { if(osc>=0&&osc<3&&std::isfinite(x)) vaWaveform_[osc]=std::max(0,std::min(4,static_cast<int>(std::lround(x)))); }
void Synth::setVaNoise(int osc, float x) noexcept { if(osc>=0&&osc<3&&std::isfinite(x)) vaNoise_[osc]=clamp01(x); }
void Synth::setFmAlgorithm(int osc, float x) noexcept { if(osc>=0&&osc<3&&std::isfinite(x)) fmAlgorithm_[osc]=std::max(0,std::min(3,static_cast<int>(std::lround(x)))); }
void Synth::setFmOperatorRatio(int osc,int op,float x) noexcept { if(osc>=0&&osc<3&&op>=0&&op<6&&std::isfinite(x)) fmOpRatio_[osc][op]=std::max(0.25f,std::min(32.0f,x)); }
void Synth::setFmOperatorLevel(int osc,int op,float x) noexcept { if(osc>=0&&osc<3&&op>=0&&op<6&&std::isfinite(x)) fmOpLevel_[osc][op]=clamp01(x); }
void Synth::setFmFeedback(int osc, float x) noexcept { if(osc>=0&&osc<3&&std::isfinite(x)) fmFeedback_[osc]=clamp01(x); }
void Synth::setGrainWindow(int osc, float x) noexcept { if(osc>=0&&osc<3&&std::isfinite(x)) grainWindow_[osc]=std::max(0,std::min(3,static_cast<int>(std::lround(x)))); }
void Synth::setVaPulseWidth(int osc, float x) noexcept { if (osc >= 0 && osc < 3 && std::isfinite(x)) vaPulseWidth_[osc] = std::max(0.05f,std::min(x,0.95f)); }
void Synth::setVaDetune(int osc, float x) noexcept { if (osc >= 0 && osc < 3 && std::isfinite(x)) vaDetune_[osc] = std::max(0.0f,std::min(x,1.0f)); }
void Synth::setFmRatio(int osc, float x) noexcept { if (osc >= 0 && osc < 3 && std::isfinite(x)) fmRatio_[osc] = std::max(0.25f,std::min(x,16.0f)); }
void Synth::setFmIndex(int osc, float x) noexcept { if (osc >= 0 && osc < 3 && std::isfinite(x)) fmIndex_[osc] = std::max(0.0f,std::min(x,12.0f)); }
void Synth::setGranularMode(int osc, float x) noexcept { if (osc >= 0 && osc < 3 && std::isfinite(x)) granularMode_[osc] = x >= 0.5f ? 1.0f : 0.0f; }
void Synth::setGrainParameter(int osc, int parameter, float x) noexcept {
    if (osc<0 || osc>=3 || parameter<0 || parameter>=7 || !std::isfinite(x)) return;
    const float lo[7]={5,1,0,0,-24,0,0};
    const float hi[7]={500,80,1,1,24,1,1};
    grainParams_[osc][parameter]=std::max(lo[parameter],std::min(hi[parameter],x));
}
void Synth::setGrainSize(float x) noexcept { if (std::isfinite(x)) grainSize_ = std::max(5.0f,std::min(x,500.0f)); }
void Synth::setGrainDensity(float x) noexcept { if (std::isfinite(x)) grainDensity_ = std::max(1.0f,std::min(x,80.0f)); }
void Synth::setGrainPosition(float x) noexcept { if (std::isfinite(x)) grainPosition_ = clamp01(x); }
void Synth::setGrainSpray(float x) noexcept { if (std::isfinite(x)) grainSpray_ = clamp01(x); }
void Synth::setGrainPitch(float x) noexcept { if (std::isfinite(x)) grainPitch_ = std::max(-24.0f,std::min(x,24.0f)); }
void Synth::setGrainReverse(float x) noexcept { if (std::isfinite(x)) grainReverse_ = clamp01(x); }
void Synth::setGrainMix(float x) noexcept { if (std::isfinite(x)) grainMix_ = clamp01(x); }
float Synth::sampleWave(double phase, float position, int wave) const noexcept {
    const double wrapped = phase - std::floor(phase);
    const double idx = wrapped * kTableSize;
    const int a = static_cast<int>(idx) % kTableSize;
    const int b = (a + 1) % kTableSize;
    const float frac = static_cast<float>(idx - std::floor(idx));
    const int first = wave == 0 ? std::min(1, static_cast<int>(position)) : std::min(5, std::max(0, wave - 1));
    const int second = wave == 0 ? std::min(2, first + 1) : first;
    const float blend = wave == 0 ? position - first : 0.0f;
    auto read = [&](int table) { return tables_[table][a] + frac * (tables_[table][b] - tables_[table][a]); };
    return (1.0f - blend) * read(first) + blend * read(second);
}
float Synth::sampleOscWave(int osc, double phase, float position) const noexcept {
    const double p=phase-std::floor(phase);
    const double amt=wtWarpAmount_[osc];
    double warped=p;
    switch(wtWarpMode_[osc]) {
      case 1: warped=std::pow(p,1.0+amt*7.0); break; // bend+
      case 2: warped=1.0-std::pow(1.0-p,1.0+amt*7.0); break; // bend-
      case 3: warped=p+amt*std::sin(2.0*pi*p)/(2.0*pi); break; // phase bend
      case 4: warped=p*(1.0+amt*3.0); break; // sync warp
      case 5: warped=p+amt*0.5; break; // phase shift
      default: break;
    }
    if(oscWave_[osc]==7 && hasUserWave_[osc]) {
        const double idx=(warped-std::floor(warped))*kTableSize;
        const int a=static_cast<int>(idx)%kTableSize,b=(a+1)%kTableSize;
        const float t=static_cast<float>(idx-a);
        return userWave_[osc][a]*(1-t)+userWave_[osc][b]*t;
    }
    return sampleWave(warped,position,oscWave_[osc]==7?0:oscWave_[osc]);
}
void Synth::render(float* left, float* right, std::size_t frames) noexcept {
    if (!left || !right) return;
    for (std::size_t i = 0; i < frames; ++i) {
        float mixed = 0.0f;
        const double vibrato = std::pow(2.0, lfoValue(0,lfoPhase_) * lfoDepth_ / 12.0);
        lfoPhase_ += lfoRate_ / sampleRate_;
        if (lfoPhase_ >= 1.0) lfoPhase_ -= std::floor(lfoPhase_);
        const float lfo2 = static_cast<float>(lfoValue(1,lfo2Phase_)) * lfo2Depth_;
        const float lfo3 = static_cast<float>(lfoValue(2,lfo3Phase_)) * lfo3Depth_;
        lfo3Phase_ += lfo3Rate_ / sampleRate_;
        if (lfo3Phase_ >= 1.0) lfo3Phase_ -= std::floor(lfo3Phase_);
        const float curveIndex = static_cast<float>(lfo4Phase_ * 16.0);
        const int curveA = static_cast<int>(curveIndex) % 16;
        const int curveB = (curveA+1)%16;
        const float curveT = curveIndex - static_cast<float>(curveA);
        const float lfo4 = (lfo4Steps_[curveA]*(1.0f-curveT)+lfo4Steps_[curveB]*curveT)*lfo4Depth_;
        lfo4Phase_ += lfo4Rate_ / sampleRate_;
        if (lfo4Phase_ >= 1.0) lfo4Phase_ -= std::floor(lfo4Phase_);
        lfo2Phase_ += lfo2Rate_ / sampleRate_;
        if (lfo2Phase_ >= 1.0) lfo2Phase_ -= std::floor(lfo2Phase_);
        for (auto& v : voices_) {
            if (v.stage == Voice::Off) continue;
            float matrixCutoff=0.0f, matrixPitch=0.0f, matrixLevel=0.0f;
            float matrixWt[3]={0,0,0}, matrixFm[3]={0,0,0}, matrixVa[3]={0,0,0}, matrixGrain[3]={0,0,0};
            const float sources[7] = {0.0f,lfoValue(0,lfoPhase_),lfo2,lfo3,lfo4,v.velocity*2.0f-1.0f,(v.note-60.0f)/60.0f};
            for (int m=0;m<16;++m) {
                const int src=matrixSource_[m],dst=matrixDestination_[m];
                const float mod=sources[src]*matrixAmount_[m];
                if(dst==1) matrixCutoff+=mod;
                else if(dst==2) matrixPitch+=mod;
                else if(dst==3) matrixLevel+=mod;
                else if(dst>=4 && dst<=6) matrixWt[dst-4]+=mod;
                else if(dst>=7 && dst<=9) matrixFm[dst-7]+=mod;
                else if(dst>=10 && dst<=12) matrixVa[dst-10]+=mod;
                else if(dst>=13 && dst<=15) matrixGrain[dst-13]+=mod;
            }
            matrixCutoff=std::max(-2.0f,std::min(matrixCutoff,2.0f));
            matrixPitch=std::max(-2.0f,std::min(matrixPitch,2.0f));
            matrixLevel=std::max(-1.0f,std::min(matrixLevel,1.0f));
            switch (v.stage) {
                case Voice::Attack:
                    v.env += 1.0f / std::max(1.0f, attack_ * static_cast<float>(sampleRate_));
                    if (v.env >= 1.0f) { v.env = 1.0f; v.stage = Voice::Decay; }
                    break;
                case Voice::Decay:
                    v.env -= (1.0f - sustain_) / std::max(1.0f, decay_ * static_cast<float>(sampleRate_));
                    if (v.env <= sustain_) { v.env = sustain_; v.stage = Voice::Sustain; }
                    break;
                case Voice::Sustain: v.env = sustain_; break;
                case Voice::Release:
                    v.env -= v.releaseStep;
                    if (v.env <= 0.0f) { v = {}; continue; }
                    break;
                case Voice::Off: break;
            }
            float voiceSample = 0.0f;
            for (int osc = 0; osc < 3; ++osc) {
                if (oscLevel_[osc] <= 0.0f || oscEnabled_[osc] < 0.5f) continue;
                if (engineType_[osc] == 2) {
                    // Two detuned virtual-analog oscillators: saw + variable pulse.
                    const double a = vaSync_[osc]>=0.5f ? std::fmod(v.vaPhase[osc],1.0) : v.vaPhase[osc];
                    const double b = v.phase[osc];
                    const float saw = static_cast<float>(2.0*a-1.0);
                    const float pulse = b < std::max(0.05f,std::min(0.95f,vaPulseWidth_[osc]+matrixVa[osc]*0.45f)) ? 1.0f : -1.0f;
                    float va = 0.55f*saw+0.45f*pulse;
                    if(vaWaveform_[osc]==1) va=saw;
                    else if(vaWaveform_[osc]==2) va=pulse;
                    else if(vaWaveform_[osc]==3) va=static_cast<float>(1.0-4.0*std::abs(a-0.5));
                    else if(vaWaveform_[osc]==4) va=static_cast<float>(std::sin(2*pi*a));
                    v.rng=v.rng*1664525u+1013904223u;
                    const float noise=static_cast<float>((v.rng>>8)&0xffffffu)/8388608.0f-1.0f;
                    voiceSample += (va*(1.0f-vaNoise_[osc])+noise*vaNoise_[osc])*oscLevel_[osc];
                    v.vaPhase[osc] += v.phaseStep*vibrato*std::pow(2.0,vaDetune_[osc]/12.0);
                    if(vaSync_[osc]>=0.5f && v.phase[osc]+v.phaseStep*vibrato>=1.0) v.vaPhase[osc]=0.0;
                    if (v.vaPhase[osc] >= 1.0) v.vaPhase[osc] -= std::floor(v.vaPhase[osc]);
                } else if (engineType_[osc] == 3) {
                    // Six operator FM with four independent routing topologies.
                    float op[6]={0,0,0,0,0,0};
                    const float index=std::max(0.0f,fmIndex_[osc]+matrixFm[osc]*8.0f);
                    for(int k=5;k>=0;--k) {
                        float modulation=0.0f;
                        if(fmAlgorithm_[osc]==0 && k<5) modulation=op[k+1]; // serial
                        else if(fmAlgorithm_[osc]==1 && k==0) modulation=op[1]+op[2]+op[3]+op[4]+op[5]; // five modulators
                        else if(fmAlgorithm_[osc]==2 && (k==0||k==3)) modulation=op[k+1]+op[k+2]; // two groups
                        else if(fmAlgorithm_[osc]==3 && k==0) modulation=op[1]; // two carriers plus additive ops
                        const float feedback=(k==5)?fmFeedback_[osc]*v.fmLast[osc]*6.0f:0.0f;
                        op[k]=fmOpLevel_[osc][k]*static_cast<float>(std::sin(2.0*pi*v.fmOpPhase[osc][k]+index*modulation+feedback));
                        v.fmOpPhase[osc][k]+=v.phaseStep*vibrato*fmOpRatio_[osc][k]*(k==1?fmRatio_[osc]:1.0f);
                        if(v.fmOpPhase[osc][k]>=1.0) v.fmOpPhase[osc][k]-=std::floor(v.fmOpPhase[osc][k]);
                    }
                    v.fmLast[osc]=op[5];
                    float fmOut=op[0];
                    if(fmAlgorithm_[osc]==2) fmOut=0.5f*(op[0]+op[3]);
                    else if(fmAlgorithm_[osc]==3) fmOut=(op[0]+op[2]+op[3]+op[4]+op[5])*0.2f;
                    voiceSample+=fmOut*oscLevel_[osc];
                } else if (engineType_[osc] == 1 || granularMode_[osc] >= 0.5f) {
                    v.grainClock[osc] += std::max(1.0f,grainParams_[osc][1] * std::pow(2.0f,matrixGrain[osc]*3.0f)) / sampleRate_;
                    if (v.grainClock[osc] >= 1.0) {
                        v.grainClock[osc] -= 1.0;
                        for (auto& g : v.grains[osc]) if (!g.active) {
                            v.rng = v.rng*1664525u + 1013904223u;
                            const float rnd = static_cast<float>((v.rng >> 8) & 0xffffffu) / 16777216.0f;
                            const double start = std::max(0.0,std::min(1.0,static_cast<double>(grainParams_[osc][2] + (rnd-.5f)*grainParams_[osc][3])));
                            g.phase = start * kTableSize;
                            g.step = std::pow(2.0, grainParams_[osc][4] / 12.0) * ((rnd < grainParams_[osc][5]) ? -1.0 : 1.0);
                            g.age = 0; g.length = std::max(8,static_cast<int>(grainParams_[osc][0] * sampleRate_ / 1000.0)); g.active = true;
                            break;
                        }
                    }
                    float grains = 0.0f; int active = 0;
                    for (auto& g : v.grains[osc]) if (g.active) {
                        const int idx = (static_cast<int>(std::floor(g.phase)) % kTableSize + kTableSize) % kTableSize;
                        const float t=static_cast<float>(g.age)/g.length;
                        float window=0.5f-0.5f*std::cos(static_cast<float>(2*pi*t));
                        if(grainWindow_[osc]==1) window=1.0f-std::abs(2.0f*t-1.0f);
                        else if(grainWindow_[osc]==2) window=std::sin(static_cast<float>(pi*t));
                        else if(grainWindow_[osc]==3) window=(t<0.1f?t*10.0f:(t>0.9f?(1.0f-t)*10.0f:1.0f));
                        grains += ((grainSourceMode_[osc] && hasUserGrain_[osc]) ? userGrain_[osc][idx] : grainSource_[idx]) * window;
                        ++active; g.phase += g.step; if (++g.age >= g.length) g.active = false;
                    }
                    const float dry = sampleOscWave(osc,v.phase[osc],std::max(0.0f,std::min(2.0f,oscPosition_[osc]+matrixWt[osc]*2.0f)));
                    voiceSample += (dry*(1.0f-grainParams_[osc][6]) + (active ? grains / std::sqrt(static_cast<float>(active)) : 0.0f)*grainParams_[osc][6]) * oscLevel_[osc];
                } else voiceSample += sampleWave(v.phase[osc], std::max(0.0f,std::min(2.0f,oscPosition_[osc]+matrixWt[osc]*2.0f)), oscWave_[osc]) * oscLevel_[osc];
                v.phase[osc] += v.phaseStep * vibrato * std::pow(2.0, (oscTune_[osc]+matrixPitch*12.0f) / 12.0);
                if (v.phase[osc] >= 1.0) v.phase[osc] -= std::floor(v.phase[osc]);
            }
            v.subPhase += v.phaseStep * 0.5 * vibrato;
            if (v.subPhase >= 1.0) v.subPhase -= std::floor(v.subPhase);
            voiceSample += subLevel_ * std::sin(2.0 * pi * v.subPhase);
            if (lfo2Target_ == 2) voiceSample *= std::max(0.0f, 1.0f + lfo2);
            // Stable one-pole low-pass; resonance controls a mild pre-filter drive.
            const float drive = 1.0f + 2.0f * resonance_;
            const float input = std::tanh(voiceSample * drive) / drive;
            // Filter envelope follows the amp voice lifecycle, with separate ADSR times.
            if (v.stage == Voice::Attack) v.filterEnv = std::min(1.0f, v.filterEnv + 1.0f / std::max(1.0f, filterEnvAttack_ * static_cast<float>(sampleRate_)));
            else if (v.stage == Voice::Decay || v.stage == Voice::Sustain) v.filterEnv += (filterEnvSustain_ - v.filterEnv) / std::max(1.0f, filterEnvDecay_ * static_cast<float>(sampleRate_));
            else if (v.stage == Voice::Release) v.filterEnv = std::max(0.0f, v.filterEnv - v.filterReleaseStep);
            const float mod = filterEnvAmount_ * v.filterEnv + (lfo2Target_ == 1 ? lfo2 : 0.0f) + lfo3 + lfo4 + matrixCutoff;
            const float effectiveCutoff = std::max(30.0f, std::min(18000.0f, cutoff_ * std::pow(2.0f, 5.0f * mod)));
            // Topology-preserving two-integrator state-variable filter (LP/BP/HP/Notch/Peak).
            const float safeHz=std::min(effectiveCutoff,static_cast<float>(sampleRate_*0.45));
            const float g=static_cast<float>(std::tan(pi*safeHz/sampleRate_));
            const float k=1.0f/filterQ_;
            const float a1=1.0f/(1.0f+g*(g+k));
            const float v1=(v.filterIc1+g*(input-v.filterIc2))*a1;
            const float v2=v.filterIc2+g*v1;
            v.filterIc1=2.0f*v1-v.filterIc1;
            v.filterIc2=2.0f*v2-v.filterIc2;
            const float lp=v2, bp=v1, hp=input-k*bp-lp;
            float filtered=lp;
            if(filterMode_==1) filtered=bp;
            else if(filterMode_==2) filtered=hp;
            else if(filterMode_==3) filtered=lp+hp;
            else if(filterMode_==4) filtered=lp-hp;
            mixed += filtered * v.env * v.velocity * std::max(0.0f,1.0f+matrixLevel);
        }
        const float output = std::max(-1.0f, std::min(mixed * gain_, 1.0f));
        left[i] = output;
        right[i] = output;
    }
}
}
