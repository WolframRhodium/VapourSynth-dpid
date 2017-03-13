//Copyright (c) 2017 WolframRhodium. All rights reserved. 
#include <cmath>
#include <string>

#include <vapoursynth\VapourSynth.h>
#include <vapoursynth\VSHelper.h>

struct DpidData {
	VSNodeRef * node;
	VSVideoInfo vi;
	float lambda;
	uint8_t * srcInterleaved, *dstInterleaved;
	uint16_t * src16Interleaved, *dst16Interleaved;
};

struct Params {
	uint32_t oWidth, oHeight, iWidth, iHeight, pixel_max;
	float pWidth, pHeight, lambda;
};

void run8(Params& i, const void* hInput, void* hOutput);
void run16(Params& i, const void* hInput, void* hOutput);

static void process8(const VSFrameRef * src, VSFrameRef * dst, DpidData * d, const VSAPI * vsapi)
{
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
	//args.pWidth = args.iWidth / (float)args.oWidth;
	//args.pHeight = args.iHeight / (float)args.oHeight; // This args broke when the struct "Params" contained "pixel_max".
	args.lambda = d->lambda;
	args.pixel_max = (1 << d->vi.format->bitsPerSample) - 1;

	for (unsigned y = 0; y < height; y++)
	{
		for (unsigned x = 0; x < width; x++)
		{
			const unsigned pos = (width * y + x) * 3;
			d->srcInterleaved[pos] = srcpR[x];
			d->srcInterleaved[pos + 1] = srcpG[x];
			d->srcInterleaved[pos + 2] = srcpB[x];
		}

		srcpR += srcStride;
		srcpG += srcStride;
		srcpB += srcStride;
	}

	run8(args, d->srcInterleaved, d->dstInterleaved);

	for (int y = 0; y < d->vi.height; y++)
	{
		for (int x = 0; x < d->vi.width; x++)
		{
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

static void process16(const VSFrameRef * src, VSFrameRef * dst, DpidData * d, const VSAPI * vsapi)
{
	const unsigned width = vsapi->getFrameWidth(src, 0);
	const unsigned height = vsapi->getFrameHeight(src, 0);
	const unsigned srcStride = vsapi->getStride(src, 0) / sizeof(uint16_t);
	const unsigned dstStride = vsapi->getStride(dst, 0) / sizeof(uint16_t);
	const uint16_t * srcpR = reinterpret_cast<const uint16_t *>(vsapi->getReadPtr(src, 0));
	const uint16_t * srcpG = reinterpret_cast<const uint16_t *>(vsapi->getReadPtr(src, 1));
	const uint16_t * srcpB = reinterpret_cast<const uint16_t *>(vsapi->getReadPtr(src, 2));
	uint16_t * VS_RESTRICT dstpR = reinterpret_cast<uint16_t *>(vsapi->getWritePtr(dst, 0));
	uint16_t * VS_RESTRICT dstpG = reinterpret_cast<uint16_t *>(vsapi->getWritePtr(dst, 1));
	uint16_t * VS_RESTRICT dstpB = reinterpret_cast<uint16_t *>(vsapi->getWritePtr(dst, 2));
	
	Params args;
	args.iWidth = width;
	args.iHeight = height;
	args.oWidth = d->vi.width;
	args.oHeight = d->vi.height;
	//args.pWidth = args.iWidth / (float)args.oWidth;
	//args.pHeight = args.iHeight / (float)args.oHeight;  // This args broke when the struct "Params" contained "pixel_max".
	args.lambda = d->lambda;
	args.pixel_max = (1 << d->vi.format->bitsPerSample) - 1;

	for (unsigned y = 0; y < height; y++)
	{
		for (unsigned x = 0; x < width; x++)
		{
			const unsigned pos = (width * y + x) * 3;
			d->src16Interleaved[pos] = srcpR[x];
			d->src16Interleaved[pos + 1] = srcpG[x];
			d->src16Interleaved[pos + 2] = srcpB[x];
		}

		srcpR += srcStride;
		srcpG += srcStride;
		srcpB += srcStride;
	}

	run16(args, d->src16Interleaved, d->dst16Interleaved);

	for (int y = 0; y < d->vi.height; y++)
	{
		for (int x = 0; x < d->vi.width; x++)
		{
			const int pos = (d->vi.width * y + x) * 3;
			dstpR[x] = d->dst16Interleaved[pos];
			dstpG[x] = d->dst16Interleaved[pos + 1];
			dstpB[x] = d->dst16Interleaved[pos + 2];
		}

		dstpR += dstStride;
		dstpG += dstStride;
		dstpB += dstStride;
	}
}

static void VS_CC DpidInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi)
{
	DpidData * d = static_cast<DpidData *>(*instanceData);
	vsapi->setVideoInfo(&d->vi, 1, node);
}

static const VSFrameRef *VS_CC DpidGetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi)
{
	DpidData * d = static_cast<DpidData *>(*instanceData);

	if (activationReason == arInitial)
	{
		vsapi->requestFrameFilter(n, d->node, frameCtx);
	}
	else if (activationReason == arAllFramesReady)
	{
		const VSFrameRef * src = vsapi->getFrameFilter(n, d->node, frameCtx);
		VSFrameRef * dst = vsapi->newVideoFrame(d->vi.format, d->vi.width, d->vi.height, src, core);

		if (d->vi.format->bytesPerSample == 1)
		{
			process8(src, dst, d, vsapi);
		}
		else //d->vi.format->bytesPerSample == 1
		{
			process16(src, dst, d, vsapi);
		}

		vsapi->freeFrame(src);
		return dst;
	}

	return nullptr;
}

static void VS_CC DpidFree(void *instanceData, VSCore *core, const VSAPI *vsapi)
{
	DpidData * d = static_cast<DpidData *>(instanceData);

	vsapi->freeNode(d->node);

	if (d->vi.format->bytesPerSample == 1)
	{
		delete[] d->srcInterleaved;
		delete[] d->dstInterleaved;
	}
	else // d->vi.format->bytesPerSample == 2
	{
		delete[] d->src16Interleaved;
		delete[] d->dst16Interleaved;
	}
	delete d;
}

static void VS_CC DpidCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi)
{
	DpidData d{};
	int err;

	d.node = vsapi->propGetNode(in, "clip", 0, nullptr);
	d.vi = *vsapi->getVideoInfo(d.node);

	if (!isConstantFormat(&d.vi) || d.vi.format->sampleType != stInteger || d.vi.format->colorFamily != cmRGB)
	{
		vsapi->setError(out, "Dpid: only constant format integer RGB input supported");
		vsapi->freeNode(d.node);
		return;
	}

	d.vi.width = int64ToIntS(vsapi->propGetInt(in, "width", 0, &err));

	if (err)
	{
		d.vi.width = 0;
	}

	d.vi.height = int64ToIntS(vsapi->propGetInt(in, "height", 0, &err));

	if (err)
	{
		d.vi.height = 0;
	}

	if (d.vi.width == 0 && d.vi.height == 0)
	{
		vsapi->setError(out, "Dpid: \"width\" and \"height\" can not be equal to 0 at the same time");
		vsapi->freeNode(d.node);
		return;
	}
	else if (d.vi.width == 0)
	{
		d.vi.width = vsapi->getVideoInfo(d.node)->width * d.vi.height / vsapi->getVideoInfo(d.node)->height;
	}
	else if (d.vi.height == 0)
	{
		d.vi.height = vsapi->getVideoInfo(d.node)->height * d.vi.width / vsapi->getVideoInfo(d.node)->width;
	}

	d.lambda = static_cast<float>(vsapi->propGetFloat(in, "lambda", 0, &err));

	if (err)
	{
		d.lambda = 1.0f;
	}

	if (d.vi.format->bytesPerSample == 1)
	{
		d.srcInterleaved = new (std::nothrow) uint8_t[vsapi->getVideoInfo(d.node)->width * vsapi->getVideoInfo(d.node)->height * 3];
		d.dstInterleaved = new (std::nothrow) uint8_t[d.vi.width * d.vi.height * 3];
		if (!d.srcInterleaved || !d.dstInterleaved)
		{
			vsapi->setError(out, "Dpid: malloc failure (srcInterleaved/dstInterleaved)");
			vsapi->freeNode(d.node);
			return;
		}
	}
	
	else // (d.vi.format->bytesPerSample == 2)
	{
		d.src16Interleaved = new (std::nothrow) uint16_t[vsapi->getVideoInfo(d.node)->width * vsapi->getVideoInfo(d.node)->height * 3];
		d.dst16Interleaved = new (std::nothrow) uint16_t[d.vi.width * d.vi.height * 3];
		if (!d.src16Interleaved || !d.dst16Interleaved)
		{
			vsapi->setError(out, "Dpid: malloc failure (src16Interleaved/dst16Interleaved)");
			vsapi->freeNode(d.node);
			return;
		}
	}

	DpidData * data = new DpidData{ d };

	vsapi->createFilter(in, out, "Dpid", DpidInit, DpidGetFrame, DpidFree, fmParallelRequests, 0, data, core);
}

//////////////////////////////////////////
// Init

VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin)
{
	configFunc("com.wolframrhodium.dpid", "dpid", "Rapid, Detail-Preserving Image Downscaling", VAPOURSYNTH_API_VERSION, 1, plugin);
	registerFunc("Dpid", "clip:clip;width:int:opt;height:int:opt;lambda:float:opt",
		DpidCreate, nullptr, plugin);
}
