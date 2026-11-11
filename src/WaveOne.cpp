#include "WaveOne.h"
#include <algorithm>
#include <cmath>
#include <limits>

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
void Synth::setFilterCutoff(float x) noexcept { if (std::isfinite(x)) cutoff_ = std::max(30.0f, std::min(x, 18000.0f)); }
void Synth::setFilterResonance(float x) noexcept { if (std::isfinite(x)) resonance_ = clamp01(x); }
void Synth::setLfoRate(float x) noexcept { if (std::isfinite(x)) lfoRate_ = std::max(0.01f, std::min(x, 20.0f)); }
void Synth::setLfoDepth(float x) noexcept { if (std::isfinite(x)) lfoDepth_ = std::max(0.0f, std::min(x, 12.0f)); }
void Synth::setOscWave(int osc, float x) noexcept { if (osc >= 0 && osc < 3 && std::isfinite(x)) oscWave_[osc] = std::max(0, std::min(static_cast<int>(std::lround(x)), 6)); }
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
void Synth::setMatrixDestination(int slot, float x) noexcept { if(slot>=0 && slot<16 && std::isfinite(x)) matrixDestination_[slot]=std::max(0,std::min(3,static_cast<int>(std::lround(x)))); }
void Synth::setMatrixAmount(int slot, float x) noexcept { if(slot>=0 && slot<16 && std::isfinite(x)) matrixAmount_[slot]=std::max(-1.0f,std::min(x,1.0f)); }
void Synth::setEngineType(int osc, float x) noexcept { if (osc >= 0 && osc < 3 && std::isfinite(x)) engineType_[osc] = std::max(0,std::min(3,static_cast<int>(std::lround(x)))); }
void Synth::setVaPulseWidth(int osc, float x) noexcept { if (osc >= 0 && osc < 3 && std::isfinite(x)) vaPulseWidth_[osc] = std::max(0.05f,std::min(x,0.95f)); }
void Synth::setVaDetune(int osc, float x) noexcept { if (osc >= 0 && osc < 3 && std::isfinite(x)) vaDetune_[osc] = std::max(0.0f,std::min(x,1.0f)); }
void Synth::setFmRatio(int osc, float x) noexcept { if (osc >= 0 && osc < 3 && std::isfinite(x)) fmRatio_[osc] = std::max(0.25f,std::min(x,16.0f)); }
void Synth::setFmIndex(int osc, float x) noexcept { if (osc >= 0 && osc < 3 && std::isfinite(x)) fmIndex_[osc] = std::max(0.0f,std::min(x,12.0f)); }
void Synth::setGranularMode(int osc, float x) noexcept { if (osc >= 0 && osc < 3 && std::isfinite(x)) granularMode_[osc] = x >= 0.5f ? 1.0f : 0.0f; }
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
            const float sources[7] = {0.0f,lfoValue(0,lfoPhase_),lfo2,lfo3,lfo4,v.velocity*2.0f-1.0f,(v.note-60.0f)/60.0f};
            for (int m=0;m<16;++m) {
                const int src=matrixSource_[m],dst=matrixDestination_[m];
                const float mod=sources[src]*matrixAmount_[m];
                if(dst==1) matrixCutoff+=mod;
                else if(dst==2) matrixPitch+=mod;
                else if(dst==3) matrixLevel+=mod;
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
                    const double a = v.vaPhase[osc];
                    const double b = v.phase[osc];
                    const float saw = static_cast<float>(2.0*a-1.0);
                    const float pulse = b < vaPulseWidth_[osc] ? 1.0f : -1.0f;
                    voiceSample += (0.55f*saw+0.45f*pulse)*oscLevel_[osc];
                    v.vaPhase[osc] += v.phaseStep*vibrato*std::pow(2.0,vaDetune_[osc]/12.0);
                    if (v.vaPhase[osc] >= 1.0) v.vaPhase[osc] -= std::floor(v.vaPhase[osc]);
                } else if (engineType_[osc] == 3) {
                    // Two-operator FM prototype; full DX7 six-operator engine is pending.
                    const double mod = std::sin(2.0*pi*v.fmPhase[osc]);
                    voiceSample += static_cast<float>(std::sin(2.0*pi*v.phase[osc]+fmIndex_[osc]*mod))*oscLevel_[osc];
                    v.fmPhase[osc] += v.phaseStep*vibrato*fmRatio_[osc];
                    if (v.fmPhase[osc] >= 1.0) v.fmPhase[osc] -= std::floor(v.fmPhase[osc]);
                } else if (engineType_[osc] == 1 || granularMode_[osc] >= 0.5f) {
                    v.grainClock += grainDensity_ / sampleRate_;
                    if (v.grainClock >= 1.0) {
                        v.grainClock -= 1.0;
                        for (auto& g : v.grains) if (!g.active) {
                            v.rng = v.rng*1664525u + 1013904223u;
                            const float rnd = static_cast<float>((v.rng >> 8) & 0xffffffu) / 16777216.0f;
                            const double start = std::max(0.0,std::min(1.0,static_cast<double>(grainPosition_ + (rnd-.5f)*grainSpray_)));
                            g.phase = start * kTableSize;
                            g.step = std::pow(2.0, grainPitch_ / 12.0) * ((rnd < grainReverse_) ? -1.0 : 1.0);
                            g.age = 0; g.length = std::max(8,static_cast<int>(grainSize_ * sampleRate_ / 1000.0)); g.active = true;
                            break;
                        }
                    }
                    float grains = 0.0f; int active = 0;
                    for (auto& g : v.grains) if (g.active) {
                        const int idx = (static_cast<int>(std::floor(g.phase)) % kTableSize + kTableSize) % kTableSize;
                        const float window = 0.5f - 0.5f*std::cos(static_cast<float>(2*pi*g.age / g.length));
                        grains += grainSource_[idx] * window;
                        ++active; g.phase += g.step; if (++g.age >= g.length) g.active = false;
                    }
                    const float dry = sampleWave(v.phase[osc], oscPosition_[osc], oscWave_[osc]);
                    voiceSample += (dry*(1.0f-grainMix_) + (active ? grains / std::sqrt(static_cast<float>(active)) : 0.0f)*grainMix_) * oscLevel_[osc];
                } else voiceSample += sampleWave(v.phase[osc], oscPosition_[osc], oscWave_[osc]) * oscLevel_[osc];
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
            const float alpha = static_cast<float>(1.0 - std::exp(-2.0 * pi * effectiveCutoff / sampleRate_));
            v.filterState += alpha * (input - v.filterState);
            mixed += v.filterState * v.env * v.velocity * std::max(0.0f,1.0f+matrixLevel);
        }
        const float output = std::max(-1.0f, std::min(mixed * gain_, 1.0f));
        left[i] = output;
        right[i] = output;
    }
}
}
