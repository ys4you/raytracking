#include "template.h"
#include "Spline.h"

void CatmullRomSpline::AddPoint(const float3& p)
{
    points.push_back(p);
}

void CatmullRomSpline::Clear()
{
    points.clear();
    arcTable.clear();
    paramTable.clear();
    totalArcLength = 0.0f;
}

int CatmullRomSpline::WrapIndex(int i) const
{
    int n = (int)points.size();

    if (loop)
        return (i % n + n) % n;

    return std::clamp(i, 0, n - 1);
}

int CatmullRomSpline::SegmentCount() const
{
    if (points.size() < 2) return 0;
    return loop ? (int)points.size() : (int)points.size() - 1;
}

float3 CatmullRomSpline::Evaluate(float t) const
{
    int segs = SegmentCount();
    if (segs == 0) return float3(0);

    if (loop)
    {
        t = fmodf(t, (float)segs);
        if (t < 0) t += segs;
    }
    else
    {
        t = std::clamp(t, 0.0f, (float)segs - 1e-4f);
    }

    int seg = (int)floorf(t);
    float lt = t - seg;

    float3 P0 = points[WrapIndex(seg - 1)];
    float3 P1 = points[WrapIndex(seg)];
    float3 P2 = points[WrapIndex(seg + 1)];
    float3 P3 = points[WrapIndex(seg + 2)];

    float lt2 = lt * lt;
    float lt3 = lt2 * lt;

    return 0.5f * (
        P1 * 2.0f +
        (P2 - P0) * lt +
        (P0 * 2.0f - P1 * 5.0f + P2 * 4.0f - P3) * lt2 +
        (P1 * 3.0f - P0 - P2 * 3.0f + P3) * lt3
        );
}

float3 CatmullRomSpline::EvaluateTangent(float t) const
{
    int segs = SegmentCount();
    if (segs == 0) return float3(0, 0, 1);

    if (loop)
    {
        t = fmodf(t, (float)segs);
        if (t < 0) t += segs;
    }
    else
    {
        t = std::clamp(t, 0.0f, (float)segs - 1e-4f);
    }

    int seg = (int)floorf(t);
    float lt = t - seg;

    float3 P0 = points[WrapIndex(seg - 1)];
    float3 P1 = points[WrapIndex(seg)];
    float3 P2 = points[WrapIndex(seg + 1)];
    float3 P3 = points[WrapIndex(seg + 2)];

    float lt2 = lt * lt;

    return 0.5f * (
        (P2 - P0) +
        (P0 * 4.0f - P1 * 10.0f + P2 * 8.0f - P3 * 2.0f) * lt +
        (P1 * 9.0f - P0 * 3.0f - P2 * 9.0f + P3 * 3.0f) * lt2
        );
}

SplineSample CatmullRomSpline::Sample(float t) const
{
    SplineSample s;
    s.position = Evaluate(t);
    s.tangent = normalize(EvaluateTangent(t));
    return s;
}

void CatmullRomSpline::BuildArcLengthTable()
{
    arcTable.clear();
    paramTable.clear();

    int segs = SegmentCount();
    if (segs == 0) return;

    int total = segs * SAMPLES_PER_SEG + 1;

    float3 prev = Evaluate(0.0f);
    float cumulative = 0.0f;

    arcTable.push_back(0.0f);
    paramTable.push_back(0.0f);

    for (int i = 1; i < total; i++)
    {
        float t = (float)i / (float)SAMPLES_PER_SEG;
        float3 cur = Evaluate(t);

        cumulative += length(cur - prev);

        arcTable.push_back(cumulative);
        paramTable.push_back(t);

        prev = cur;
    }

    totalArcLength = cumulative;
}

float CatmullRomSpline::SampleByArcLength(float dist) const
{
    if (arcTable.empty()) return 0.0f;

    if (loop)
    {
        dist = fmodf(dist, totalArcLength);
        if (dist < 0) dist += totalArcLength;
    }
    else
    {
        dist = std::clamp(dist, 0.0f, totalArcLength);
    }

    auto it = std::lower_bound(arcTable.begin(), arcTable.end(), dist);
    int idx = (int)(it - arcTable.begin());

    if (idx >= arcTable.size() - 1)
        return paramTable.back();

    float lenA = arcTable[idx];
    float lenB = arcTable[idx + 1];

    float frac = (dist - lenA) / (lenB - lenA);

    return paramTable[idx] + frac * (paramTable[idx + 1] - paramTable[idx]);
}