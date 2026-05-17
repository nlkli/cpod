#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/samplefmt.h>

#include <AudioToolbox/AudioToolbox.h>

#define SR 44100
#define BUF 1024

typedef struct {

  char *next_audio_path;

  AVFormatContext *fmt_ctx;
  const AVCodec *codec;
  int audio_stream_idx;
  AVCodecContext *codec_ctx;
  AVPacket *packet;
  AVFrame *frame;

} PlayerState;

int next_audio(char *path, PlayerState *ps) {
  ps->next_audio_path = path;

  if (ps->fmt_ctx != NULL)
    avformat_close_input(&ps->fmt_ctx);

  int ret;

  ret = avformat_open_input(&ps->fmt_ctx, path, NULL, NULL);
  if (ret < 0)
    return ret;

  ret = avformat_find_stream_info(ps->fmt_ctx, NULL);
  if (ret < 0)
    return ret;

  ret = av_find_best_stream(ps->fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, &ps->codec,
                            0);
  if (ret < 0)
    return ret;

  ps->audio_stream_idx = ret;

  if (ps->codec_ctx != NULL)
    avcodec_free_context(&ps->codec_ctx);

  ps->codec_ctx = avcodec_alloc_context3(ps->codec);
  if (!ps->codec_ctx)
    return -1;

  avcodec_parameters_to_context(
      ps->codec_ctx, ps->fmt_ctx->streams[ps->audio_stream_idx]->codecpar);

  ret = avcodec_open2(ps->codec_ctx, ps->codec, NULL);
  if (ret < 0)
    return ret;

  if (ps->packet != NULL)
    av_packet_free(&ps->packet);
  if (ps->frame != NULL)
    av_frame_free(&ps->frame);

  ps->packet = av_packet_alloc();
  ps->frame = av_frame_alloc();

  return 0;
}

int next_frame(PlayerState *ps) {

  if (avcodec_receive_frame(ps->codec_ctx, ps->frame) == 0) {

    enum AVSampleFormat fmt = ps->frame->format;

    const char *fmt_name = av_get_sample_fmt_name(fmt);

    int planar = av_sample_fmt_is_planar(fmt);

    int channels = ps->frame->ch_layout.nb_channels;

    int bytes_per_sample = av_get_bytes_per_sample(fmt);

    int total_bytes = ps->frame->nb_samples * channels * bytes_per_sample;

    char layout[128];
    av_channel_layout_describe(&ps->frame->ch_layout, layout, sizeof(layout));

    printf("sample_rate      : %d Hz\n", ps->frame->sample_rate);
    printf("channels         : %d\n", channels);
    printf("channel_layout   : %s\n", layout);
    printf("samples          : %d\n", ps->frame->nb_samples);
    printf("sample_format    : %s\n", fmt_name ? fmt_name : "unknown");
    printf("planar           : %s\n", planar ? "yes" : "no");
    printf("bytes/sample     : %d\n", bytes_per_sample);
    printf("frame_size       : %d bytes\n", total_bytes);
    printf("pts              : %lld\n", ps->frame->pts);
    printf("duration         : %lld\n", ps->frame->duration);

    printf("\n");

    av_frame_unref(ps->frame);

    return 1;
  }

  av_packet_unref(ps->packet);

  do {
    if (av_read_frame(ps->fmt_ctx, ps->packet) < 0) {
      av_frame_free(&ps->frame);
      av_packet_free(&ps->packet);
      return 0;
    }
  } while (ps->packet->stream_index != ps->audio_stream_idx);

  avcodec_send_packet(ps->codec_ctx, ps->packet);

  return next_frame(ps);
}

int main(void) {
  PlayerState ps = {0};

  next_audio("./resources/Hide rework - Dark Fantasy Quest [lzwc6VDSBUo].opus",
             &ps);

  while (next_frame(&ps));

  avcodec_free_context(&ps.codec_ctx);
  avformat_close_input(&ps.fmt_ctx);

  return 0;
}

// void callback(void *userData,
//               AudioQueueRef q,
//               AudioQueueBufferRef buf)
// {
//     float *out = (float *)buf->mAudioData;
//
//     for (int i = 0; i < BUF; i++) {
//
//         out[i] = sinf(phase) * 0.2f;
//
//         phase += 2.0f * M_PI * 440.0f / SR;
//     }
//
//     buf->mAudioDataByteSize = BUF * sizeof(float);
//
//     AudioQueueEnqueueBuffer(q, buf, 0, NULL);
// }

// int main()
// {
//     AudioStreamBasicDescription asbd = {0};
//
//     asbd.mSampleRate = 48000;
//
//     // PCM
//     asbd.mFormatID = kAudioFormatLinearPCM;
//
//     // float32 + packed
//     asbd.mFormatFlags =
//         kLinearPCMFormatFlagIsFloat |
//         kLinearPCMFormatFlagIsPacked;
//
//     // float32 = 32 bits
//     asbd.mBitsPerChannel = 32;
//
//     // stereo
//     asbd.mChannelsPerFrame = 2;
//
//     // PCM packet = 1 frame
//     asbd.mFramesPerPacket = 1;
//
//     // float32 stereo:
//     // 4 bytes * 2 channels
//     asbd.mBytesPerFrame = 8;
//
//     // PCM: packet == frame
//     asbd.mBytesPerPacket = 8;
//
//     // ...
//
//     AudioQueueRef q;
//
//     AudioQueueNewOutput(&asbd, callback, NULL, NULL, NULL, 0, &q);
//
//     for (int i = 0; i < 3; i++) {
//         AudioQueueBufferRef buf;
//         AudioQueueAllocateBuffer(q, BUF * sizeof(float), &buf);
//         callback(NULL, q, buf);
//     }
//
//     AudioQueueStart(q, NULL);
//
//     sleep(5);
//
//     AudioQueueStop(q, true);
//     AudioQueueDispose(q, true);
// }
