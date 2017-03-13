# VapourSynth-dpid
Copyright© 2017 WolframRhodium

Rapid, Detail-Preserving Image Downscaler for VapourSynth
## Description
[dpid](http://www.gcc.tu-darmstadt.de/home/proj/dpid/) is an algorithm which preserves visually important details in downscaled images and is especially suited for large downscaling factors.

It acts like a convolutional filters where input pixels contribute more to the output image the more their color deviates from their local neighborhood, which preserves visually important details.

## Requirements
CUDA-Enabled GPU

## Supported Formats

sample type & bps: RGB 8-16 bit integer

## Usage

```python
dpid.Dpid(clip clip[, int width=0, int height=0, float lambda=1.0])
```

- input:
    The input clip, must be of RGB color family and integer sample type.

- width & height: (Default: 0)
    The width and height of output clip. One of which can be 0, and the downscaling will keep the aspect ratio.

- lambda: (Default: 1.0)
    Power factor of range kernel. It can be used to tune the amplification of the weights of pixels that represent detail—from a box filter over an emphasis of distinct pixels towards a selection of only the most distinct pixels. This parameter happens to be a python keyword, so you may need to refer to the [doc](http://www.vapoursynth.com/doc/pythonreference.html#python-keywords-as-filter-arguments).

## Benchmark

Configuration: Intel Core i7-6700HQ, NVIDIA Geforce GTX 960M, 16GB DDR4-2133 MHz, Windows 10 64bit

Testing 512 frames from 1920x1080 to 1280x720, lambda=1.0...
Output 512 frames in 28.03 seconds (18.27 fps)

Testing 512 frames from 1920x1080 to 400x225, lambda=1.0...
Output 512 frames in 9.54 seconds (53.68 fps)

(Usage is under 40% when downscale to 400x225)
