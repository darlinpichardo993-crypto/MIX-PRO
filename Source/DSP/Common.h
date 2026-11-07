#pragma once
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <vector>
namespace mp {
constexpr double kPi = 3.14159265358979323846;
inline float dbToGain(float db) { return std::pow(10.0f, db * 0.05f); }
inline float gainToDb(float g) { return 20.0f * std::log10(std::max(g, 1.0e-9f)); }
template <class T> inline T clampT(T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float flushDenormal(float x) { return std::fabs(x) < 1e-20f ? 0.f : x; }
}
