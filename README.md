# Description

A quality metric for lossy image and video compression.

This is [a port of the VapourSynth plugin butteraugli](https://github.com/fdar0536/VapourSynth-butteraugli).

[Google's Butteraugli](https://github.com/google/butteraugli) is used.

AviSynth+ >=3.6 required in order to use this filter.

# Usage

```
Butteraugli (clip clip1, clip clip2, float "hf_asymmetry", bool "heatmap", bool "linput")
```

## Parameters:

- clip1, clip2\
    Clips that are used for estimating the psychovisual similarity. They must be in RGB planar format and must have same dimensions.
        
- hf_asymmetry\
    Multiplier for penalizing new HF artifacts more than blurring away features. 1.0=neutral.\
    Must be greater than 0.0.\
    Default: 1.0.
    
- heatmap\
    True: A heatmap is returned containing differences between two input clips.\
    False: Returns clip2.\
    Default: True.
    
- linput\
    True: The input clips must have linear transfer functions.\
    False: The input clips are assumed in sRGB color space and internal conversion to linear transfer function is done.\
    Default: False.
    
    
The psychovisual similarity of the clips will be stored as frame property '_FrameButteraugli' in the output clip. Larger values indicate to bigger difference. 

# License

The used butteraugli library is licensed under Apache-2.0 license.

This project and binaries are licensed under the GPLv3 license.
