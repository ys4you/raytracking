#include "template.h"
#include "Core/Animations/Spline.h"
#include "PhysicsBall.h"
#include <cstdio>
#include <cmath>

static int g_passed = 0;
static int g_failed = 0;

static void Check(bool condition, const char* name, const char* detail = "")
{
    if (condition) { g_passed++; printf("  [PASS] %s %s\n", name, detail); }
    else { g_failed++; printf("  [FAIL] %s %s\n", name, detail); }
}

static bool Near(float a, float b, float eps = 1e-4f) { return fabsf(a - b) < eps; }

static bool Near3(float3 a, float3 b, float eps = 1e-4f)
{
    return Near(a.x, b.x, eps) && Near(a.y, b.y, eps) && Near(a.z, b.z, eps);
}


static void TestSpline()
{
    printf("\n=== Catmull-Rom Spline Tests ===\n");

    // Non-looping, 4 collinear points: segment 0 starts at points[0]
    // because WrapIndex clamps seg-1 to 0.
    {
        CatmullRomSpline spline;
        spline.loop = false;
        spline.AddPoint(float3(0, 0, 0));
        spline.AddPoint(float3(1, 0, 0));
        spline.AddPoint(float3(2, 0, 0));
        spline.AddPoint(float3(3, 0, 0));
        spline.BuildArcLengthTable();

        float3 p0 = spline.Evaluate(0.0f);
        Check(Near3(p0, float3(0, 0, 0)),
            "CR-Linear: Evaluate(0) == points[0]", "(expected (0,0,0))");

        float3 p1 = spline.Evaluate(1.0f);
        Check(Near3(p1, float3(1, 0, 0), 0.01f),
            "CR-Linear: Evaluate(1) == points[1]", "(expected (1,0,0))");

        float3 p2 = spline.Evaluate(2.0f);
        Check(Near3(p2, float3(2, 0, 0), 0.01f),
            "CR-Linear: Evaluate(2) == points[2]", "(expected (2,0,0))");

        float3 tanN = normalize(spline.EvaluateTangent(1.5f));
        Check(Near3(tanN, float3(1, 0, 0), 0.01f),
            "CR-Linear: tangent at t=1.5 == +X", "(expected (1,0,0))");
    }

    // Curved path for arc-length tests
    {
        CatmullRomSpline spline;
        spline.loop = false;
        spline.AddPoint(float3(0, 0, 0));
        spline.AddPoint(float3(1, 0, 0));
        spline.AddPoint(float3(1, 1, 0));
        spline.AddPoint(float3(0, 1, 0));
        spline.BuildArcLengthTable();

        Check(spline.SegmentCount() == 3,
            "CR-Curved: SegmentCount == 3", "(4 points, non-looping)");

        Check(Near3(spline.Evaluate(0.0f), float3(0, 0, 0)),
            "CR-Curved: Evaluate(0) == points[0]", "(expected (0,0,0))");

        Check(spline.totalArcLength > 0.0f,
            "CR-Curved: totalArcLength > 0", "");

        Check(Near(spline.SampleByArcLength(0.0f), 0.0f, 0.01f),
            "CR-Curved: SampleByArcLength(0) ~ 0", "");

        float tEnd = spline.SampleByArcLength(spline.totalArcLength);
        Check(tEnd > (float)spline.SegmentCount() * 0.8f,
            "CR-Curved: SampleByArcLength(total) ~ end param", "");

        // Uniform speed: equal arc-length steps should produce equal chord lengths
        float3 q1 = spline.Evaluate(spline.SampleByArcLength(spline.totalArcLength * 0.25f));
        float3 q2 = spline.Evaluate(spline.SampleByArcLength(spline.totalArcLength * 0.50f));
        float3 q3 = spline.Evaluate(spline.SampleByArcLength(spline.totalArcLength * 0.75f));

        float ratio = length(q3 - q2) / max(length(q2 - q1), 0.001f);
        Check(ratio > 0.7f && ratio < 1.4f,
            "CR-Curved: arc-length uniform speed", "(25%-50% chord ~ 50%-75% chord)");
    }

    // Looping spline
    {
        CatmullRomSpline spline;
        spline.loop = true;
        spline.AddPoint(float3(0, 0, 0));
        spline.AddPoint(float3(1, 0, 0));
        spline.AddPoint(float3(1, 1, 0));
        spline.AddPoint(float3(0, 1, 0));
        spline.BuildArcLengthTable();

        Check(spline.SegmentCount() == 4,
            "CR-Loop: SegmentCount == 4", "(4 points, looping)");

        Check(Near3(spline.Evaluate(0.0f), spline.Evaluate(4.0f), 0.01f),
            "CR-Loop: Evaluate(0) ~ Evaluate(N)", "(loop wrapping)");

        Check(spline.SampleByArcLength(spline.totalArcLength + 0.1f) >= 0.0f,
            "CR-Loop: SampleByArcLength wraps past total", "");
    }
}


using Tmpl8::PhysicsBall;

static float KineticEnergy(const PhysicsBall& b)
{
    return 0.5f * b.mass * dot(b.velocity, b.velocity);
}

static float3 Momentum(const PhysicsBall& b)
{
    return b.velocity * b.mass;
}

static void TestResolvePair()
{
    printf("\n=== PhysicsBall::ResolvePair Tests ===\n");

    // No-op: far apart, nothing should change
    {
        PhysicsBall a(float3(0.0f, 0.5f, 0.5f), 0.05f, 1.0f);
        PhysicsBall b(float3(0.5f, 0.5f, 0.5f), 0.05f, 1.0f);
        a.velocity = float3(1, 0, 0);
        b.velocity = float3(-1, 0, 0);

        float3 posA = a.position, posB = b.position;
        float3 velA = a.velocity, velB = b.velocity;
        PhysicsBall::ResolvePair(a, b);

        Check(Near3(a.position, posA, 1e-6f) && Near3(b.position, posB, 1e-6f) &&
            Near3(a.velocity, velA, 1e-6f) && Near3(b.velocity, velB, 1e-6f),
            "Collision: no-op when not overlapping", "(positions and velocities unchanged)");
    }

    // Depenetration: overlapping balls should move apart
    {
        PhysicsBall a(float3(0.50f, 0.5f, 0.5f), 0.05f, 1.0f);
        PhysicsBall b(float3(0.56f, 0.5f, 0.5f), 0.05f, 1.0f);
        a.velocity = float3(0); b.velocity = float3(0);

        float distBefore = length(b.position - a.position);
        PhysicsBall::ResolvePair(a, b);

        Check(length(b.position - a.position) > distBefore,
            "Collision: depenetration increases distance", "(balls pushed apart)");
    }

    // Equal-mass depenetration should be symmetric
    {
        PhysicsBall a(float3(0.50f, 0.5f, 0.5f), 0.05f, 1.0f);
        PhysicsBall b(float3(0.56f, 0.5f, 0.5f), 0.05f, 1.0f);
        a.velocity = float3(0); b.velocity = float3(0);

        float3 posA = a.position, posB = b.position;
        PhysicsBall::ResolvePair(a, b);

        Check(Near(length(a.position - posA), length(b.position - posB), 1e-5f),
            "Collision: equal-mass depenetration is symmetric", "(both balls move same distance)");
    }

    // Impulse tests: relVn = dot(va-vb, n) must be < 0 to fire.
    // n = normalize(b.pos - a.pos) = +X, so va=(-v,0,0) vb=(+v,0,0)
    // gives relVn = -2v < 0.

    // Momentum conservation
    {
        PhysicsBall a(float3(0.47f, 0.5f, 0.5f), 0.05f, 1.0f);
        PhysicsBall b(float3(0.56f, 0.5f, 0.5f), 0.05f, 1.0f);
        a.velocity = float3(-1, 0, 0); b.velocity = float3(1, 0, 0);
        a.restitution = b.restitution = 0.8f;

        float3 momBefore = Momentum(a) + Momentum(b);
        PhysicsBall::ResolvePair(a, b);

        Check(Near3(momBefore, Momentum(a) + Momentum(b), 1e-4f),
            "Collision: momentum conserved", "(impulse is equal and opposite)");
    }

    // Inelastic: KE must not increase
    {
        PhysicsBall a(float3(0.47f, 0.5f, 0.5f), 0.05f, 1.0f);
        PhysicsBall b(float3(0.56f, 0.5f, 0.5f), 0.05f, 1.0f);
        a.velocity = float3(-2, 0.3f, 0); b.velocity = float3(1, -0.2f, 0);
        a.restitution = b.restitution = 0.5f;

        float keBefore = KineticEnergy(a) + KineticEnergy(b);
        PhysicsBall::ResolvePair(a, b);

        Check(KineticEnergy(a) + KineticEnergy(b) <= keBefore + 1e-4f,
            "Collision: energy does not increase (e=0.5)", "");
    }

    // Elastic: KE must be exactly conserved
    {
        PhysicsBall a(float3(0.47f, 0.5f, 0.5f), 0.05f, 1.0f);
        PhysicsBall b(float3(0.56f, 0.5f, 0.5f), 0.05f, 1.0f);
        a.velocity = float3(-1, 0, 0); b.velocity = float3(1, 0, 0);
        a.restitution = b.restitution = 1.0f;

        float keBefore = KineticEnergy(a) + KineticEnergy(b);
        PhysicsBall::ResolvePair(a, b);

        Check(Near(keBefore, KineticEnergy(a) + KineticEnergy(b), 1e-3f),
            "Collision: elastic (e=1) conserves KE", "");
    }

    // Argument-order symmetry
    {
        PhysicsBall a1(float3(0.47f, 0.5f, 0.5f), 0.05f, 2.0f);
        PhysicsBall b1(float3(0.56f, 0.5f, 0.5f), 0.05f, 1.0f);
        a1.velocity = float3(-1.5f, 0, 0); b1.velocity = float3(0.5f, 0, 0);
        a1.restitution = b1.restitution = 0.7f;

        PhysicsBall a2 = a1, b2 = b1;
        PhysicsBall::ResolvePair(a1, b1);
        PhysicsBall::ResolvePair(b2, a2);

        Check(Near3(a1.velocity, a2.velocity, 1e-4f) &&
            Near3(b1.velocity, b2.velocity, 1e-4f),
            "Collision: symmetric under argument swap", "(ResolvePair(a,b) == ResolvePair(b,a))");
    }

    // Lighter ball should get a larger velocity change
    {
        PhysicsBall heavy(float3(0.47f, 0.5f, 0.5f), 0.05f, 10.0f);
        PhysicsBall light(float3(0.56f, 0.5f, 0.5f), 0.05f, 1.0f);
        heavy.velocity = float3(-1, 0, 0); light.velocity = float3(0, 0, 0);
        heavy.restitution = light.restitution = 1.0f;

        float3 hBefore = heavy.velocity, lBefore = light.velocity;
        PhysicsBall::ResolvePair(heavy, light);

        Check(length(light.velocity - lBefore) > length(heavy.velocity - hBefore),
            "Collision: lighter ball deflects more", "(mass 1 vs mass 10)");
    }

    // Equal-mass elastic: velocities swap along collision normal
    {
        PhysicsBall a(float3(0.47f, 0.5f, 0.5f), 0.05f, 1.0f);
        PhysicsBall b(float3(0.56f, 0.5f, 0.5f), 0.05f, 1.0f);
        a.velocity = float3(-1, 0, 0); b.velocity = float3(1, 0, 0);
        a.restitution = b.restitution = 1.0f;

        PhysicsBall::ResolvePair(a, b);

        Check(Near(a.velocity.x, 1.0f, 0.05f) && Near(b.velocity.x, -1.0f, 0.05f),
            "Collision: equal-mass elastic swaps velocities", "(analytical: vA->+1, vB->-1)");
    }
}


void RunAllTests()
{
    g_passed = 0;
    g_failed = 0;

    printf("============================================\n");
    printf("  ORDER - Unit Test Suite\n");
    printf("============================================\n");

    TestSpline();
    TestResolvePair();

    printf("\n============================================\n");
    printf("  Results: %d passed, %d failed, %d total\n",
        g_passed, g_failed, g_passed + g_failed);
    printf("============================================\n\n");
}