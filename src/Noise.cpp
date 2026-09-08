#include "Noise.h"
#include <cmath>
#include <algorithm>

Noise::Noise(unsigned int seed) : m_seed(seed) {}

float Noise::Hash(int x, int y) const {
    // Simple integer hash
    unsigned int h = m_seed;
    h ^= (unsigned int)x * 374761393u;
    h ^= (unsigned int)y * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h = h ^ (h >> 16);
    return (h & 0xFFFFFF) / static_cast<float>(0xFFFFFF);
}

float Noise::Smooth(float t) const {
    return t * t * (3.0f - 2.0f * t);
}

float Noise::Value2D(float x, float y) const {
    int x0 = (int)std::floor(x);
    int y0 = (int)std::floor(y);
    int x1 = x0 + 1;
    int y1 = y0 + 1;

    float sx = Smooth(x - x0);
    float sy = Smooth(y - y0);

    float n00 = Hash(x0, y0);
    float n10 = Hash(x1, y0);
    float n01 = Hash(x0, y1);
    float n11 = Hash(x1, y1);

    float ix0 = n00 + (n10 - n00) * sx;
    float ix1 = n01 + (n11 - n01) * sx;
    return ix0 + (ix1 - ix0) * sy;
}

float Noise::FBM2D(float x, float y, int octaves, float lacunarity, float gain) const {
    float sum = 0.0f;
    float amp = 1.0f;
    float freq = 1.0f;
    float maxAmp = 0.0f;

    for (int i = 0; i < octaves; ++i) {
        sum += Value2D(x * freq, y * freq) * amp;
        maxAmp += amp;
        amp *= gain;
        freq *= lacunarity;
    }
    return sum / maxAmp;
}