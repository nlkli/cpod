#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/samplefmt.h>

#include <AudioToolbox/AudioToolbox.h>

#define SR 44100
#define BUF 1024

static float phase = 0;

void callback(void *userData,
              AudioQueueRef q,
              AudioQueueBufferRef buf)
{
    float *out = (float *)buf->mAudioData;

    for (int i = 0; i < BUF; i++) {

        out[i] = sinf(phase) * 0.2f;

        phase += 2.0f * M_PI * 440.0f / SR;
    }

    buf->mAudioDataByteSize = BUF * sizeof(float);

    AudioQueueEnqueueBuffer(q, buf, 0, NULL);
}

int main()
{
    AudioStreamBasicDescription asbd = {0};

    asbd.mSampleRate = 48000;

    // PCM
    asbd.mFormatID = kAudioFormatLinearPCM;

    // float32 + packed
    asbd.mFormatFlags =
        kLinearPCMFormatFlagIsFloat |
        kLinearPCMFormatFlagIsPacked;

    // float32 = 32 bits
    asbd.mBitsPerChannel = 32;

    // stereo
    asbd.mChannelsPerFrame = 2;

    // PCM packet = 1 frame
    asbd.mFramesPerPacket = 1;

    // float32 stereo:
    // 4 bytes * 2 channels
    asbd.mBytesPerFrame = 8;

    // PCM: packet == frame
    asbd.mBytesPerPacket = 8;

    // ...

    AudioQueueRef q;

    AudioQueueNewOutput(&asbd, callback, NULL, NULL, NULL, 0, &q);

    for (int i = 0; i < 3; i++) {
        AudioQueueBufferRef buf;
        AudioQueueAllocateBuffer(q, BUF * sizeof(float), &buf);
        callback(NULL, q, buf);
    }

    AudioQueueStart(q, NULL);

    sleep(5);

    AudioQueueStop(q, true);
    AudioQueueDispose(q, true);
}

// int main(void) {
//     const char *url = "./resources/Hide rework - Dark Fantasy Quest [lzwc6VDSBUo].opus";
// 
//     AVFormatContext *fmt_ctx = NULL;
// 
//     int ret;
// 
//     ret = avformat_open_input(&fmt_ctx, url, NULL, NULL);
//     if (ret < 0)
//         abort();
// 
//     ret = avformat_find_stream_info(fmt_ctx, NULL);
//     if (ret < 0)
//         abort();
// 
//     const AVCodec *dec;
// 
//     ret = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, &dec, 0);
//     if (ret < 0)
//         abort();
// 
//     int audio_stream_idx = ret;
// 
//     AVCodecContext *dec_ctx;
// 
//     dec_ctx = avcodec_alloc_context3(dec);
//     if (!dec_ctx)
//         abort();
// 
//     avcodec_parameters_to_context(dec_ctx, fmt_ctx->streams[audio_stream_idx]->codecpar);
// 
//     ret = avcodec_open2(dec_ctx, dec, NULL);
//     if (ret < 0)
//         abort();
// 
//     AVPacket *packet = av_packet_alloc();
//     AVFrame *frame = av_frame_alloc();
// 
//     while (av_read_frame(fmt_ctx, packet) >= 0) {
// 
//         if (packet->stream_index != audio_stream_idx) {
//             av_packet_unref(packet);
//             continue;
//         }
// 
//         avcodec_send_packet(dec_ctx, packet);
// 
//         while (avcodec_receive_frame(dec_ctx, frame) == 0) {
// 
//             enum AVSampleFormat fmt = frame->format;
// 
//             const char *fmt_name =
//                 av_get_sample_fmt_name(fmt);
// 
//             int planar =
//                 av_sample_fmt_is_planar(fmt);
// 
//             int channels =
//                 frame->ch_layout.nb_channels;
// 
//             int bytes_per_sample =
//                 av_get_bytes_per_sample(fmt);
// 
//             int total_bytes =
//                 frame->nb_samples *
//                 channels *
//                 bytes_per_sample;
// 
//             char layout[128];
//             av_channel_layout_describe(
//                     &frame->ch_layout,
//                     layout,
//                     sizeof(layout));
// 
//             printf("sample_rate      : %d Hz\n", frame->sample_rate);
//             printf("channels         : %d\n", channels);
//             printf("channel_layout   : %s\n", layout);
//             printf("samples          : %d\n", frame->nb_samples);
//             printf("sample_format    : %s\n", fmt_name ? fmt_name : "unknown");
//             printf("planar           : %s\n", planar ? "yes" : "no");
//             printf("bytes/sample     : %d\n", bytes_per_sample);
//             printf("frame_size       : %d bytes\n", total_bytes);
//             printf("pts              : %lld\n", frame->pts);
//             printf("duration         : %lld\n", frame->duration);
// 
//             printf("\n");
// 
//             av_frame_unref(frame);
//         }
// 
//         av_packet_unref(packet);
//     }
// 
//     av_frame_free(&frame);
//     av_packet_free(&packet);
// 
//     avcodec_free_context(&dec_ctx);
//     avformat_close_input(&fmt_ctx);
// }
