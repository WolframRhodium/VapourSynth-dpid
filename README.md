# VapourSynth-dpid
Copyright© 2019 WolframRhodium

Rapid, Detail-Preserving Image Downscaler for VapourSynth

## Description

[dpid](http://www.gcc.tu-darmstadt.de/home/proj/dpid/) is an algorithm that preserves visually important details in downscaled images and is especially suited for large downscaling factors.

It acts like a convolutional filter where input pixels contribute more to the output image the more their color deviates from their local neighborhood, which preserves visually important details.

__[CuPy version](https://github.com/WolframRhodium/muvsfunc/blob/master/Collections/examples/Dpid_cupy/dpid_cupy.vpy) (with additional  floating point support)__

## Supported Formats

sample type & bps: RGB/YUV/GRAY 8-16 bit integer

## Usage

```python
dpid.Dpid(clip clip[, int width=0, int height=0, float lambda=1.0, float[] src_left=0, float[] src_top=0, bool read_chromaloc=True])
```

- clip:
    The input clip.

- width & height: (Default: clip.width / clip.height)
    The width and height of the output clip. One of which can be 0, and the downscaling will keep the aspect ratio.

- lambda: (Default: 1.0)
    The power factor of range kernel. It can be used to tune the amplification of the weights of pixels that represent detail—from a box filter over an emphasis of distinct pixels towards a selection of only the most distinct pixels. This parameter happens to be a python keyword, so you may need to refer to the [doc](http://www.vapoursynth.com/doc/pythonreference.html#python-keywords-as-filter-arguments).

- src_left, src_top: (Default: 0)
    Used to select the source region of the input to use. It an also be used to shift the image. Only the first value in the array is passed to `resize.Bilinear()`.

- read_chromaloc: (Default: True)
    Whether to read `_ChromaLocation` property.

---

```python
dpid.DpidRaw(clip clip[, clip clip2, float lambda=1.0, float[] src_left=0, float[] src_top=0, bool read_chromaloc=True, int[] planes=[0, 1, 2]])
```

- clip:
    (Same as `dpid.Dpid()`)

- clip2:
    User-defined downsampled clip. Should be of the same format and number of frames as `clip`.

- lambda: (Default: 1.0)
    (Same as `dpid.Dpid()`)

- src_left, src_top: (Default: 0)
    Used to select the source region of the input to use. It can also be used to shift the image.

- read_chromaloc: (Default: True)
    (Same as `dpid.Dpid()`)

- planes: (Default: [0, 1, 2])
    Sets which planes will be processed. Any unprocessed planes will be simply copied from `clip2`.
