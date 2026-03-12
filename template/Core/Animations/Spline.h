#pragma once
#include <vector>
#include <algorithm>

struct SplineSample
{
    float3 position;
    float3 tangent;
};

class CatmullRomSpline
{
public:

    // control points
    std::vector<float3> points;

    // configuration
    bool loop = false;

    // arc-length info
    float totalArcLength = 0.0f;

    // ---- Setup ----
    void AddPoint(const float3& p);
    void Clear();
    void BuildArcLengthTable();

    // ---- Sampling ----
    float3 Evaluate(float t) const;
    float3 EvaluateTangent(float t) const;
    SplineSample Sample(float t) const;

    // ---- Arc-length sampling (constant speed) ----
    float SampleByArcLength(float distance) const;

    int SegmentCount() const;

private:

    std::vector<float> arcTable;
    std::vector<float> paramTable;

    static constexpr int SAMPLES_PER_SEG = 100;

    int WrapIndex(int i) const;
};