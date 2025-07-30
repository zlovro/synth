#include "freeverb.h"
#include <stdlib.h>
#include <sfs/sfs.h>

#define undenormalize(n) { if (abs(n) < 1e-37) { (n) = 0; } }

RAM_D2_BUFFER fv_Context gFvCtx = {};

static inline float allpass_process(fv_Allpass *ap, float input) {
    float bufout = ap->buf[ap->bufidx];
    undenormalize(bufout);

    float output        = -input + bufout;
    ap->buf[ap->bufidx] = input + bufout * ap->feedback;

    if (++ap->bufidx >= ap->bufsize)
    {
        ap->bufidx = 0;
    }

    return output;
}


static inline float comb_process(fv_Comb *cmb, float input) {
    float output = cmb->buf[cmb->bufidx];
    undenormalize(output);

    cmb->filterstore = output * cmb->damp2 + cmb->filterstore * cmb->damp1;
    undenormalize(cmb->filterstore);

    cmb->buf[cmb->bufidx] = input + cmb->filterstore * cmb->feedback;

    if (++cmb->bufidx >= cmb->bufsize)
    {
        cmb->bufidx = 0;
    }

    return output;
}


static inline void comb_set_damp(fv_Comb *cmb, float n) {
    cmb->damp1 = n;
    cmb->damp2 = 1.0 - n;
}


void fv_init(fv_Context *pCtx, fv_ReverbSettings *pSettings) {
    zmem(pCtx, sizeof(*pCtx));

    for (int i = 0; i < FV_NUMALLPASSES; i++)
    {
        pCtx->allpass[i].feedback = 0.5;
    }

    fv_set_param(pCtx, pSettings);
}

void fv_set_param(fv_Context *ctx, fv_ReverbSettings *pReverbSettings) {
    fv_set_samplerate(ctx, SFS_SAMPLERATE);
    fv_set_wet(ctx, pReverbSettings->wet);
    fv_set_roomsize(ctx, pReverbSettings->roomSize);
    fv_set_dry(ctx, pReverbSettings->dry);
    fv_set_damp(ctx, pReverbSettings->damp);
    fv_set_width(ctx, pReverbSettings->width);
}


void fv_mute(fv_Context *ctx) {
    for (int i = 0; i < FV_NUMCOMBS; i++)
    {
        zmem(ctx->comb[i].buf, sizeof(ctx->comb[i].buf));
    }
    for (int i = 0; i < FV_NUMALLPASSES; i++)
    {
        zmem(ctx->allpass[i].buf, sizeof(ctx->allpass[i].buf));
    }
}


static void update(fv_Context *ctx) {
    ctx->wet1 = ctx->wet * (ctx->width * 0.5 + 0.5);
    ctx->wet2 = ctx->wet * ((1 - ctx->width) * 0.5);

    if (ctx->mode >= FV_FREEZEMODE)
    {
        ctx->roomsize1 = 1;
        ctx->damp1     = 0;
        ctx->gain      = FV_MUTED;
    }
    else
    {
        ctx->roomsize1 = ctx->roomsize;
        ctx->damp1     = ctx->damp;
        ctx->gain      = FV_FIXEDGAIN;
    }

    for (int i = 0; i < FV_NUMCOMBS; i++)
    {
        ctx->comb[i].feedback = ctx->roomsize1;
        comb_set_damp(&ctx->comb[i], ctx->damp1);
    }
}


void fv_set_samplerate(fv_Context *ctx, float value) {
    const int combs[]     = {1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617};
    const int allpasses[] = {556, 441, 341, 225};

    double multiplier = value / SFS_SAMPLERATE;

    /* init comb buffers */
    for (int i = 0; i < FV_NUMCOMBS; i++)
    {
        ctx->comb[i].bufsize = combs[i] * multiplier;
    }

    /* init allpass buffers */
    for (int i = 0; i < FV_NUMALLPASSES; i++)
    {
        ctx->allpass[i].bufsize = allpasses[i] * multiplier;
    }
}


void fv_set_mode(fv_Context *ctx, float value) {
    ctx->mode = value;
    update(ctx);
}


void fv_set_roomsize(fv_Context *ctx, float value) {
    ctx->roomsize = value * FV_SCALEROOM + FV_OFFSETROOM;
    update(ctx);
}


void fv_set_damp(fv_Context *ctx, float value) {
    ctx->damp = value * FV_SCALEDAMP;
    update(ctx);
}


void fv_set_wet(fv_Context *ctx, float value) {
    ctx->wet = value * FV_SCALEWET;
    update(ctx);
}


void fv_set_dry(fv_Context *ctx, float value) {
    ctx->dry = value * FV_SCALEDRY;
}


void fv_set_width(fv_Context *ctx, float value) {
    ctx->width = value;
    update(ctx);
}


void fv_process(fv_Context *ctx, float *buf, int n) {
    for (int i = 0; i < n; i++)
    {
        float outl  = 0;
        float outr  = 0;
        float input = buf[i] * ctx->gain;

        /* accumulate comb filters in parallel */
        for (int j = 0; j < FV_NUMCOMBS; j++)
        {
            outl += comb_process(&ctx->comb[j], input);
        }

        /* feed through allpasses in series */
        for (int j = 0; j < FV_NUMALLPASSES; j++)
        {
            outl = allpass_process(&ctx->allpass[j], outl);
        }

        /* replace buffer with output */
        buf[i] = outl * ctx->wet1 + outr * ctx->wet2 + buf[i] * ctx->dry;
    }
}
