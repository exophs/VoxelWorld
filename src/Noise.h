#pragma once

// Simple 2D/3D value noise + fBm for terrain
class Noise {
public:
    explicit Noise(unsigned int seed = 1337);

    float Value2D(float x, float y) const;
    float FBM2D(float x, float y, int octaves = 4, float lacunarity = 2.0f, float gain = 0.5f) const;

private:
    unsigned int m_seed;
    float Hash(int x, int y) const;
    float Smooth(float t) const;
};