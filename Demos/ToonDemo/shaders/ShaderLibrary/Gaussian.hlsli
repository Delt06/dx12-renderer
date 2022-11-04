#ifndef GAUSSIAN_HLSLI
#define GAUSSIAN_HLSLI

const static uint GaussianFilterSize = 21;
const static int GaussianIndexOffset = GaussianFilterSize / 2;

const static float GaussianCoefficients[GaussianFilterSize] =
{
    0.000272337, 0.00089296, 0.002583865, 0.00659813, 0.014869116,
    0.029570767, 0.051898313, 0.080381679, 0.109868729, 0.132526984,
    0.14107424,
    0.132526984, 0.109868729, 0.080381679, 0.051898313, 0.029570767,
    0.014869116, 0.00659813, 0.002583865, 0.00089296, 0.000272337
};

#endif
