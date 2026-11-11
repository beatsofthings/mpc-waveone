#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace waveone {
class Synth {
public:
    static constexpr int kVoices = 8;
    static constexpr int kTableSize = 2048;
    Synth();
    void prepare(double sampleRate) noexcept;
    void noteOn(int midiNote, float velocity) noexcept;
    void noteOff(int midiNote) noexcept;
    void allNotesOff() noexcept;
    void setPosition(float position) noexcept;
    void setOscPosition(int osc, float position) noexcept;
    void setOscLevel(int osc, float level) noexcept;
    void setOscTune(int osc, float semitones) noexcept;
    void setOscEnabled(int osc, float enabled) noexcept;
    void setSubLevel(float level) noexcept;
    void setFilterCutoff(float hz) noexcept;
    void setFilterResonance(float amount) noexcept;
    void setLfoRate(float hz) noexcept;
    void setLfoDepth(float semitones) noexcept;
    void setAttack(float seconds) noexcept;
    void setDecay(float seconds) noexcept;
    void setSustain(float level) noexcept;
    void setRelease(float seconds) noexcept;
    void setGain(float gain) noexcept;
    void render(float* left, float* right, std::size_t frames) noexcept;
private:
    struct Voice { int note = -1; std::array<double,3> phase{{0.0,0.0,0.0}}; double phaseStep = 0.0; double subPhase = 0.0; float filterState = 0.0f; float env = 0.0f; float velocity = 0.0f; std::uint64_t started = 0; enum Stage { Off, Attack, Decay, Sustain, Release } stage = Off; float releaseStep = 0.0f; };
    std::array<Voice, kVoices> voices_{};
    std::array<std::array<float, kTableSize>, 3> tables_{};
    double sampleRate_ = 48000.0;
    std::array<float,3> oscPosition_{{0.0f,0.0f,0.0f}};
    std::array<float,3> oscLevel_{{1.0f,0.0f,0.0f}};
    std::array<float,3> oscTune_{{0.0f,0.0f,0.0f}};
    std::array<float,3> oscEnabled_{{1.0f,1.0f,1.0f}};
    float subLevel_ = 0.0f, cutoff_ = 18000.0f, resonance_ = 0.0f;
    float lfoRate_ = 2.0f, lfoDepth_ = 0.0f;
    double lfoPhase_ = 0.0;
    float attack_ = 0.01f, decay_ = 0.12f, sustain_ = 0.7f, release_ = 0.25f, gain_ = 0.2f;
    std::uint64_t counter_ = 0;
    float sampleWave(double phase, float position) const noexcept;
};
}
