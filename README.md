# VapourSynth-dpid
Copyright© 2017 WolframRhodium

Rapid, Detail-Preserving Image Downscaler for VapourSynth
## Description
[dpid](http://www.gcc.tu-darmstadt.de/home/proj/dpid/) is an algorithm which preserves visually important details in downscaled images and is especially suited for large downscaling factors.

It acts like a convolutional filters where input pixels contribute more to the output image the more their color deviates from their local neighborhood, which preserves visually important details.

## Requirements
CUDA (and NVIDA GPU with at least Kepler Architecture to enable compute capabilities 3.0)

## Supported Formats

sample type & bps: RGB 8 bit integer

## Usage

```python
dpid.Dpid(clip clip[, int width=128, int height=128, float lambda=1.0])
```

- input:
    The input clip, must be of RGB color family and 8 bit integer.

- width & height:
    The width and height of output clip.

- lambda:
    Power factor of range kernel. It can be used to tune the amplification of the weights of pixels that represent detail—from a box filter over an emphasis of distinct pixels towards a selection of only the most distinct pixels.
<<<<<<< HEAD

## Benchmark

Configuration: Intel Core i7-6700HQ, NVIDIA Geforce GTX 960M, 16GB DDR4-2133 MHz, Windows 10 64bit

Testing 512 frames from 1920x1080 to 1280x720, lambda=0.0...
Output 512 frames in 28.74 seconds (17.81 fps)
--------
Testing 512 frames from 1920x1080 to 1280x720, lambda=0.5...
Output 512 frames in 27.98 seconds (18.30 fps)
--------
Testing 512 frames from 1920x1080 to 1280x720, lambda=1.0...
Output 512 frames in 28.03 seconds (18.27 fps)
--------
Testing 512 frames from 1920x1080 to 400x225, lambda=0.0...
Output 512 frames in 9.43 seconds (54.32 fps)
--------
Testing 512 frames from 1920x1080 to 400x225, lambda=0.5...
Output 512 frames in 9.42 seconds (54.36 fps)
--------
Testing 512 frames from 1920x1080 to 400x225, lambda=1.0...
Output 512 frames in 9.54 seconds (53.68 fps)

(Usage is under 40% when downscale to 400x225)
=======
>>>>>>> origin/master
