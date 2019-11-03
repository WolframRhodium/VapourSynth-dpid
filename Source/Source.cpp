#include "vapoursynth/VapourSynth.h"
#include "vapoursynth/VSHelper.h"
#include <cstdint>
#include <cmath>
#include <string>
#include <algorithm>
#include <memory>


struct DpidData {
    VSNodeRef *node1, *node2;
    int dst_w, dst_h;
    float lambda[3];
    float src_left[3], src_top[3];
    bool process[3];
    bool read_chromaloc;
};


static float contribution(float f, float x, float y, 
    float sx, float ex, float sy, float ey) {

    if (x < sx)
        f *= 1.0f - (sx - x);

    if ((x + 1.0f) > ex)
        f *= ex - x;

    if (y < sy)
        f *= 1.0f - (sy - y);

    if ((y + 1.0f) > ey)
        f *= ey - y;

    return f;
}

template<typename T>
static void dpidProcess(const T * VS_RESTRICT srcp, int src_stride, 
    const T * VS_RESTRICT downp, int down_stride, 
    T * VS_RESTRICT dstp, int dst_stride, 
    int src_w, int src_h, int dst_w, int dst_h, float lambda, 
    float src_left, float src_top) {

    const float scale_x = static_cast<float>(src_w) / dst_w;
    const float scale_y = static_cast<float>(src_h) / dst_h;

    for (int outer_y = 0; outer_y < dst_h; ++outer_y) {
        for (int outer_x = 0; outer_x < dst_w; ++outer_x) {

            // avg = RemoveGrain(down, 11)
            float avg {};
            for (int inner_y = -1; inner_y <= 1; ++inner_y) {
                for (int inner_x = -1; inner_x <= 1; ++inner_x) {

                    int y = std::clamp(outer_y + inner_y, 0, dst_h - 1);
                    int x = std::clamp(outer_x + inner_x, 0, dst_w - 1);

                    T pixel = downp[y * down_stride + x];
                    avg += pixel * (2 - std::abs(inner_y)) * (2 - std::abs(inner_x));
                }
            }
            avg /= 16.f;

            // Dpid
            const float sx = std::clamp(outer_x * scale_x + src_left, 0.f, static_cast<float>(src_w));
            const float ex = std::clamp((outer_x + 1) * scale_x + src_left, 0.f, static_cast<float>(src_w));
            const float sy = std::clamp(outer_y * scale_y + src_top, 0.f, static_cast<float>(src_h));
            const float ey = std::clamp((outer_y + 1) * scale_y + src_top, 0.f, static_cast<float>(src_h));

            const int sxr = static_cast<int>(std::floor(sx));
            const int exr = static_cast<int>(std::ceil(ex));
            const int syr = static_cast<int>(std::floor(sy));
            const int eyr = static_cast<int>(std::ceil(ey));

            float sum_pixel {};
            float sum_weight {};

            for (int inner_y = syr; inner_y < eyr; ++inner_y) {
                for (int inner_x = sxr; inner_x < exr; ++inner_x) {
                    T pixel = srcp[inner_y * src_stride + inner_x];
                    float distance = std::abs(avg - static_cast<float>(pixel));
                    float weight = std::pow(distance, lambda);
                    weight = contribution(weight, static_cast<float>(inner_x), static_cast<float>(inner_y), sx, ex, sy, ey);

                    sum_pixel += weight * pixel;
                    sum_weight += weight;
                }
            }

            dstp[outer_y * dst_stride + outer_x] = static_cast<T>((sum_weight == 0.f) ? avg : sum_pixel / sum_weight);
        }
    }
}

static const VSFrameRef *VS_CC dpidGetframe(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
    DpidData *d = reinterpret_cast<DpidData *>(*instanceData);

    if (activationReason == arInitial) {
        vsapi->requestFrameFilter(n, d->node1, frameCtx);
        vsapi->requestFrameFilter(n, d->node2, frameCtx);
    } else if (activationReason == arAllFramesReady) {
        const VSFrameRef *src1 = vsapi->getFrameFilter(n, d->node1, frameCtx);
        const VSFrameRef *src2 = vsapi->getFrameFilter(n, d->node2, frameCtx);
        const VSFormat *fi = vsapi->getFrameFormat(src2);

        const VSFrameRef * fr[] = {
            d->process[0] ? nullptr : src2, 
            d->process[1] ? nullptr : src2, 
            d->process[2] ? nullptr : src2};

        constexpr int pl[] = {0, 1, 2};

        VSFrameRef *dst = vsapi->newVideoFrame2(
            fi, vsapi->getFrameWidth(src2, 0), vsapi->getFrameHeight(src2, 0), fr, pl, src2, core);

        for (int plane = 0; plane < fi->numPlanes; ++plane) {
            if (d->process[plane]) {
                const void *src1p = vsapi->getReadPtr(src1, plane);
                const int src1_stride = vsapi->getStride(src1, plane) / fi->bytesPerSample;
                const void *src2p = vsapi->getReadPtr(src2, plane);
                const int src2_stride = vsapi->getStride(src2, plane) / fi->bytesPerSample;
                void *dstp = vsapi->getWritePtr(dst, plane);
                const int dst_stride = vsapi->getStride(dst, plane) / fi->bytesPerSample;

                const int src_w = vsapi->getFrameWidth(src1, plane);
                const int src_h = vsapi->getFrameHeight(src1, plane);
                const int dst_w = vsapi->getFrameWidth(src2, plane);
                const int dst_h = vsapi->getFrameHeight(src2, plane);

                float src_left, src_top;
                if (plane != 0 && d->read_chromaloc) { 
                    int chromaLocation;

                    {
                        int err;

                        chromaLocation = int64ToIntS(vsapi->propGetInt(vsapi->getFramePropsRO(src2), "_ChromaLocation", 0, &err));
                        if (err) {
                            chromaLocation = 0;
                        }
                    }

                    const float hSubS = static_cast<float>(1 << fi->subSamplingW);
                    const float hCPlace = (chromaLocation == 0 || chromaLocation == 2 || chromaLocation == 4) 
                        ? (0.5f - hSubS / 2) : 0.f;
                    const float hScale = static_cast<float>(dst_w) / static_cast<float>(src_w);

                    const float vSubS = static_cast<float>(1 << fi->subSamplingH);
                    const float vCPlace = (chromaLocation == 2 || chromaLocation == 3) 
                        ? (0.5f - vSubS / 2) : ((chromaLocation == 4 || chromaLocation == 5) ? (vSubS / 2 - 0.5f) : 0.f);
                    const float vScale = static_cast<float>(dst_h) / static_cast<float>(src_h);

                    src_left = ((d->src_left[plane] - hCPlace) * hScale + hCPlace) / hScale / hSubS;
                    src_top = ((d->src_top[plane] - vCPlace) * vScale + vCPlace) / vScale / vSubS;
                } else {
                    src_left = d->src_left[plane];
                    src_top = d->src_top[plane];
                }

                // process
                if (fi->sampleType == stInteger) {
                    if (fi->bytesPerSample == 1) {
                        dpidProcess(
                            reinterpret_cast<const uint8_t *>(src1p), src1_stride, 
                            reinterpret_cast<const uint8_t *>(src2p), src2_stride, 
                            reinterpret_cast<uint8_t *>(dstp), dst_stride, 
                            src_w, src_h, dst_w, dst_h, d->lambda[plane], 
                            src_left, src_top);
                    } else if (fi->bytesPerSample == 2) {
                        dpidProcess(
                            reinterpret_cast<const uint16_t *>(src1p), src1_stride, 
                            reinterpret_cast<const uint16_t *>(src2p), src2_stride, 
                            reinterpret_cast<uint16_t *>(dstp), dst_stride, 
                            src_w, src_h, dst_w, dst_h, d->lambda[plane], 
                            src_left, src_top);
                    }
                } else if (fi->sampleType == stFloat) {
                    if (fi->bytesPerSample == 4) {
                        dpidProcess(
                            reinterpret_cast<const float *>(src1p), src1_stride, 
                            reinterpret_cast<const float *>(src2p), src2_stride, 
                            reinterpret_cast<float *>(dstp), dst_stride, 
                            src_w, src_h, dst_w, dst_h, d->lambda[plane], 
                            src_left, src_top);
                    }
                }
            }
        }

        vsapi->freeFrame(src1);
        vsapi->freeFrame(src2);
        return dst;
    }

    return nullptr;
}

static void VS_CC dpidNodeInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi) {
    DpidData *d = reinterpret_cast<DpidData *>(*instanceData);
    vsapi->setVideoInfo(vsapi->getVideoInfo(d->node2), 1, node);
}


static void VS_CC dpidNodeFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
    DpidData *d = reinterpret_cast<DpidData *>(instanceData);

    vsapi->freeNode(d->node1);
    vsapi->freeNode(d->node2);
    delete d;
}


static void VS_CC dpidRawCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
    std::unique_ptr<DpidData> d = std::make_unique<DpidData>();

    d->node1 = vsapi->propGetNode(in, "clip", 0, nullptr);
    d->node2 = vsapi->propGetNode(in, "clip2", 0, nullptr);
    const VSVideoInfo *vi = vsapi->getVideoInfo(d->node2);
    d->dst_w = vi->width;
    d->dst_h = vi->height;

    int err;

    try {
        if (!isConstantFormat(vi) ||
            (vi->format->sampleType == stInteger && vi->format->bitsPerSample > 16) ||
            (vi->format->sampleType == stFloat && vi->format->bitsPerSample != 32))
            throw std::string{"only constant format 8-16 bit integer and 32 bit float input supported"};


        if (const VSVideoInfo *vi_src = vsapi->getVideoInfo(d->node1);
            (vi->format->id != vi_src->format->id) || (vi->numFrames != vi_src->numFrames))
            throw std::string{"\"clip\" and \"clip2\" must be of the same format and number of frames"};

        const int numLambda = vsapi->propNumElements(in, "lambda");
        if (numLambda > vi->format->numPlanes)
            throw std::string{"more \"lambda\" given than there are planes"};

        for (int i = 0; i < 3; i++) {
            if (i < numLambda)
                d->lambda[i] = static_cast<float>(vsapi->propGetFloat(in, "lambda", i, nullptr));
            else if (i == 0)
                d->lambda[0] = 1.0f;
            else 
                d->lambda[i] = d->lambda[i-1];
        }

        const int numPlanes = vsapi->propNumElements(in, "planes");

        for (int i = 0; i < 3; i++)
            d->process[i] = (numPlanes <= 0);

        for (int i = 0; i < numPlanes; i++) {
            const int n = int64ToIntS(vsapi->propGetInt(in, "planes", i, nullptr));

            if (n < 0 || n >= vi->format->numPlanes)
                throw std::string{"plane index out of range"};

            if (d->process[n])
                throw std::string{"plane specified twice"};

            d->process[n] = true;
        }

        const int numSrcLeft = vsapi->propNumElements(in, "src_left");
        if (numSrcLeft > vi->format->numPlanes)
            throw std::string{"more \"src_left\" given than there are planes"};

        const int numSrcTop = vsapi->propNumElements(in, "src_top");
        if (numSrcTop > vi->format->numPlanes)
            throw std::string{"more \"src_top\" given than there are planes"};

        for (int i = 0; i < 3; i++) {
            if (i < numSrcLeft)
                d->src_left[i] = static_cast<float>(vsapi->propGetFloat(in, "src_left", i, nullptr));
            else if (i == 0)
                d->src_left[0] = 0.0f;
            else 
                d->src_left[i] = d->src_left[i-1];

            if (i < numSrcTop)
                d->src_top[i] = static_cast<float>(vsapi->propGetFloat(in, "src_top", i, nullptr));
            else if (i == 0)
                d->src_top[0] = 0.0f;
            else 
                d->src_top[i] = d->src_top[i-1];
        }

        d->read_chromaloc = static_cast<bool>(vsapi->propGetInt(in, "read_chromaloc", 0, &err));
        if (err) {
            d->read_chromaloc = true;
        }

    } catch (const std::string &error) {
        vsapi->setError(out, ("DpidRaw: " + error).c_str());
        vsapi->freeNode(d->node1);
        vsapi->freeNode(d->node2);
        return;
    }

    vsapi->createFilter(in, out, "DpidRaw", dpidNodeInit, dpidGetframe, dpidNodeFree,
        fmParallel, nfNoCache, d.release(), core);
}


static void VS_CC dpidCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
    std::unique_ptr<DpidData> d = std::make_unique<DpidData>();

    VSNodeRef *node = vsapi->propGetNode(in, "clip", 0, nullptr);
    d->node1 = node;

    const VSVideoInfo *vi = vsapi->getVideoInfo(node);

    int err;

    try {
        if (!isConstantFormat(vi) ||
            (vi->format->sampleType == stInteger && vi->format->bitsPerSample > 16) ||
            (vi->format->sampleType == stFloat && vi->format->bitsPerSample != 32))
            throw std::string{"only constant format 8-16 bit integer and 32 bit float input supported"};

        // read arguments
        d->dst_w = int64ToIntS(vsapi->propGetInt(in, "width", 0, &err));
        if (err) {
            d->dst_w = vi->width;
        }

        d->dst_h = int64ToIntS(vsapi->propGetInt(in, "height", 0, &err));
        if (err) {
            d->dst_h = vi->height;
        }

        if (d->dst_w == vi->width && d->dst_h == vi->height) 
            throw std::string{"dimensions of output is identical to input. "
                "Please consider remove the function call"};

        const int numLambda = vsapi->propNumElements(in, "lambda");
        if (numLambda > vi->format->numPlanes)
            throw std::string{"more \"lambda\" given than there are planes"};

        for (int i = 0; i < 3; i++) {
            if (i < numLambda)
                d->lambda[i] = static_cast<float>(vsapi->propGetFloat(in, "lambda", i, nullptr));
            else if (i == 0)
                d->lambda[0] = 1.0f;
            else 
                d->lambda[i] = d->lambda[i-1];
        }

        for (int i = 0; i < 3; i++)
            d->process[i] = true;

        const int numSrcLeft = vsapi->propNumElements(in, "src_left");
        if (numSrcLeft > vi->format->numPlanes)
            throw std::string{"more \"src_left\" given than there are planes"};

        const int numSrcTop = vsapi->propNumElements(in, "src_top");
        if (numSrcTop > vi->format->numPlanes)
            throw std::string{"more \"src_top\" given than there are planes"};

        for (int i = 0; i < 3; i++) {
            if (i < numSrcLeft)
                d->src_left[i] = static_cast<float>(vsapi->propGetFloat(in, "src_left", i, nullptr));
            else if (i == 0)
                d->src_left[0] = 0.0f;
            else 
                d->src_left[i] = d->src_left[i-1];

            if (i < numSrcTop)
                d->src_top[i] = static_cast<float>(vsapi->propGetFloat(in, "src_top", i, nullptr));
            else if (i == 0)
                d->src_top[0] = 0.0f;
            else 
                d->src_top[i] = d->src_top[i-1];
        }

        d->read_chromaloc = static_cast<bool>(vsapi->propGetInt(in, "read_chromaloc", 0, &err));
        if (err) {
            d->read_chromaloc = true;
        }

        // preprocess
        VSMap * vtmp1 = vsapi->createMap();
        vsapi->propSetNode(vtmp1, "clip", node, paReplace);
        vsapi->propSetInt(vtmp1, "width", d->dst_w, paReplace);
        vsapi->propSetInt(vtmp1, "height", d->dst_h, paReplace);
        vsapi->propSetFloat(vtmp1, "src_left", d->src_left[0], paReplace);
        vsapi->propSetFloat(vtmp1, "src_top", d->src_top[0], paReplace);

        VSMap * vtmp2 = vsapi->invoke(vsapi->getPluginByNs("resize", core), "Bilinear", vtmp1);
        if (vsapi->getError(vtmp2)) {
            vsapi->setError(out, vsapi->getError(vtmp2));
            vsapi->freeMap(vtmp1);
            vsapi->freeMap(vtmp2);
            return;
        }

        vsapi->clearMap(vtmp1);
        node = vsapi->propGetNode(vtmp2, "clip", 0, nullptr);
        vsapi->clearMap(vtmp2);

        d->node2 = node;
        vsapi->createFilter(vtmp1, vtmp2, "Dpid", dpidNodeInit, dpidGetframe, dpidNodeFree,
            fmParallel, nfNoCache, d.release(), core);

        vsapi->freeMap(vtmp1);
        node = vsapi->propGetNode(vtmp2, "clip", 0, nullptr);
        vsapi->freeMap(vtmp2);

        vsapi->propSetNode(out, "clip", node, paReplace);
        vsapi->freeNode(node);

    } catch (const std::string &error) {
        vsapi->setError(out, ("Dpid: " + error).c_str());
        vsapi->freeNode(node);
        return;
    }
}


VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin) {
    configFunc("com.wolframrhodium.dpid", "dpid", "Rapid, Detail-Preserving Image Downscaling", VAPOURSYNTH_API_VERSION, 1, plugin);

    registerFunc("DpidRaw", 
        "clip:clip;"
        "clip2:clip;"
        "lambda:float[]:opt;"
        "src_left:float[]:opt;"
        "src_top:float[]:opt;"
        "read_chromaloc:int:opt;"
        "planes:int[]:opt;", dpidRawCreate, 0, plugin);

    registerFunc("Dpid", 
        "clip:clip;"
        "width:int:opt;"
        "height:int:opt;"
        "lambda:float[]:opt;"
        "src_left:float[]:opt;"
        "src_top:float[]:opt;"
        "read_chromaloc:int:opt;", dpidCreate, 0, plugin);
}
