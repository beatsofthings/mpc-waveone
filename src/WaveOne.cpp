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
void Synth::setLfo2Rate(float x) noexcept { if (std::isfinite(x)) lfo2Rate_ = std::max(0.01f, std::min(x, 20.0f)); }
void Synth::setLfo2Depth(float x) noexcept { if (std::isfinite(x)) lfo2Depth_ = clamp01(x); }
void Synth::setLfo2Target(float x) noexcept { if (std::isfinite(x)) lfo2Target_ = std::max(0, std::min(static_cast<int>(std::lround(x)), 2)); }
void Synth::setFilterEnvAmount(float x) noexcept { if (std::isfinite(x)) filterEnvAmount_ = std::max(-1.0f, std::min(x, 1.0f)); }
void Synth::setFilterEnvAttack(float x) noexcept { if (std::isfinite(x)) filterEnvAttack_ = std::max(0.001f, std::min(x, 10.0f)); }
void Synth::setFilterEnvDecay(float x) noexcept { if (std::isfinite(x)) filterEnvDecay_ = std::max(0.001f, std::min(x, 10.0f)); }
void Synth::setFilterEnvSustain(float x) noexcept { if (std::isfinite(x)) filterEnvSustain_ = clamp01(x); }
void Synth::setFilterEnvRelease(float x) noexcept { if (std::isfinite(x)) filterEnvRelease_ = std::max(0.001f, std::min(x, 10.0f)); }
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
        const double vibrato = std::pow(2.0, std::sin(lfoPhase_ * 2.0 * pi) * lfoDepth_ / 12.0);
        lfoPhase_ += lfoRate_ / sampleRate_;
        if (lfoPhase_ >= 1.0) lfoPhase_ -= std::floor(lfoPhase_);
        const float lfo2 = static_cast<float>(std::sin(lfo2Phase_ * 2.0 * pi)) * lfo2Depth_;
        lfo2Phase_ += lfo2Rate_ / sampleRate_;
        if (lfo2Phase_ >= 1.0) lfo2Phase_ -= std::floor(lfo2Phase_);
        for (auto& v : voices_) {
            if (v.stage == Voice::Off) continue;
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
                voiceSample += sampleWave(v.phase[osc], oscPosition_[osc], oscWave_[osc]) * oscLevel_[osc];
                v.phase[osc] += v.phaseStep * vibrato * std::pow(2.0, oscTune_[osc] / 12.0);
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
            const float mod = filterEnvAmount_ * v.filterEnv + (lfo2Target_ == 1 ? lfo2 : 0.0f);
            const float effectiveCutoff = std::max(30.0f, std::min(18000.0f, cutoff_ * std::pow(2.0f, 5.0f * mod)));
            const float alpha = static_cast<float>(1.0 - std::exp(-2.0 * pi * effectiveCutoff / sampleRate_));
            v.filterState += alpha * (input - v.filterState);
            mixed += v.filterState * v.env * v.velocity;
        }
        const float output = std::max(-1.0f, std::min(mixed * gain_, 1.0f));
        left[i] = output;
        right[i] = output;
    }
}
}
