//Copyright (c) 2017 WolframRhodium. All rights reserved. 
#include <cmath>
#include <string>

#include <vapoursynth\VapourSynth.h>
#include <vapoursynth\VSHelper.h>

struct DpidData {
    VSNodeRef * node;
    VSVideoInfo vi;
	float lambda;
    uint8_t * srcInterleaved, * dstInterleaved;
};

struct Params {
	uint32_t oWidth;
	uint32_t oHeight;
	uint32_t iWidth;
	uint32_t iHeight;
	float pWidth;
	float pHeight;
	float lambda;
};

void run(const Params& i, const void* hInput, void* hOutput);

static void process(const VSFrameRef * src, VSFrameRef * dst, DpidData * VS_RESTRICT d, const VSAPI * vsapi) {
    const unsigned width = vsapi->getFrameWidth(src, 0);
    const unsigned height = vsapi->getFrameHeight(src, 0);
    const unsigned srcStride = vsapi->getStride(src, 0) / sizeof(uint8_t);
    const unsigned dstStride = vsapi->getStride(dst, 0) / sizeof(uint8_t);
    const uint8_t * srcpR = reinterpret_cast<const uint8_t *>(vsapi->getReadPtr(src, 0));
    const uint8_t * srcpG = reinterpret_cast<const uint8_t *>(vsapi->getReadPtr(src, 1));
    const uint8_t * srcpB = reinterpret_cast<const uint8_t *>(vsapi->getReadPtr(src, 2));
	uint8_t * VS_RESTRICT dstpR = reinterpret_cast<uint8_t *>(vsapi->getWritePtr(dst, 0));
	uint8_t * VS_RESTRICT dstpG = reinterpret_cast<uint8_t *>(vsapi->getWritePtr(dst, 1));
	uint8_t * VS_RESTRICT dstpB = reinterpret_cast<uint8_t *>(vsapi->getWritePtr(dst, 2));

	Params args;
	args.iWidth = width;
	args.iHeight = height;
	args.oWidth = d->vi.width;
	args.oHeight = d->vi.height;
	args.pWidth = args.iWidth / (float)args.oWidth;
	args.pHeight = args.iHeight / (float)args.oHeight;
	args.lambda = d->lambda;

    for (unsigned y = 0; y < height; y++) {
        for (unsigned x = 0; x < width; x++) {
            const unsigned pos = (width * y + x) * 3;
            d->srcInterleaved[pos] = srcpR[x];
            d->srcInterleaved[pos + 1] = srcpG[x];
            d->srcInterleaved[pos + 2] = srcpB[x];
        }

        srcpR += srcStride;
        srcpG += srcStride;
        srcpB += srcStride;
    }

	run(args, d->srcInterleaved, d->dstInterleaved);

    for (int y = 0; y < d->vi.height; y++) {
        for (int x = 0; x < d->vi.width; x++) {
            const int pos = (d->vi.width * y + x) * 3;
            dstpR[x] = d->dstInterleaved[pos];
            dstpG[x] = d->dstInterleaved[pos + 1];
            dstpB[x] = d->dstInterleaved[pos + 2];
        }

        dstpR += dstStride;
        dstpG += dstStride;
        dstpB += dstStride;
    }
}

static void VS_CC DpidInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi) {
    DpidData * d = static_cast<DpidData *>(*instanceData);
    vsapi->setVideoInfo(&d->vi, 1, node);
}

static const VSFrameRef *VS_CC DpidGetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
    DpidData * d = static_cast<DpidData *>(*instanceData);

    if (activationReason == arInitial) {
        vsapi->requestFrameFilter(n, d->node, frameCtx);
    } else if (activationReason == arAllFramesReady) {
        const VSFrameRef * src = vsapi->getFrameFilter(n, d->node, frameCtx);
        VSFrameRef * dst = vsapi->newVideoFrame(d->vi.format, d->vi.width, d->vi.height, src, core);

        process(src, dst, d, vsapi);

        vsapi->freeFrame(src);
        return dst;
    }

    return nullptr;
}

static void VS_CC DpidFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
    DpidData * d = static_cast<DpidData *>(instanceData);

    vsapi->freeNode(d->node);

    delete[] d->srcInterleaved;
    delete[] d->dstInterleaved;

    delete d;
}

static void VS_CC DpidCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
    DpidData d{};
    int err;

    d.node = vsapi->propGetNode(in, "clip", 0, nullptr);
    d.vi = *vsapi->getVideoInfo(d.node);

    try {
		if (!isConstantFormat(&d.vi) || d.vi.format->sampleType != stInteger || d.vi.format->bitsPerSample != 8 || d.vi.format->colorFamily != cmRGB)
			throw std::string{ "only constant format 8-bit integer RGB input supported" };

		d.vi.width = int64ToIntS(vsapi->propGetInt(in, "width", 0, &err));

		if (err)
			d.vi.width = 128;

		d.vi.height = int64ToIntS(vsapi->propGetInt(in, "height", 0, &err));

		if (err)
			d.vi.height = 128;

		d.lambda = (float)vsapi->propGetFloat(in, "lambda", 0, &err);

		if (err)
			d.lambda = 1.0f;

		d.srcInterleaved = new (std::nothrow) uint8_t[vsapi->getVideoInfo(d.node)->width * vsapi->getVideoInfo(d.node)->height * 3];
		d.dstInterleaved = new (std::nothrow) uint8_t[d.vi.width * d.vi.height * 3];
		if (!d.srcInterleaved || !d.dstInterleaved)
			throw std::string{ "malloc failure (srcInterleaved/dstInterleaved)" };

    } catch (std::string & error) {
        vsapi->setError(out, ("Dpid: " + error).c_str());
        vsapi->freeNode(d.node);
        return;
    }

    DpidData * data = new DpidData{ d };

    vsapi->createFilter(in, out, "Dpid", DpidInit, DpidGetFrame, DpidFree, fmParallelRequests, 0, data, core);
}

//////////////////////////////////////////
// Init

VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin) {
    configFunc("com.wolframrhodium.dpid", "dpid", "Rapid, Detail-Preserving Image Downscaling", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("Dpid", "clip:clip;width:int:opt;height:int:opt;lamda:float:opt",
                 DpidCreate, nullptr, plugin);
}
