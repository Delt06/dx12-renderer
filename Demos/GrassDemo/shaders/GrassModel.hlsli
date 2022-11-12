#ifndef GRASS_MODEL_HLSLI
#define GRASS_MODEL_HLSLI

struct GrassModel
{
    matrix Model;
    float _Padding[16 * 3];
};

#endif
