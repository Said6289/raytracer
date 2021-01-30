#include "ray.h"

void
write_ppm(char *filename, uint32_t width, uint32_t height, uint32_t *pixels)
{
    FILE *file = fopen(filename, "w");
    fprintf(file, "P3\n");
    fprintf(file, "%u %u\n", width, height);
    fprintf(file, "255\n");
    for (uint32_t i = 0; i < width*height; ++i) {
        uint8_t r = (pixels[i] >> 16) & 0xFF;
        uint8_t g = (pixels[i] >> 8) & 0xFF;
        uint8_t b = (pixels[i] >> 0) & 0xFF;
        fprintf(file, "%u %u %u\n", r, g, b);
    }
    fclose(file);
}

static void
RaycasterSetFromCamera(raycaster *Raycaster, v2 PixP, camera *Camera)
{
    Raycaster->Origin.x = 0.0f;
    Raycaster->Origin.y = 0.0f;
    Raycaster->Origin.z = 6.0f;

    Raycaster->Direction = normalize(subtract(V3(PixP.x, PixP.y, 5.0f), Raycaster->Origin));
}

static bool
IntersectScene(raycaster *Raycaster, scene *Scene, hit *Hit)
{
    float MinHitD = FLT_MAX;

    bool DidHit = false;

    for (int SphereI = 0; SphereI < Scene->SphereCount; SphereI++) {
        sphere *Sphere = Scene->Spheres + SphereI;

        v3 SphereP = subtract(Raycaster->Origin, Sphere->Center);

        f32 A = dot(Raycaster->Direction, SphereP);
        f32 Discriminant = A * A;
        Discriminant -= dot(SphereP, SphereP) - (Sphere->Radius * Sphere->Radius);

        if (Discriminant >= 0) {
            Discriminant = sqrt(Discriminant);

            f32 d1 = -A + Discriminant;
            f32 d2 = -A - Discriminant;

            f32 d = d1;
            if (d2 < d) {
                d = d2;
            }

            if (d > 0 && d < MinHitD) {
                MinHitD = d;
                DidHit = true;

                Hit->Material = Sphere->Material;
                Hit->Point = add(Raycaster->Origin, scale(Raycaster->Direction, d));
                Hit->Normal = normalize(subtract(Hit->Point, Sphere->Center));
            }
        }
    }

    for (int PlaneI = 0; PlaneI < Scene->PlaneCount; PlaneI++) {
        plane *Plane = Scene->Planes + PlaneI;

        f32 Denominator = dot(Raycaster->Direction, Plane->Normal);
        if (fabs(Denominator) > 1e-6f) {

            v3 PlaneP = scale(Plane->Normal, Plane->D);
            f32 d = dot(subtract(PlaneP, Raycaster->Origin), Plane->Normal);
            d /= Denominator;

            if (d > 0 && d < MinHitD) {
                MinHitD = d;
                DidHit = true;

                Hit->Point = add(Raycaster->Origin, scale(Raycaster->Direction, d));
                Hit->Normal = Plane->Normal;
                Hit->Material = Plane->Material;
            }
        }
    }

    return DidHit;
}

static v3
GammaToLinear(v3 C)
{
    return C;
}

static v3
LinearToGamma(v3 C)
{
    return C;
}

static v3
Reflect(v3 Incident, v3 Normal)
{
    v3 Result = add(Incident, scale(Normal, 2.0f * dot(Incident, Normal)));
    return Result;
}

static bool
IsLitBy(v3 P, v3 Dir, float LightD, raycaster *Raycaster, scene *Scene)
{
    hit Hit = {};

    Raycaster->Origin = P;
    Raycaster->Direction = Dir;
    bool FoundIntersection = IntersectScene(Raycaster, Scene, &Hit);

    bool HitLight = false;
    if (!FoundIntersection) {
        HitLight = true;
    } else if (length(subtract(Hit.Point, P)) >= LightD) {
        HitLight = true;
    }
    return HitLight;
}

static void
OutputXY(image_data *ImageData, int X, int Y, v3 PixC)
{
    ImageData->Pixels[Y * ImageData->Width + X] = color_to_u32(PixC);
}

static void
RaytraceTile(
    image_data *ImageData,
    camera *Camera,
    scene *Scene,
    int LightCount,
    light *Lights,
    float StartX, float StartY,
    int TileW, int TileH,
    int SuperSamplingRate,
    int ReflectionCount,
    bool UseMirrors,
    bool CalcDiffuse,
    bool CalcPhong,
    bool Shadows
) {
    v3 DefaultC = V3(0, 0, 0);

    raycaster Raycaster = {};
    ray NextRay = {};
    regs Regs = {};

    int HitCount = 0;
    hit Hit = {};

    uint32_t FilmW = ImageData->Width;
    uint32_t FilmH = ImageData->Height;

    float PixW = 2.0f / (float)FilmW;
    float PixH = 2.0f / (float)FilmH;

    int RaysPerAxis = 1 << SuperSamplingRate;
    int RaysPerPixel = RaysPerAxis * RaysPerAxis;

    float SampleW = PixW / RaysPerAxis;
    float SampleH = PixH / RaysPerAxis;

    for(int Y = StartY; Y < StartY + TileH; ++Y) {
        for(int X = StartX; X < StartX + TileW; ++X) {
            v3 PixC = V3(0, 0, 0);

            for (int Sample = 0; Sample < RaysPerPixel; Sample++) {
                v3 SampleC = V3(0, 0, 0);

                float OffsetX = ((float)(Sample % RaysPerAxis) + 0.5f) * SampleW;
                float OffsetY = ((float)(Sample / RaysPerAxis) + 0.5f) * SampleH;

                float PixCenterX = (2.0f * X / FilmW) - 1.0f;
                float PixCenterY = -((2.0f * Y / FilmH) - 1.0f);

                float PixX = PixCenterX + OffsetX;
                float PixY = PixCenterY + OffsetY;

                RaycasterSetFromCamera(&Raycaster, V2(PixX, PixY), Camera);

                NextRay.Origin = Raycaster.Origin;
                NextRay.Direction = Raycaster.Direction;

                float MirrorAttenuation = 1.0f;

                for (int Bounce = 0; Bounce < ReflectionCount + 1; Bounce++) {
                    Raycaster.Origin = NextRay.Origin;
                    Raycaster.Direction = NextRay.Direction;

                    v3 FinalC = {};

                    bool DidHit = IntersectScene(&Raycaster, Scene, &Hit);

                    if (DidHit) {
                        material *Mat = Hit.Material;

                        if (Mat) {
                            v3 Normal = Hit.Normal;

                            v3 MatC = GammaToLinear(Mat->Color);

                            if (Mat->Mirror) {
                                NextRay.Origin = Hit.Point;
                                NextRay.Direction = Reflect(Raycaster.Direction, Normal);
                            }

                            v3 ViewRay = scale(Raycaster.Direction, -1.0f);

                            for (int LightI = 0; LightI < LightCount; ++LightI) {
                                light *Light = Lights + LightI;

                                v3 LightC = GammaToLinear(Light->Color);
                                v3 LightP = Light->Position;
                                v3 LightRay = subtract(LightP, Hit.Point);
                                v3 LightRayN = normalize(LightRay);
                                float LightD = length(LightRay);

                                float Intensity = Light->Intensity;
                                Intensity /= LightD * LightD;

                                bool HitLight = IsLitBy(Hit.Point, LightRayN, LightD, &Raycaster, Scene);
                                if (!Shadows || HitLight) {
                                    v3 RefRay = normalize(Reflect(scale(LightRay, -1.0f), Normal));

                                    float Lambert = Clamp(0, dot(LightRayN, Normal), 1);
                                    float Specular = Clamp(0, dot(RefRay, ViewRay), 1);
                                    Specular = powf(Specular, Mat->Shininess);

                                    v3 LambertC = MatC;
                                    if (CalcDiffuse) {
                                        LambertC = scale(LambertC, Lambert);
                                    }
                                    v3 SpecularC = scale(V3(1, 1, 1), Specular);

                                    v3 PhongC = LambertC;
                                    // TODO(said): Finish specular lighting

                                    v3 C = hadamard(LightC, PhongC);

                                    if (CalcPhong || CalcDiffuse) {
                                        C = scale(C, Intensity);
                                    }

                                    FinalC = add(FinalC, C);
                                }
                            }

                            FinalC = scale(FinalC, MirrorAttenuation);

                            if (!UseMirrors || !Mat->Mirror) {
                                SampleC = add(SampleC, FinalC);
                                break; // bounce loop
                            } else {
                                SampleC = add(SampleC, scale(FinalC, Mat->Reflectivity));
                                MirrorAttenuation *= 1.0f - Mat->Reflectivity;
                            }
                        }
                    } else {
                        FinalC = scale(DefaultC, MirrorAttenuation);
                        SampleC = add(SampleC, FinalC);
                        break;
                    }
                }

                PixC = add(PixC, scale(SampleC, 1.0f / RaysPerPixel));
            }

            PixC = LinearToGamma(PixC);
            OutputXY(ImageData, X, Y, PixC);
        }
    }
}

int
main(int ArgCount, char **Args)
{
    material Materials[] = {
        {{1.0f, 0.5f, 0.0f}, false, 0.0f, 0.0f},
        {{1.0f, 1.0f, 1.0f}, false, 0.0f, 0.0f},
    };

    sphere Spheres[] = {
        {V3( 0.0f, 1.0f, 0.0f), 1.0f, &Materials[0]},
        {V3(-3.0f, 1.0f, 0.0f), 1.0f, &Materials[0]},
        {V3( 3.0f, 1.0f, 0.0f), 1.0f, &Materials[0]},
    };

    plane Planes[] = {
        {-1.0f, V3(0.0f, 1.0f, 0.0f), &Materials[1]},
    };

    light Light0 = {};
    Light0.Position = V3(3, 3, 1);
    Light0.Color = V3(0, 1, 0);
    Light0.Intensity = 3.0f;

    light Light1 = {};
    Light1.Position = V3(-3, 0.1f, 3);
    Light1.Color = V3(1, 0, 1);
    Light1.Intensity = 1.0f;

    light Light2 = {};
    Light2.Position = V3(3, 0.1f, 3);
    Light2.Color = V3(1, 1, 1);
    Light2.Intensity = 1.0f;

    light Light3 = {};
    Light3.Position = V3(0, 0.5f, 3);
    Light3.Color = V3(1, 0, 0);
    Light3.Intensity = 2.0f;

    light Lights[4];
    Lights[0] = Light0;
    Lights[1] = Light1;
    Lights[2] = Light2;
    Lights[3] = Light3;

    camera Camera = {};
    scene Scene = {};

    Scene.SphereCount = ArrayCount(Spheres);
    Scene.Spheres = Spheres;

    Scene.PlaneCount = ArrayCount(Planes);
    Scene.Planes = Planes;

    image_data ImageData = {};
    ImageData.Width = 512;
    ImageData.Height = 512;
    ImageData.Pixels = malloc(ImageData.Width * ImageData.Height * sizeof(uint32_t));

    RaytraceTile(
            &ImageData,
            &Camera,
            &Scene,
            ArrayCount(Lights),
            Lights,
            0, 0,
            ImageData.Width, ImageData.Height,
            2,
            0,
            true,
            true,
            true,
            true);

    write_ppm("out.ppm", ImageData.Width, ImageData.Height, ImageData.Pixels);
}
