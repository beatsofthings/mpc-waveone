#include "WaveOne.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <vector>
int main() {
    waveone::Synth synth;
    synth.prepare(48000);
    constexpr int size = 48000;
    std::vector<float> left(size), right(size);
    synth.render(left.data(), right.data(), size);
    for (int i = 0; i < size; ++i) assert(left[i] == 0 && right[i] == 0);
    synth.noteOn(69, 1.0f);
    synth.render(left.data(), right.data(), size);
    float peak = 0.0f;
    for (int i = 0; i < size; ++i) { assert(std::isfinite(left[i])); assert(left[i] == right[i]); peak = std::max(peak, std::abs(left[i])); }
    assert(peak > 0.01f && peak <= 1.0f);
    synth.noteOff(69);
    synth.render(left.data(), right.data(), size);
    float tail = 0.0f;
    for (int i = size - 100; i < size; ++i) tail = std::max(tail, std::abs(left[i]));
    assert(tail < 0.0001f);
    synth.setPosition(1.75f);
    synth.noteOn(60, 0.8f);
    synth.render(left.data(), right.data(), size);
    assert(std::isfinite(left[size-1]));
    synth.allNotesOff();
    synth.render(left.data(), right.data(), size);
    for (int i = 0; i < size; ++i) assert(left[i] == 0.0f);
}
