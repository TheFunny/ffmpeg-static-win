// Guards the two defects this package exists to avoid, without needing the app:
//
//   1. libvpx must be built with --enable-vp9-highbitdepth. Without it,
//      avcodec_open2() with AV_PIX_FMT_YUV420P10LE fails ("Specified pixel
//      format ... not supported") and StickerProcess' MP4 path silently loses
//      10-bit VP9.
//   2. The package must be complete for static linking: avcodec/avformat/
//      avdevice plus libvpx and zlib and the Windows system libraries, with no
//      ffmpeg DLL left to import (checked by run-smoke.ps1 with dumpbin).
//
// Built and run by test/run-smoke.ps1 against installed/x64-windows-static.
// MSVC C11: no compound literals, no designated initializers.

#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libavutil/frame.h>

#include <stdint.h>
#include <stdio.h>

#define WIDTH 64
#define HEIGHT 64
#define FRAMES 3

static int fail(const char *what) {
    fprintf(stderr, "FAIL: %s\n", what);
    return 1;
}

static int failed(const char *what, int err) {
    char buf[AV_ERROR_MAX_STRING_SIZE] = {0};
    av_strerror(err, buf, sizeof(buf));
    fprintf(stderr, "FAIL: %s: %s\n", what, buf);
    return 1;
}

int main(void) {
    // Touch the avdevice surface too: it is part of the requested feature set
    // and drags in the vfwcap -> avicap32 dependency chain.
    avdevice_register_all();
    printf("libavdevice %u.%u.%u, libavcodec %u.%u.%u\n",
           avdevice_version() >> 16, (avdevice_version() >> 8) & 0xff, avdevice_version() & 0xff,
           avcodec_version() >> 16, (avcodec_version() >> 8) & 0xff, avcodec_version() & 0xff);

    const AVCodec *enc = avcodec_find_encoder_by_name("libvpx-vp9");
    if (!enc)
        return fail("libvpx-vp9 encoder is missing from avcodec");

    AVCodecContext *ctx = avcodec_alloc_context3(enc);
    if (!ctx)
        return fail("avcodec_alloc_context3(libvpx-vp9)");

    ctx->width = WIDTH;
    ctx->height = HEIGHT;
    ctx->time_base.num = 1;
    ctx->time_base.den = 30;
    ctx->framerate.num = 30;
    ctx->framerate.den = 1;
    ctx->bit_rate = 200000;
    ctx->pix_fmt = AV_PIX_FMT_YUV420P10LE;  // the highbitdepth gate

    int err = avcodec_open2(ctx, enc, NULL);
    if (err < 0)
        return failed("avcodec_open2 with AV_PIX_FMT_YUV420P10LE (libvpx built without highbitdepth?)", err);

    AVFrame *frame = av_frame_alloc();
    if (!frame)
        return fail("av_frame_alloc");
    frame->format = ctx->pix_fmt;
    frame->width = WIDTH;
    frame->height = HEIGHT;
    if (av_frame_get_buffer(frame, 32) < 0)
        return fail("av_frame_get_buffer");

    AVPacket *pkt = av_packet_alloc();
    if (!pkt)
        return fail("av_packet_alloc");

    int encoded = 0;
    for (int n = 0; n < FRAMES; n++) {
        if (av_frame_make_writable(frame) < 0)
            return fail("av_frame_make_writable");
        for (int y = 0; y < HEIGHT; y++) {
            uint16_t *row = (uint16_t *)(frame->data[0] + (size_t)y * frame->linesize[0]);
            for (int x = 0; x < WIDTH; x++)
                row[x] = (uint16_t)((x * 1023) / (WIDTH - 1));
        }
        for (int p = 1; p < 3; p++) {
            for (int y = 0; y < HEIGHT / 2; y++) {
                uint16_t *row = (uint16_t *)(frame->data[p] + (size_t)y * frame->linesize[p]);
                for (int x = 0; x < WIDTH / 2; x++)
                    row[x] = 512;
            }
        }
        frame->pts = n;
        if ((err = avcodec_send_frame(ctx, frame)) < 0)
            return failed("avcodec_send_frame", err);
        while (avcodec_receive_packet(ctx, pkt) == 0) {
            encoded += pkt->size;
            av_packet_unref(pkt);
        }
    }
    if ((err = avcodec_send_frame(ctx, NULL)) < 0)  // flush
        return failed("avcodec_send_frame(NULL)", err);
    while (avcodec_receive_packet(ctx, pkt) == 0) {
        encoded += pkt->size;
        av_packet_unref(pkt);
    }
    if (encoded <= 0)
        return fail("no VP9 packet produced");

    // The muxer the app wraps the encoded stream in.
    AVFormatContext *out = NULL;
    if (avformat_alloc_output_context2(&out, NULL, "webm", NULL) < 0 || !out)
        return fail("webm muxer is missing from avformat");
    avformat_free_context(out);

    printf("ok: libvpx-vp9 yuv420p10le produced %d bytes across %d frames; webm muxer present\n",
           encoded, FRAMES);

    av_packet_free(&pkt);
    av_frame_free(&frame);
    avcodec_free_context(&ctx);
    return 0;
}
