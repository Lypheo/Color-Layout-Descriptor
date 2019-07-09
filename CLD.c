#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "VapourSynth.h"
#include "VSHelper.h"

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

typedef struct {
    VSNodeRef *node;
    const VSVideoInfo *vi;
} FilterData;

static void VS_CC filterInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi) {
    FilterData *d = (FilterData *) * instanceData;
    vsapi->setVideoInfo(d->vi, 1, node);
}

int* zigzag(int m)
{
    int i, j, n, *s;
    s = malloc(sizeof(int) * m * m);

    for (i = n = 0; i < m * 2; i++)
        for (j = (i < m) ? 0 : i-m+1; j <= i && j < m; j++)
            s[n++ ] = (i&1)? j*(m-1)+i : (i-j)*m+j ;
    return s;
}

double dct_transform(int x, int y, const int64_t* grid) {
    double alpha_x = x == 0 ? sqrt(1./8.) : sqrt(2./8.);
    double alpha_y = y == 0 ? sqrt(1./8.) : sqrt(2./8.);

    double sum = 0;
    for (int ix=0; ix<8; ix++) {
        for (int iy=0; iy<8; iy++) {
            sum += (grid[iy*8 + ix]-128) * cos(M_PI * (2*ix+1) * x/16) * cos(M_PI * (2*iy+1) * y/16);
        }
    }
    sum = alpha_x*alpha_y*sum;
    return sum;
}

static const VSFrameRef *VS_CC filterGetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
    FilterData *d = (FilterData *) * instanceData;

    if (activationReason == arInitial) {
        vsapi->requestFrameFilter(n, d->node, frameCtx);
    } else if (activationReason == arAllFramesReady) {

        const VSFrameRef *frame = vsapi->getFrameFilter(n, d->node, frameCtx);
        const VSFormat *fi = d->vi->format;
        int ih = vsapi->getFrameHeight(frame, 0);
        int iw = vsapi->getFrameWidth(frame, 0);

        int64_t image[3][8][8];

        for (int plane = 0; plane < fi->numPlanes; plane++) {
            // source plane
            const uint8_t *framep = vsapi->getReadPtr(frame, plane);
            int frame_stride = vsapi->getStride(frame, plane);

            double interval_w = iw/8.;
            double interval_h = ih/8.;

            for (int j = 0; j < 8; j++) {
                for (int i = 0; i < 8; i++) {

                    double avg = 0;
                    int pixel_number = 0;
                    int start_y = (int) round(j * interval_h);
                    int end_y = (int) round((j+1) * interval_h); // exclusive
                    int start_x = (int) round(i * interval_w);
                    int end_x = (int) round((i+1) * interval_w); // exclusive

                    for (int y = start_y; y < end_y; y++) {
                        for (int x = start_x; x < end_x; x++) {
                            avg += framep[x + y*frame_stride];
                            pixel_number++;
                        }
                    }

                    image[plane][j][i] = (int64_t) (avg/pixel_number);
                }
            }
        }

        int* zz = zigzag(8);
        double dct[3][8][8];
        for (int plane = 0; plane < 3; plane++){
            for (int x = 0; x < 8; x++) {
                for (int y = 0; y < 8; y++) {
                    // zig-zag scan coordinates
                    int xx = zz[x + y*8]%8;
                    int yy = (int) floor(zz[x + y*8] / 8.);
                    dct[plane][y][x] = dct_transform(xx, yy, (const int64_t*) image[plane]);
                }
            }
        }
        free(zz);

        VSFrameRef *dst = vsapi->copyFrame(frame, core);
        VSMap *props = vsapi->getFramePropsRW(dst);
        vsapi->propSetFloatArray(props, "CLD_y", (const double *) dct[0], 64);
        vsapi->propSetFloatArray(props, "CLD_u", (const double *) dct[1], 64);
        vsapi->propSetFloatArray(props, "CLD_v", (const double *) dct[2], 64);

        vsapi->freeFrame(frame);
        return dst;
    }

//    char debug[100];
//    sprintf(debug, "Pixel: (%d, %d); RPM: (%f,%f) | %f,%f | Window:%d,%d - %d,%d", x, y, rpm_x, rpm_y, pixel, normalizer, window_x_lower, window_x_upper, window_y_lower, window_y_upper);
//    vsapi->logMessage(mtDebug, debug);
    return 0;
}

static void VS_CC filterFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
    FilterData *d = (FilterData *)instanceData;
    vsapi->freeNode(d->node);
    free(d);
}

static void VS_CC filterCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
    FilterData d;
    FilterData *data;
    int err;

    d.node = vsapi->propGetNode(in, "clip", 0, 0);
    d.vi = vsapi->getVideoInfo(d.node);
    if (d.vi->format->bitsPerSample != 8 || d.vi->format->sampleType != stInteger || d.vi->format->colorFamily != cmYUV) {
        vsapi->setError(out, "CLD: Only 8-bit YUV input supported.");
        vsapi->freeNode(d.node);
        return;
    }


    data = malloc(sizeof(d));
    *data = d;

    vsapi->createFilter(in, out, "ComputeCLD", filterInit, filterGetFrame, filterFree, fmParallel, 0, data, core);
}

//////////////////////////////////////////
// Init

VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin) {
    configFunc("com.CLD.ColorLayoutDescriptor", "cld", "MPEG-7 Color Layout Descriptor", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("ComputeCLD", "clip:clip", filterCreate, 0, plugin);
}
