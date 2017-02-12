# VapourSynth-dpid
Copyright© 2017 WolframRhodium
Rapid, Detail-Preserving Image Downscaler for VapourSynth
## Description
[dpid](http://www.gcc.tu-darmstadt.de/home/proj/dpid/) is an algorithm which preserves visually important details in downscaled images and is especially suited for large downscaling factors.

It acts like a convolutional filters where input pixels contribute more to the output image the more their color deviates from their local neighborhood, which preserves visually important details.

## Requirements
CUDA (and NVIDA GPU with at least Kepler Architecture to enable compute capabilities 3.5)

## Supported Formats

sample type & bps: RGB 8 bit integer

## Usage

```python
dpid.Dpid(clip clip[, int width=128, int height=128, float lambda=1.0])
```

- input:
    The input clip, must be of RGB color family and 8 bit integer.

- width & height:
    The width and height of output clip. One of them can be 0, and the downscaling will keep the aspect ratio.

- lambda:
    Power factor of range kernel. It can be used to tune the amplification of the weights of pixels that represent detail—from a box filter over an emphasis of distinct pixels towards a selection of only the most distinct pixels.