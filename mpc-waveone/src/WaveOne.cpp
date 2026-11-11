#include "WaveOne.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace waveone {
namespace {
constexpr double pi = 3.14159265358979323846;
float clamp01(float value) noexcept { return std::clamp(value, 0.0f, 1.0f); }
}
Synth::Synth() {
    // Basic prototype waveforms. Band-limited oscillator is an upcoming improvement.
    for (int i = 0; i < kTableSize; ++i) {
        const double phase = static_cast<double>(i) / kTableSize;
        tables_[0][i] = static_cast<float>(std::sin(2.0 * pi * phase));
        tables_[1][i] = static_cast<float>(2.0 * phase - 1.0);
        tables_[2][i] = phase < 0.5 ? 1.0f : -1.0f;
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
    }
}
void Synth::allNotesOff() noexcept { for (auto& v : voices_) v = {}; }
void Synth::setPosition(float x) noexcept { if (std::isfinite(x)) position_ = std::clamp(x, 0.0f, 2.0f); }
void Synth::setAttack(float x) noexcept { if (std::isfinite(x)) attack_ = std::clamp(x, 0.001f, 10.0f); }
void Synth::setDecay(float x) noexcept { if (std::isfinite(x)) decay_ = std::clamp(x, 0.001f, 10.0f); }
void Synth::setSustain(float x) noexcept { if (std::isfinite(x)) sustain_ = clamp01(x); }
void Synth::setRelease(float x) noexcept { if (std::isfinite(x)) release_ = std::clamp(x, 0.001f, 10.0f); }
void Synth::setGain(float x) noexcept { if (std::isfinite(x)) gain_ = clamp01(x); }
float Synth::sampleWave(double phase) const noexcept {
    const double wrapped = phase - std::floor(phase);
    const double idx = wrapped * kTableSize;
    const int a = static_cast<int>(idx) % kTableSize;
    const int b = (a + 1) % kTableSize;
    const float frac = static_cast<float>(idx - std::floor(idx));
    const int first = std::min(1, static_cast<int>(position_));
    const int second = std::min(2, first + 1);
    const float blend = position_ - first;
    auto read = [&](int table) { return tables_[table][a] + frac * (tables_[table][b] - tables_[table][a]); };
    return (1.0f - blend) * read(first) + blend * read(second);
}
void Synth::render(float* left, float* right, std::size_t frames) noexcept {
    if (!left || !right) return;
    for (std::size_t i = 0; i < frames; ++i) {
        float mixed = 0.0f;
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
            mixed += sampleWave(v.phase) * v.env * v.velocity;
            v.phase += v.phaseStep;
            if (v.phase >= 1.0) v.phase -= std::floor(v.phase);
        }
        const float output = std::clamp(mixed * gain_, -1.0f, 1.0f);
        left[i] = output;
        right[i] = output;
    }
}
}
