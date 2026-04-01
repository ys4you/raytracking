#include "template.h"
#include "Core/ShadingPoint.h"
#include "Core/Lighting/PointLight.h"
#include "Core/Lighting/DirectionalLight.h"
#include "Core/Lighting/SpotLight.h"
#include "Core/Lighting/AreaLight.h"
#include <immintrin.h>

#include "Core/Audio/AudioSystem.h"


float3 Renderer::Trace(Ray& ray, int depth, int, int)
{
    constexpr int MAX_DEPTH = 15;
    if (depth >= MAX_DEPTH)
        return float3(0, 0, 0);

    scene.FindNearest(ray);

    if (ray.voxel == 0 && ray.sphereIndex < 0 && ray.instanceIndex < 0)
        return sky.GetSkyColor(ray.D);

    const Material* matPtr = nullptr;
    if (ray.sphereIndex >= 0)
        matPtr = &scene.GetSphereMat((uint)scene.sphereSOA.material[ray.sphereIndex]);
    else if (ray.instanceIndex >= 0 && ray.voxel > 0)
        matPtr = &scene.GetMat(ray.materialIndex);
    else if (ray.voxel > 0)
        matPtr = &scene.GetMat(ray.materialIndex);

    if (!matPtr) return sky.GetSkyColor(ray.D);
    const Material& mat = *matPtr;

    if (fastSphereShading && ray.sphereIndex >= 0 &&
        static_cast<int>(scene.spheres.size()) >= fastSphereThreshold &&
        mat.type != MaterialType::Emissive)
    {
        const float3 hitPos = ray.O + ray.t * ray.D;
        const float3 centre = float3(
            scene.sphereSOA.cx[ray.sphereIndex],
            scene.sphereSOA.cy[ray.sphereIndex],
            scene.sphereSOA.cz[ray.sphereIndex]
        );
        const float3 diff = hitPos - centre;

        const float  invLen = _mm_cvtss_f32(_mm_rsqrt_ss(_mm_set_ss(dot(diff, diff))));

    	const float3 N = diff * invLen;

        const float  ndotl = max(0.0f, dot(N, fastSphereLightDir));
        float3 color = mat.albedo * (fastSphereAmbient + (1.0f - fastSphereAmbient) * ndotl);

        for (const PointLight& pl : lights.points)
        {
            if (!pl.enabled) continue;
            const float3 toLight = pl.position - hitPos;
            const float  dist2 = dot(toLight, toLight);
            const float  invDist = _mm_cvtss_f32(_mm_rsqrt_ss(_mm_set_ss(dist2)));
            const float3 L = toLight * invDist;
            const float  nl = max(0.0f, dot(N, L));
            color += mat.albedo * pl.color * nl * (invDist * invDist);
        }

        return color;
    }

    ShadingPoint sp;
    sp.position = ray.IntersectionPoint();
    sp.normal = ray.GetNormal(scene);
    sp.albedo = ray.GetAlbedo(scene);

    if (debugNormals)
        return 0.5f * (sp.normal + float3(1.0f));

    switch (mat.type)
    {
    case MaterialType::Lambertian:
    {
        float hemi = 0.5f + 0.5f * sp.normal.y;
        float3 color = float3(0.25f + 0.15f * hemi);

        for (const PointLight& l : lights.points)             if (l.enabled) color += IlluminatePoint(l, sp, scene);
        for (const DirectionalLight& l : lights.directionals) if (l.enabled) color += IlluminateDirectional(l, sp, scene);
        for (const SpotLight& l : lights.spots)               if (l.enabled) color += IlluminateSpot(l, sp, scene);
        for (const AreaLight& l : lights.areas)                if (l.enabled) color += IlluminateArea(l, sp, scene);
        color *= mat.albedo;
        return color;
    }
    case MaterialType::Metal:
    {
        float3 N = sp.normal;
        float3 R = normalize(ray.D - 2.0f * dot(ray.D, N) * N);
        if (mat.roughness > 0.0f) R += mat.roughness * RandomInUnitSphere();
        R = normalize(R);
        Ray reflectedRay(sp.position + N * EPSILON, R);
        return Trace(reflectedRay, depth + 1) * mat.albedo;
    }
    case MaterialType::Dielectric:
    {
        float3 N = sp.normal;
        float3 I = normalize(ray.D);
        float3 refracted;
        float  ni_over_nt = dot(I, N) > 0 ? mat.ior : 1.0f / mat.ior;
        float  reflect_prob = 1.0f;
        if (Refract(I, N, ni_over_nt, refracted))
            reflect_prob = Schlick(dot(I, N), mat.ior);

        reflect_prob = max(reflect_prob, mat.metallic);

        const float voxelSkip = 2.0f / 256.0f;

        if (mat.metallic > 0.0f && depth < 1)
        {
            Ray reflectedRay(sp.position + N * EPSILON, reflect(I, N));
            Ray throughRay(sp.position - N * voxelSkip, I);
            float3 refl = Trace(reflectedRay, depth + 1);
            float3 thru = Trace(throughRay, depth + 1);
            return reflect_prob * refl + (1.0f - reflect_prob) * thru;
        }

        if (RandomFloat() < reflect_prob)
        {
            Ray reflectedRay(sp.position + N * EPSILON, reflect(I, N));
            return Trace(reflectedRay, depth + 1);
        }
        else
        {
            Ray refractedRay(sp.position - N * voxelSkip, refracted);
            return Trace(refractedRay, depth + 1);
        }
    }
    case MaterialType::Emissive:
        return mat.emission * mat.emissionStr;
    }

    return sky.GetSkyColor(ray.D);
}


void Renderer::ApplyBloom() const
{
#pragma omp parallel for schedule(static)
    for (int by = 0; by < BLOOM_H; by++)
        for (int bx = 0; bx < BLOOM_W; bx++)
        {
            float3 sum = float3(0, 0, 0);
            for (int dy = 0; dy < 4; dy++)
                for (int dx = 0; dx < 4; dx++)
                {
                    int sx = min(bx * 4 + dx, SCRWIDTH - 1);
                    int sy = min(by * 4 + dy, SCRHEIGHT - 1);
                    sum += accumulator[sx + sy * SCRWIDTH];
                }
            float3 avg = sum * (1.0f / 16.0f);
            float lum = avg.x * 0.299f + avg.y * 0.587f + avg.z * 0.114f;
            bloomDown[bx + by * BLOOM_W] = (lum > bloomThreshold)
                ? avg * ((lum - bloomThreshold) / lum)
                : float3(0, 0, 0);
        }

    for (int pass = 0; pass < 2; pass++)
    {
        const int R = bloomRadius * (pass + 1);
        const float invK = 1.0f / (float)(2 * R + 1);

#pragma omp parallel for schedule(static)
        for (int by = 0; by < BLOOM_H; by++)
            for (int bx = 0; bx < BLOOM_W; bx++)
            {
                float3 s = float3(0, 0, 0);
                for (int dx = -R; dx <= R; dx++)
                {
                    int sx = bx + dx;
                    if (sx < 0) sx = 0;
                    if (sx >= BLOOM_W) sx = BLOOM_W - 1;
                    s += bloomDown[sx + by * BLOOM_W];
                }
                bloomTemp[bx + by * BLOOM_W] = s * invK;
            }

#pragma omp parallel for schedule(static)
        for (int by = 0; by < BLOOM_H; by++)
            for (int bx = 0; bx < BLOOM_W; bx++)
            {
                float3 s = float3(0, 0, 0);
                for (int dy = -R; dy <= R; dy++)
                {
                    int sy = by + dy;
                    if (sy < 0) sy = 0;
                    if (sy >= BLOOM_H) sy = BLOOM_H - 1;
                    s += bloomTemp[bx + sy * BLOOM_W];
                }
                bloomDown[bx + by * BLOOM_W] = s * invK;
            }
    }

#pragma omp parallel for schedule(static)
    for (int y = 0; y < SCRHEIGHT; y++)
        for (int x = 0; x < SCRWIDTH; x++)
        {
            float bx = ((float)x + 0.5f) * 0.25f - 0.5f;
            float by = ((float)y + 0.5f) * 0.25f - 0.5f;
            int x0 = (int)floorf(bx), y0 = (int)floorf(by);
            float fx = bx - (float)x0, fy = by - (float)y0;
            int x1 = x0 + 1, y1 = y0 + 1;
            if (x0 < 0) x0 = 0; if (x1 >= BLOOM_W) x1 = BLOOM_W - 1;
            if (y0 < 0) y0 = 0; if (y1 >= BLOOM_H) y1 = BLOOM_H - 1;
            if (x0 >= BLOOM_W) x0 = BLOOM_W - 1;
            if (y0 >= BLOOM_H) y0 = BLOOM_H - 1;

            float3 b00 = bloomDown[x0 + y0 * BLOOM_W];
            float3 b10 = bloomDown[x1 + y0 * BLOOM_W];
            float3 b01 = bloomDown[x0 + y1 * BLOOM_W];
            float3 b11 = bloomDown[x1 + y1 * BLOOM_W];
            float3 top = b00 * (1.0f - fx) + b10 * fx;
            float3 bot = b01 * (1.0f - fx) + b11 * fx;
            float3 bloom = top * (1.0f - fy) + bot * fy;

            const int idx = x + y * SCRWIDTH;
            float3 hdr = accumulator[idx] + bloom * bloomIntensity;
            float3 mapped;
            mapped.x = hdr.x / (1.0f + hdr.x);
            mapped.y = hdr.y / (1.0f + hdr.y);
            mapped.z = hdr.z / (1.0f + hdr.z);
            screen->pixels[idx] = RGBF32_to_RGB8(mapped);
        }
}


void Renderer::Init()
{
    std::cout << "screen width: " << SCRWIDTH << " screen height: " << SCRHEIGHT << std::endl;

    sampleCountPerPixel = new int[SCRWIDTH * SCRHEIGHT]();

    Surface* bn = new Surface("assets/BlueNoise256x256.png");
    assert(bn->width == BN_SIZE && bn->height == BN_SIZE);
    blueNoise = new uint8_t[BN_SIZE * BN_SIZE];
    for (int i = 0; i < BN_SIZE * BN_SIZE; i++)
    {
        uint p = bn->pixels[i];
        blueNoise[i] = (uint8_t)((p >> 16) & 255);
    }
    delete bn;

    lights.directionals.push_back(sky.sun);
    lights.directionals.push_back(sky.moon);
    {
        PointLight pl{ { 1,1,1 }, { 1,1,1 }, false };
        lights.points.push_back(pl);
    }

    history = new float3[SCRWIDTH * SCRHEIGHT];
    memset(history, 0, SCRWIDTH * SCRHEIGHT * sizeof(float3));

    rayDirTable = new float3[SCRWIDTH * SCRHEIGHT];
    rayTableDirty = true;

    bloomDown = new float3[BLOOM_W * BLOOM_H];
    bloomTemp = new float3[BLOOM_W * BLOOM_H];

    InitAccumulator();

    if (sceneManager.HasActive() && sceneManager.Active().usePhysics)
    {
        for (int i = 0; i < static_cast<int>(scene.spheres.size()); i++)
        {
            int b = physics.AddBall(scene.spheres[i].center, scene.spheres[i].radius);
            physics.balls[b].visualIndex = i;
        }
    }

    cameraSpline.BuildArcLengthTable();
    cameraFollower.spline = &cameraSpline;
    cameraFollower.speed = sceneManager.Active().splineSpeed;
    cameraFollower.loop = true;

    GameScenes::RegisterAllScenes(sceneManager);
    sceneManager.LoadScene(0, scene, camera, sky, lights);

    GameScenes::RegisterAllScenes(sceneManager);
    sceneManager.LoadScene(0, scene, camera, sky, lights);
    lastLoadedSceneID = sceneManager.CurrentID();

    if (sceneManager.HasActive() && sceneManager.Active().usePhysics)
    {
        for (int i = 0; i < static_cast<int>(scene.spheres.size()); i++)
        {
            int b = physics.AddBall(scene.spheres[i].center, scene.spheres[i].radius);
            physics.balls[b].visualIndex = i;
        }
    }

    lastLoadedSceneID = sceneManager.CurrentID();

    if (sceneManager.HasActive() && !sceneManager.Active().splinePoints.empty())
    {
        cameraSpline = CatmullRomSpline();
        for (auto& p : sceneManager.Active().splinePoints)
            cameraSpline.AddPoint(p);
        cameraSpline.BuildArcLengthTable();
        cameraFollower.spline = &cameraSpline;
        cameraFollower.speed = sceneManager.Active().splineSpeed;
        cameraFollower.loop = true;
        useSplineCamera = true;
    }
    else
    {
        useSplineCamera = false;
    }

    eventSystem.Load("events.bin");

    eventSystem.knownTypes.push_back("brickmap_speed");
    eventSystem.knownTypes.push_back("brickmap_reset");

    eventSystem.RegisterHandler("fade_in", [this](const TimeEvent& e) {
        float duration = e.param1 > 0.0f ? e.param1 : 1.0f;
        fadeOpacity = 1.0f;
        fadeTarget = 0.0f;
        fadeSpeed = 1.0f / duration;
        fadeColor = float3(e.param2, e.param3, 0);
        if (fadeColor.x == 0 && fadeColor.y == 0)
            fadeColor = float3(0, 0, 0);
        });

    eventSystem.RegisterHandler("fade_out", [this](const TimeEvent& e) {
        float duration = e.param1 > 0.0f ? e.param1 : 1.0f;
        fadeTarget = 1.0f;
        fadeSpeed = 1.0f / duration;
        fadeColor = float3(e.param2, e.param3, 0);
        if (fadeColor.x == 0 && fadeColor.y == 0)
            fadeColor = float3(0, 0, 0);
        });

    eventSystem.RegisterHandler("scene_change", [this](const TimeEvent& e) {
        int idx = (int)e.param1;
        if (idx >= 0 && idx < sceneManager.SceneCount())
        {
            sceneManager.LoadScene(idx, scene, camera, sky, lights);
            ResetAccumulator();
            rayTableDirty = true;
            lightColorsStored = false;
        }
        });

    eventSystem.RegisterHandler("camera_cut", [this](const TimeEvent& e) {
        int idx = (int)e.param1;
        if (idx >= 0 && idx < sceneManager.SceneCount())
        {
            sceneManager.LoadScene(idx, scene, camera, sky, lights);
            ResetAccumulator();
            rayTableDirty = true;
            lightColorsStored = false;
        }
        });

    eventSystem.RegisterHandler("flash", [this](const TimeEvent& e) {
        bloomIntensity = e.param1 > 0 ? e.param1 : 1.0f;
        ResetAccumulator();
        });

    eventSystem.RegisterHandler("bloom_pulse", [this](const TimeEvent& e) {
        bloomIntensity = e.param1;
        });

    eventSystem.RegisterHandler("custom", [this](const TimeEvent& e) {
        if (e.strParam == "fade_in_lights")
        {
            lightFadeActive = true;
            lightFadeTimer = 0.0f;
            lightFadeDuration = e.param1 > 0 ? e.param1 : 2.0f;
            lightFadeFrom = lightFadeMult;
            lightFadeTo = e.param2 > 0 ? e.param2 : 1.0f;
        }
        else if (e.strParam == "fade_out_lights")
        {
            lightFadeActive = true;
            lightFadeTimer = 0.0f;
            lightFadeDuration = e.param1 > 0 ? e.param1 : 2.0f;
            lightFadeFrom = lightFadeMult;
            lightFadeTo = 0.0f;
        }
        else if (e.strParam == "bloom")
        {
            bloomIntensity = e.param1;
            bloomThreshold = e.param2 > 0 ? e.param2 : bloomThreshold;
        }
        else if (e.strParam == "sun")
        {
            sky.sunIntensity = e.param1;
            sky.skyCacheDirty = true;
        }
        else if (e.strParam == "speed")
        {
            cameraFollower.speed = e.param1;
        }
        else if (e.strParam == "timeofday")
        {
            sky.timeOfDay = e.param1;
            sky.skyCacheDirty = true;
        }
        ResetAccumulator();
        });

    eventSystem.RegisterHandler("brickmap_speed", [](const TimeEvent& e) {
        auto& bm = GameScenes::GetBrickmapState();
        if (bm) bm->speedMul = e.param1;
        });
    eventSystem.RegisterHandler("brickmap_reset", [](const TimeEvent& e) {
        auto& bm = GameScenes::GetBrickmapState();
        if (bm) bm->Reset();
        });

    AudioSystem::Get().Play("assets/Audio/Music/ambient.mp3", true);
    eventSystem.SetTrack(AudioSystem::Get().GetSound("assets/Audio/Music/ambient.mp3"));
}


void Renderer::Tick(float deltaTime)
{
    const float3 orbitCenter = float3(0.5f, 0.5f, 0.5f);

    if (sceneManager.HasActive() && sceneManager.CurrentID() != lastLoadedSceneID)
    {
        lastLoadedSceneID = sceneManager.CurrentID();
        SceneDef& def = sceneManager.Active();
        if (!def.splinePoints.empty())
        {
            cameraSpline = CatmullRomSpline();
            for (auto& p : def.splinePoints)
                cameraSpline.AddPoint(p);
            cameraSpline.BuildArcLengthTable();
            cameraFollower.spline = &cameraSpline;
            cameraFollower.speed = sceneManager.Active().splineSpeed;
            cameraFollower.loop = true;
            useSplineCamera = true;
        }
        else
        {
            useSplineCamera = false;
        }

        physics.balls.clear();
        physics.accumulator = 0.0;
        if (sceneManager.Active().usePhysics)
        {
            for (int i = 0; i < static_cast<int>(scene.spheres.size()); i++)
            {
                int b = physics.AddBall(scene.spheres[i].center, scene.spheres[i].radius);
                physics.balls[b].visualIndex = i;
            }
        }

        rayTableDirty = true;
        lightColorsStored = false;
        ResetAccumulator();
        rayTableDirty = true;
        lightColorsStored = false;
        ResetAccumulator();
    }

    auto startTime = std::chrono::high_resolution_clock::now();
    sampleCount++;
    if (!scene.voxelInstances.empty()) sampleCount = 0;
    frameIndex++;
    int totalRaysThisFrame = 0;

    sky.Update(deltaTime);

    float dt = deltaTime * 0.001f;

    if (useSplineCamera && sceneManager.Active().splineEnabled)
    {
        cameraFollower.Update(dt);
        camera.camPos = cameraFollower.position;
        camera.camTarget = orbitCenter;
    }
    else if (useSplineCamera && !sceneManager.Active().splineEnabled)
    {
        // scene has locked the camera — apply its override
        camera.camPos = sceneManager.Active().camPos;
        camera.camTarget = sceneManager.Active().camTarget;
    }

    if (lights.directionals.size() >= 2)
    {
        lights.directionals[0] = sky.sun;
        lights.directionals[1] = sky.moon;
    }

    if (!lightColorsStored && !lights.points.empty())
    {
        originalPointLightColors.clear();
        for (const auto& pl : lights.points)
            originalPointLightColors.push_back(pl.color);
        lightColorsStored = true;
    }

    if (lightFadeActive)
    {
        lightFadeTimer += dt;
        float t = clamp(lightFadeTimer / lightFadeDuration, 0.0f, 1.0f);
        float smooth = t * t * (3.0f - 2.0f * t);
        lightFadeMult = lightFadeFrom + (lightFadeTo - lightFadeFrom) * smooth;

        if (t >= 1.0f)
            lightFadeActive = false;

        ResetAccumulator();
    }

    if (lightColorsStored)
    {
        for (int i = 0; i < (int)lights.points.size() && i < (int)originalPointLightColors.size(); i++)
            lights.points[i].color = originalPointLightColors[i] * lightFadeMult;
    }

    const bool cameraMoving =
        length(camera.camPos - prevCamera.camPos) > 1e-4f ||
        length(camera.camTarget - prevCamera.camTarget) > 1e-4f;

    if (cameraMoving) rayTableDirty = true;

    if (!cameraMoving && rayTableDirty)
    {
#pragma omp parallel for schedule(static)
        for (int y = 0; y < SCRHEIGHT; y++)
            for (int x = 0; x < SCRWIDTH; x++)
                rayDirTable[x + y * SCRWIDTH] =
                camera.GetPrimaryRay(static_cast<float>(x) + 0.5f,
                    static_cast<float>(y) + 0.5f).D;
        rayTableDirty = false;
    }

    if (sceneManager.HasActive() && sceneManager.Active().usePhysics)
    {
        physics.Update(dt, scene);

        bool anyBallMoved = !physics.balls.empty();
        for (auto& ball : physics.balls)
            if (ball.visualIndex >= 0 && ball.visualIndex < static_cast<int>(scene.spheres.size()))
                scene.spheres[ball.visualIndex].center = ball.position;

        if (anyBallMoved)
        {
            scene.BuildSphereBVH();
            ResetAccumulator();
        }
    }
    eventSystem.Tick(deltaTime);

    {
        auto& bm = GameScenes::GetBrickmapState();
        if (bm && bm->bloomSpike > 0.01f)
        {
            bloomIntensity = 0.3f + bm->bloomSpike;
            ResetAccumulator();
        }
        else
        {
            bloomIntensity = 0.3f;
        }
    }

    auto& bm = GameScenes::GetBrickmapState();
    if (bm && useSplineCamera && sceneManager.HasActive()
        && strcmp(sceneManager.Active().name, "Brickmap") == 0)
        cameraFollower.speed = bm->camSpeedRequest;

    if (sceneManager.HasActive() && sceneManager.Active().tickCallback)
    {
        sceneManager.Active().tickCallback(
            sceneManager.Active(), scene, deltaTime,
            [this]() { ResetAccumulator(); }
        );
    }

    constexpr int TILE = 16;
    const int tilesX = (SCRWIDTH + TILE - 1) / TILE;
    const int tilesY = (SCRHEIGHT + TILE - 1) / TILE;
    const int totalTiles = tilesX * tilesY;

    if (!scene.voxelInstances.empty())
    {
        memset(history, 0, SCRWIDTH * SCRHEIGHT * sizeof(float3));
        sampleCount = 0;
    }

    const bool hasInstances = !scene.voxelInstances.empty();
    if (hasInstances) sampleCount = 0;

#pragma omp parallel for schedule(dynamic, 1) reduction(+:totalRaysThisFrame)
for (int tileIdx = 0; tileIdx < totalTiles; ++tileIdx)
{
    const int tileRow = tileIdx / tilesX;
    const int tileCol = tileIdx % tilesX;
    const int x0 = tileCol * TILE;
    const int y0 = tileRow * TILE;
    const int x1 = min(x0 + TILE, SCRWIDTH);
    const int y1 = min(y0 + TILE, SCRHEIGHT);
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x)
        {
            const int idx = x + y * SCRWIDTH;
            const bool traceThisPixel = (sampleCount <= 1) ||
                (((x + y) & 1) == (static_cast<int>(frameIndex) & 1));
            if (!traceThisPixel)
            {
                accumulator[idx] = scene.voxelInstances.empty() ? history[idx] : float3(0);
                continue;
            }
            const float jx = BlueNoise(x, y, sampleCount);
            const float jy = BlueNoise(y, x, sampleCount);
            Ray r = (cameraMoving || rayTableDirty)
                ? camera.GetPrimaryRay(x + jx, y + jy)
                : Ray(camera.camPos, rayDirTable[idx]);
            const float3 sample = Trace(r, 0, 0, 0);
            totalRaysThisFrame++;
            float3 blended;

            if (r.instanceIndex >= 0)
            {
                blended = sample;
                sampleCountPerPixel[idx] = 1;
            }
            else if (!cameraMoving)
            {
                blended = sample;
                if (r.voxel != 0)
                    sampleCountPerPixel[idx] = 1;
            }
            else if (r.voxel > 0 && scene.voxelInstances.empty())
            {
                const float3 P = r.O + r.t * r.D;
                float prev_x, prev_y;
                if (prevCamera.WorldToScreen(P, prev_x, prev_y))
                {
                    const int   ix = static_cast<int>(prev_x);
                    const int   iy = static_cast<int>(prev_y);
                    const float fx = prev_x - (float)ix;
                    const float fy = prev_y - (float)iy;
                    if (ix < 0 || ix + 1 >= SCRWIDTH ||
                        iy < 0 || iy + 1 >= SCRHEIGHT)
                    {
                        blended = sample;
                        sampleCountPerPixel[idx] = 1;
                    }
                    else
                    {
                        const float3 h00 = history[ix + iy * SCRWIDTH];
                        const float3 h10 = history[ix + 1 + iy * SCRWIDTH];
                        const float3 h01 = history[ix + (iy + 1) * SCRWIDTH];
                        const float3 h11 = history[ix + 1 + (iy + 1) * SCRWIDTH];
                        const float3 top = lerp(h00, h10, fx);
                        const float3 bot = lerp(h01, h11, fx);
                        float3 historySample = lerp(top, bot, fy);
                        blended = 0.8f * historySample + 0.2f * sample;
                        sampleCountPerPixel[idx] = 6;
                    }
                }
                else
                {
                    blended = sample;
                    sampleCountPerPixel[idx] = 1;
                }
            }
            else
            {
                blended = sample;
                sampleCountPerPixel[idx] = 1;
            }

            accumulator[idx] = blended;
        }
}
    if (enableBloom)
    {
        ApplyBloom();
    }
    else
    {
#pragma omp parallel for schedule(static)
        for (int i = 0; i < SCRWIDTH * SCRHEIGHT; i++)
        {
            float3 c = accumulator[i];
            float3 mapped;
            mapped.x = c.x / (1.0f + c.x);
            mapped.y = c.y / (1.0f + c.y);
            mapped.z = c.z / (1.0f + c.z);
            screen->pixels[i] = RGBF32_to_RGB8(mapped);
        }
    }

    {
        if (fadeOpacity < fadeTarget)
            fadeOpacity = min(fadeOpacity + fadeSpeed * dt, fadeTarget);
        else if (fadeOpacity > fadeTarget)
            fadeOpacity = max(fadeOpacity - fadeSpeed * dt, fadeTarget);

        if (fadeOpacity > 0.001f)
        {
            uint fadeR = (uint)(fadeColor.x * 255.0f);
            uint fadeG = (uint)(fadeColor.y * 255.0f);
            uint fadeB = (uint)(fadeColor.z * 255.0f);

#pragma omp parallel for schedule(static)
            for (int i = 0; i < SCRWIDTH * SCRHEIGHT; i++)
            {
                uint pixel = screen->pixels[i];
                uint r = (pixel >> 16) & 255;
                uint g = (pixel >> 8) & 255;
                uint b = pixel & 255;

                r = (uint)(r * (1.0f - fadeOpacity) + fadeR * fadeOpacity);
                g = (uint)(g * (1.0f - fadeOpacity) + fadeG * fadeOpacity);
                b = (uint)(b * (1.0f - fadeOpacity) + fadeB * fadeOpacity);

                screen->pixels[i] = (r << 16) | (g << 8) | b;
            }
        }
    }

    prevCamera = camera;
    camera.HandleInput(deltaTime);
    swap(history, accumulator);

    auto endTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float> frameDuration = endTime - startTime;
    lastFrameTime = frameDuration.count();
    avgFrameTimeMs = lastFrameTime * 1000.0f;
    fps = 1.0f / lastFrameTime;
    rps = (float)totalRaysThisFrame / (lastFrameTime * 1'000'000.0f);

    if (rebuildSphereBVH)
    {
        scene.BuildSphereBVH();
        rebuildSphereBVH = false;
    }
}


void Renderer::UIStats()
{
    ImGui::Begin("Stats", nullptr, ImGuiWindowFlags_NoCollapse);

    ImGui::Text("Renderer");
    ImGui::Separator();
    ImGui::Text("Voxel: %i",
        camera.GetPinholeRay(static_cast<float>(mousePos.x),
            static_cast<float>(mousePos.y)).voxel);
    ImGui::Text("%.2f ms | %.1f FPS", avgFrameTimeMs, fps);
    ImGui::Text("%.1f Mrays/s", rps);
    ImGui::Text("Ray table: %s", rayTableDirty ? "dirty" : "cached");
    ImGui::Text("Light mult: %.2f", lightFadeMult);

    static float fpsHistory[120] = {};
    static int   offset = 0;
    fpsHistory[offset] = fps;
    offset = (offset + 1) % IM_ARRAYSIZE(fpsHistory);

    ImGui::PlotLines("FPS", fpsHistory, IM_ARRAYSIZE(fpsHistory), offset,
        nullptr, 0.0f, 120.0f, ImVec2(0, 60));

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 graphPos = ImGui::GetItemRectMin();
    ImVec2 graphSize = ImGui::GetItemRectSize();
    float  y60 = graphPos.y + graphSize.y * (1.0f - 60.0f / 120.0f);
    draw_list->AddLine(ImVec2(graphPos.x, y60),
        ImVec2(graphPos.x + graphSize.x, y60),
        IM_COL32(255, 0, 0, 255));

    ImGui::End();
}


static void LightColorDot(float3 c)
{
    ImGui::ColorButton("##dot", ImVec4(c.x, c.y, c.z, 1.0f),
        ImGuiColorEditFlags_NoTooltip |
        ImGuiColorEditFlags_NoPicker |
        ImGuiColorEditFlags_NoBorder,
        ImVec2(10, 10));
    ImGui::SameLine();
}


void Renderer::UI()
{
    ImGui::Begin("Inspector");

    sceneManager.UI(scene, camera, sky, lights, [this]() { ResetAccumulator(); });
    eventSystem.UI([this]() { ResetAccumulator(); });

    UIStats();

    if (ImGui::CollapsingHeader("Camera & Debug"))
    {
        if (ImGui::Checkbox("Show Normals", &debugNormals))
            ResetAccumulator();

        if (ImGui::Checkbox("Use Spline Camera", &useSplineCamera))
            ResetAccumulator();

        ImGui::Spacing();
        ImGui::TextDisabled("Position:  %.3f  %.3f  %.3f",
            camera.camPos.x, camera.camPos.y, camera.camPos.z);
        ImGui::TextDisabled("Target:    %.3f  %.3f  %.3f",
            camera.camTarget.x, camera.camTarget.y, camera.camTarget.z);

        ImGui::Spacing();
        bool camChanged = false;

        if (ImGui::TreeNode("Depth of Field"))
        {
            camChanged |= ImGui::SliderFloat("Aperture", &camera.aperture, 0.0f, 0.5f, "%.4f");
            camChanged |= ImGui::SliderFloat("Blur Factor", &camera.blurFactor, 0.0f, 1.0f);
            camChanged |= ImGui::DragFloat2("Focus Range", &camera.focusRange.x, 0.01f, 0.0f, 100.0f, "%.2f");
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Projection"))
        {
            camChanged |= ImGui::Checkbox("Use Fisheye", &camera.useFisheye);
            camChanged |= ImGui::SliderFloat("HFOV (deg)", &camera.hfov, 30.0f, 160.0f, "%.1f");

            if (!camera.useFisheye)
            {
                camChanged |= ImGui::SliderFloat("Panini d", &camera.panini_d, 0.0f, 1.5f, "%.3f");
                camChanged |= ImGui::SliderFloat("Panini s", &camera.panini_s, 0.0f, 1.0f, "%.3f");
            }
            ImGui::TreePop();
        }

        if (camChanged)
        {
            rayTableDirty = true;
            ResetAccumulator();
        }
    }

    if (ImGui::CollapsingHeader("Post-Processing"))
    {
        bool ppChanged = false;
        ppChanged |= ImGui::Checkbox("Enable Bloom", &enableBloom);
        if (enableBloom)
        {
            ppChanged |= ImGui::SliderFloat("Bloom Threshold", &bloomThreshold, 0.1f, 5.0f, "%.2f");
            ppChanged |= ImGui::SliderFloat("Bloom Intensity", &bloomIntensity, 0.0f, 1.0f, "%.2f");
            ppChanged |= ImGui::SliderInt("Bloom Radius", &bloomRadius, 2, 16);
        }

        ImGui::Spacing();
        ImGui::Text("Screen Fade: %.0f%%", fadeOpacity * 100.0f);
        ImGui::SliderFloat("Fade Opacity", &fadeOpacity, 0.0f, 1.0f);
        ImGui::SliderFloat("Light Mult", &lightFadeMult, 0.0f, 1.0f);

        if (ppChanged) ResetAccumulator();
    }

    if (ImGui::CollapsingHeader("Sky"))
    {
        bool skyChanged = false;

        skyChanged |= ImGui::SliderFloat("Time of Day", &sky.timeOfDay, 0.0f, 1.0f, "%.3f");
        skyChanged |= ImGui::SliderFloat("Cycle Speed", &sky.cycleSpeed, 0.0f, 0.1f, "%.4f");
        skyChanged |= ImGui::Checkbox("Animate", &sky.animate);

        if (ImGui::TreeNode("Sun"))
        {
            skyChanged |= ImGui::ColorEdit3("Noon Color", &sky.sunNoonColor.x);
            skyChanged |= ImGui::ColorEdit3("Horizon Color", &sky.sunHorizonColor.x);
            skyChanged |= ImGui::SliderFloat("Intensity", &sky.sunIntensity, 0.0f, 10.0f);
            ImGui::TreePop();
        }
        if (ImGui::TreeNode("Moon"))
        {
            skyChanged |= ImGui::ColorEdit3("Color", &sky.moonColor.x);
            skyChanged |= ImGui::SliderFloat("Intensity", &sky.moonIntensity, 0.0f, 1.0f);
            ImGui::TreePop();
        }
        if (ImGui::TreeNode("Atmosphere"))
        {
            skyChanged |= ImGui::ColorEdit3("Zenith", &sky.zenithColor.x);
            skyChanged |= ImGui::ColorEdit3("Horizon", &sky.horizonColor.x);
            ImGui::TreePop();
        }

        if (skyChanged) ResetAccumulator();
    }

    if (ImGui::CollapsingHeader("Lights"))
    {
        bool lightsChanged = false;
        int  uid = 0;

        int totalLights = (int)(lights.points.size() + lights.directionals.size() +
            lights.spots.size() + lights.areas.size());
        ImGui::TextDisabled("%d light%s", totalLights, totalLights == 1 ? "" : "s");
        ImGui::Spacing();

        for (size_t i = 0; i < lights.points.size(); i++)
        {
            PointLight& pl = lights.points[i];
            ImGui::PushID(uid++);
            LightColorDot(pl.color);
            char label[64];
            snprintf(label, sizeof(label), "Point %d", (int)i);
            bool open = ImGui::TreeNode(label);
            ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 24);
            if (ImGui::Checkbox("##en", &pl.enabled)) lightsChanged = true;
            if (open)
            {
                ImGui::BeginDisabled(!pl.enabled);
                lightsChanged |= ImGui::DragFloat3("Position", &pl.position.x, 0.01f);
                lightsChanged |= ImGui::ColorEdit3("Color", &pl.color.x);
                ImGui::EndDisabled();
                ImGui::TreePop();
            }
            ImGui::PopID();
        }

        for (size_t i = 0; i < lights.directionals.size(); i++)
        {
            DirectionalLight& dl = lights.directionals[i];
            ImGui::PushID(uid++);
            LightColorDot(dl.color);
            char label[64];
            snprintf(label, sizeof(label), "Directional %d", (int)i);
            bool open = ImGui::TreeNode(label);
            ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 24);
            if (ImGui::Checkbox("##en", &dl.enabled)) lightsChanged = true;
            if (open)
            {
                ImGui::BeginDisabled(!dl.enabled);
                bool dirChanged = ImGui::DragFloat3("Direction", &dl.direction.x, 0.01f);
                if (dirChanged) dl.direction = normalize(dl.direction);
                lightsChanged |= dirChanged;
                lightsChanged |= ImGui::ColorEdit3("Color", &dl.color.x);
                ImGui::EndDisabled();
                ImGui::TreePop();
            }
            ImGui::PopID();
        }

        for (size_t i = 0; i < lights.spots.size(); i++)
        {
            SpotLight& sl = lights.spots[i];
            ImGui::PushID(uid++);
            LightColorDot(sl.color);
            char label[64];
            snprintf(label, sizeof(label), "Spot %d", (int)i);
            bool open = ImGui::TreeNode(label);
            ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 24);
            if (ImGui::Checkbox("##en", &sl.enabled)) lightsChanged = true;
            if (open)
            {
                ImGui::BeginDisabled(!sl.enabled);
                bool slChanged = false;
                slChanged |= ImGui::DragFloat3("Position", &sl.position.x, 0.01f);
                slChanged |= ImGui::DragFloat3("Direction", &sl.direction.x, 0.01f);
                if (slChanged) sl.direction = normalize(sl.direction);
                slChanged |= ImGui::ColorEdit3("Color", &sl.color.x);
                slChanged |= ImGui::DragFloat("Range", &sl.range, 0.1f, 0.1f, 100.0f);
                slChanged |= ImGui::DragFloat("Angle", &sl.spotAngleDeg, 0.1f, 0.1f, 90.0f);
                slChanged |= ImGui::SliderFloat("Edge Softness", &sl.edgeRoughness, 0.0f, 1.0f);
                lightsChanged |= slChanged;
                ImGui::EndDisabled();
                ImGui::TreePop();
            }
            ImGui::PopID();
        }

        for (size_t i = 0; i < lights.areas.size(); i++)
        {
            AreaLight& al = lights.areas[i];
            ImGui::PushID(uid++);
            LightColorDot(al.color);
            char label[64];
            snprintf(label, sizeof(label), "Area %d", (int)i);
            bool open = ImGui::TreeNode(label);
            ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 24);
            if (ImGui::Checkbox("##en", &al.enabled)) lightsChanged = true;
            if (open)
            {
                ImGui::BeginDisabled(!al.enabled);
                lightsChanged |= ImGui::ColorEdit3("Color", &al.color.x);
                lightsChanged |= ImGui::DragFloat3("Corner", &al.corner.x, 0.01f);
                lightsChanged |= ImGui::DragFloat3("Edge 1", &al.edge1.x, 0.01f);
                lightsChanged |= ImGui::DragFloat3("Edge 2", &al.edge2.x, 0.01f);
                ImGui::EndDisabled();
                ImGui::TreePop();
            }
            ImGui::PopID();
        }

        if (lightsChanged) ResetAccumulator();
    }

    if (ImGui::CollapsingHeader("Materials"))
    {
        bool materialsChanged = false;

        if (selectionLocked && selectedMaterialIndex != -1)
        {
            if (MaterialUI("Selected Material", scene.materials[selectedMaterialIndex]))
                materialsChanged = true;
        }
        else
        {
            ImGui::TextDisabled("Left-click a surface to select its material.");
            ImGui::TextDisabled("Right-click to deselect.");
        }

        if (materialsChanged) ResetAccumulator();
    }

    ImGui::End();
}


bool Renderer::MaterialUI(const char* label, Material& material)
{
    ImGui::PushID(label);
    bool changed = false;

    ImGui::ColorButton("##alb", ImVec4(material.albedo.x, material.albedo.y,
        material.albedo.z, 1.0f),
        ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoPicker,
        ImVec2(12, 12));
    ImGui::SameLine();

    if (ImGui::TreeNode(label))
    {
        static const char* TypeLabels[] = { "Lambertian", "Metal", "Dielectric", "Emissive" };
        int type = static_cast<int>(material.type);
        if (ImGui::Combo("Shader", &type, TypeLabels, IM_ARRAYSIZE(TypeLabels)))
        {
            material.type = static_cast<MaterialType>(type);
            changed = true;
        }

        changed |= ImGui::ColorEdit3("Albedo", &material.albedo.x);

        switch (material.type)
        {
        case MaterialType::Lambertian:
            changed |= ImGui::SliderFloat("Roughness", &material.roughness, 0.0f, 1.0f);
            break;
        case MaterialType::Metal:
            changed |= ImGui::SliderFloat("Roughness", &material.roughness, 0.0f, 1.0f);
            changed |= ImGui::SliderFloat("Metallic", &material.metallic, 0.0f, 1.0f);
            break;
        case MaterialType::Dielectric:
            changed |= ImGui::SliderFloat("IOR", &material.ior, 1.0f, 2.5f);
            if (ImGui::SmallButton("Glass")) { material.ior = 1.52f; changed = true; }
            ImGui::SameLine();
            if (ImGui::SmallButton("Water")) { material.ior = 1.33f; changed = true; }
            ImGui::SameLine();
            if (ImGui::SmallButton("Diamond")) { material.ior = 2.42f; changed = true; }
            break;
        case MaterialType::Emissive:
            changed |= ImGui::ColorEdit3("Emission", &material.emission.x);
            changed |= ImGui::SliderFloat("Intensity", &material.emissionStr, 0.0f, 50.0f);
            break;
        }

        ImGui::TreePop();
    }

    ImGui::PopID();
    return changed;
}


void Renderer::Shutdown()
{
    eventSystem.Save("events.bin");
}


void Tmpl8::Renderer::InitAccumulator()
{
    if (!accumulator)
        accumulator = static_cast<float3*>MALLOC64(SCRWIDTH * SCRHEIGHT * sizeof(float3));
    ResetAccumulator();
}


void Tmpl8::Renderer::ResetAccumulator()
{
    memset(accumulator, 0, SCRWIDTH * SCRHEIGHT * sizeof(float3));
    if (history) memset(history, 0, SCRWIDTH * SCRHEIGHT * sizeof(float3));
    memset(sampleCountPerPixel, 0, SCRWIDTH * SCRHEIGHT * sizeof(int));
    sampleCount = 0;
}


void Renderer::MouseDown(int button)
{
    if (button == 0)
    {
        if (!selectionLocked)
        {
            Ray r = camera.GetPinholeRay(static_cast<float>(mousePos.x),
                static_cast<float>(mousePos.y));
            scene.FindNearest(r);
            if (r.materialIndex != -1)
            {
                selectedMaterialIndex = r.materialIndex;
                selectionLocked = true;
            }
        }
    }
    else if (button == 1)
    {
        selectedMaterialIndex = -1;
        selectionLocked = false;
    }
}


void Renderer::KeyDown(int key)
{
    if (key == GLFW_KEY_F1) { sceneManager.LoadScene(0, scene, camera, sky, lights); ResetAccumulator(); rayTableDirty = true; lightColorsStored = false; }
    if (key == GLFW_KEY_F2) { sceneManager.LoadScene(1, scene, camera, sky, lights); ResetAccumulator(); rayTableDirty = true; lightColorsStored = false; }
}