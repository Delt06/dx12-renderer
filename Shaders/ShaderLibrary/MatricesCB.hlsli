#ifndef MATRICES_CB_HLSLI
#define MATRICES_CB_HLSLI

struct Matrices
{
    matrix Model;
    matrix ModelView;
    matrix InverseTransposeModelView;
    matrix ModelViewProjection;
    matrix View;
    matrix Projection;
    matrix InverseTransposeModel;
    float4 CameraPosition;
    matrix InverseProjection;
    matrix InverseView;
};

#endif