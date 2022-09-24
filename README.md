# DX12 Renderer

## Demos

### Deferred

- [DeferredLightingDemo.h](./DX12Renderer/DeferredLightingDemo/DeferredLightingDemo.h)
    - Deferred rendering pipeline:
        - Directional, Point, Spot, and Capsule lights;
        - Light passes are implemented using the stencil buffer;
    - PBR:
        - Cook-Torrance BRDF model;
        - Image-Based Lighting:
            - Diffuse (Irradiance Map) and Specular (Pre-Filtered Environment Map);
            - Both maps are computed on application start based on an HDR skybox cubemap;
    - HDR pipeline:
        - Render targets supporting HDR (`R16G16B16A16_FLOAT`);
        - Auto exposure via a Luminance Histogram implemented in Compute shaders;
        - Tone mapping;
    - TAA;
    - SSAO;
    - SSLR.

![Deferred Lighting Demo Screenshot](./Screenshots/DeferredLightingDemo.jpg)

| SSAO Off | SSAO On |
|----------|---------|
|![SSAO Off](./Screenshots/DeferredLightingDemo_SSAO_Off.jpg)|![SSAO On](./Screenshots/DeferredLightingDemo_SSAO_On.jpg)|

| SSLR Off | SSLR On |
|----------|---------|
|![SSLR Off](./Screenshots/DeferredLightingDemo_SSLR_Off.jpg)|![SSLR On](./Screenshots/DeferredLightingDemo_SSLR_On.jpg)|

#### Controls

- Use <kbd>WASD</kbd>/Arrow Keys to move the camera;
- Hold <kbd>LMB</kbd> and move the mouse to orient the camera;
- Press <kbd>L</kbd> to toggle light animation;
- Press <kbd>T</kbd> to toggle TAA;
- Press <kbd>O</kbd> to toggle SSAO;
- Press <kbd>P</kbd> to toggle SSLR.

### Forward

- [LightingDemo.h](DX12Renderer/src/LightingDemo/LightingDemo.h)
    - Models loaded from OBJ files;
    - .DDS textures loading:
        - Textures for this demo are stored in `BC7_UNORM`.
    - Mipmapping;
    - Phong lighting with a directional light and several point and spot lights;
        - Using diffuse, normal, gloss, and specular maps;
    - Particle system with CPU simulation (10000+ particles on screen) and instanced rendering;
    - Shadow mapping for the directional, point, and spot lights;
    - Soft shadows using 16x Poisson Sampling and Early Bail;
    - Dynamic environment reflections - see the sphere;
    - Post-processing:
        - Screen-space fog;
        - Bloom.

![Lighting Demo Screenshot](./Screenshots/LightingDemo.jpg)

![Lighting Demo Screenshot 2](./Screenshots/LightingDemo2.jpg)

#### Controls

- Use <kbd>WASD</kbd>/Arrow Keys to move the camera;
- Hold <kbd>LMB</kbd> and move the mouse to orient the camera;
- Press <kbd>L</kbd> to toggle light animation.

### Animations

- [AnimationsDemo.h](./DX12Renderer/AnimationsDemo/AnimationsDemo.h)
    - Model and animations loaded from FBX files;
    - Skinning in the vertex shader;
    - Animation state blending;
    - Animation state merging (i.e., avatar masks).

![Animations Demo GIF](./Screenshots/AnimationsDemo.gif)

#### Controls

- Use <kbd>WASD</kbd>/Arrow Keys to move the camera;
- Hold <kbd>LMB</kbd> and move the mouse to orient the camera.

### Requirements

Packages (e.g., installed via `vcpkg`):
- [assimp](https://github.com/assimp/assimp)
- [DirectXTex](https://github.com/microsoft/DirectXTex)
- [DirectXMesh](https://github.com/microsoft/DirectXMesh)

## Sources

- https://www.3dgep.com/learning-directx-12-1/
- https://www.3dgep.com/learning-directx-12-2/
- https://www.3dgep.com/learning-directx-12-3/
- https://www.3dgep.com/learning-directx-12-4/
- https://wiki.ogre3d.org/tiki-index.php?page=-Point+Light+Attenuation
- https://github.com/d3dcoder/d3d12book
- http://www.opengl-tutorial.org/ru/intermediate-tutorials/tutorial-16-shadow-mapping/
- https://learnopengl.com/Advanced-Lighting/Shadows/Point-Shadows
- https://catlikecoding.com/unity/tutorials/advanced-rendering/bloom/
- https://learnopengl.com/Advanced-Lighting/HDR
- https://www.alextardif.com/HistogramLuminance.html
- https://bruop.github.io/exposure/
- https://bruop.github.io/tonemapping/
- https://learnopengl.com/Advanced-Lighting/SSAO
- https://sugulee.wordpress.com/2021/06/21/temporal-anti-aliasingtaa-tutorial/

### Libraries and Tools

- https://github.com/microsoft/DirectXTex
- https://github.com/microsoft/DirectXMesh
- https://github.com/assimp/assimp
- https://github.com/Microsoft/DirectXTex/wiki/Texconv
- https://matheowis.github.io/HDRI-to-CubeMap/

### Assets

- https://casual-effects.com/data/
  - Teapot, Cube, Lat-Long Sphere
- https://ambientcg.com/view?id=PavingStones070
- https://ambientcg.com/view?id=Moss002
- https://ambientcg.com/view?id=Metal036
- https://www.mixamo.com/
- https://sketchfab.com/3d-models/old-wooden-chest-45f93c78e5174036801bfb535c139ac7
- https://d1ver.artstation.com/projects/3k2
- https://ambientcg.com/view?id=Ground047
- https://www.cgtrader.com/free-3d-models/electronics/video/retro-television-set-c4dbe4af-960e-4a5c-ac25-2265d6a97cf6
