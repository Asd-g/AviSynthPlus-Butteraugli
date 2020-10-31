# Description

A quality metric for lossy image and video compression.

This is [a port of the VapourSynth plugin butteraugli](https://github.com/fdar0536/VapourSynth-butteraugli).

[Google's Butteraugli](https://github.com/google/butteraugli) is used.

AviSynth+ >=3.6 required in order to use this filter.

# Usage

```
Butteraugli (clip clip1, clip clip2, bool "heatmap")
```

## Parameters:

- clip1, clip2\
    Clips that are use for estimating the psychovisual similarity. They must be in RGB 8-bit planar format.
    
- heatmap\
    True: A heatmap is returned containing differences between two input clips.\
    False: Returns clip2.\
    Default: True.
    
    
The psychovisual similarity of the clips will be stored as frame property '_FrameButteraugli' in the output clip. Larger values indicate to bigger difference. 

# License

The used butteraugli library is licensed under Apache-2.0 license.

This project and binaries are licensed under the GPLv3 license.
