## Description

A quality metric for lossy image and video compression.

This is [a port of the VapourSynth plugin butteraugli](https://github.com/fdar0536/VapourSynth-butteraugli).

[libjxl's Butteraugli](https://github.com/libjxl/libjxl) is used.

### Requirements:

- AviSynth+ 3.6 or later

- Microsoft VisualC++ Redistributable Package 2022 (can be downloaded from [here](https://github.com/abbodi1406/vcredist/releases))

### Usage:

```
Butteraugli (clip reference, clip distorted, bool "distmap", float "intensity_target", bool "linput")
```

### Parameters:

- reference, distorted\
    Clips that are used for estimating the psychovisual similarity.\
    They must be in RGB 8/16/32-bit planar format and must have same dimensions.
        
- distmap\
    Whether to return heatmap instead of distorted clip\
    Default: False.
    
- intensity_target\
    Viewing conditions screen nits.\
    Default: 80.0.
    
- linput\
    True: The input clips must have linear transfer functions.\
    False: The input clips are assumed in sRGB color space and internally converted to linear transfer.\
    Default: False.
    
    
The psychovisual similarity of the clips will be stored as frame property '_FrameButteraugli' in the output clip. Larger values indicate to bigger difference. 

### Building:

```
Requirements:
    - Git
    - GCC C++17 compiler
    - CMake >= 3.16
    - Ninja
```
```git clone --recurse-submodules https://github.com/Asd-g/AviSynthPlus-Butteraugli```

```cd AviSynthPlus-Butteraugli\libjxl```

```cmake -B build -DCMAKE_INSTALL_PREFIX=jxl_install -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DJPEGXL_ENABLE_FUZZERS=OFF -DJPEGXL_ENABLE_TOOLS=OFF -DJPEGXL_ENABLE_MANPAGES=OFF -DJPEGXL_ENABLE_BENCHMARK=OFF -DJPEGXL_ENABLE_EXAMPLES=OFF -DJPEGXL_ENABLE_JNI=OFF -DJPEGXL_ENABLE_OPENEXR=OFF -DJPEGXL_ENABLE_TCMALLOC=OFF -DBUILD_SHARED_LIBS=OFF -G Ninja```

```cmake --build build```

```cmake --install build```

```cd ..```

```cmake -B build -DCMAKE_BUILD_TYPE=Release -G Ninja```

```cmake --build build```
