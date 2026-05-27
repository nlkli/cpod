#include <dirent.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <termios.h>
#include <time.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/log.h>
#include <libswresample/swresample.h>

#include <AudioToolbox/AudioToolbox.h>

#define DEFAULT_SAMPLE_RATE 48000
#define DEFAULT_FRAMES_PER_BUF 4096

#define BUFFER_COUNT 3
#define VOLUME_STEP 0.05

typedef struct {
    char **paths;
    int len;
    int cap;
    int prevpi;
} PlayList;

void pl_init(PlayList *pl, int cap) {
    cap = (!cap) ? 1 : cap;
    pl->paths = malloc(cap * sizeof(char *));
    pl->len = 0;
    pl->cap = cap;
    pl->prevpi = -1;
}

void pl_push(PlayList *pl, const char *name) {
    if (pl->len >= pl->cap) {
        pl->cap *= 2;
        pl->paths = realloc(pl->paths, pl->cap * sizeof(char *));
    }

    pl->paths[pl->len] = malloc(strlen(name) + 1);
    if (pl->paths[pl->len]) {
        strcpy(pl->paths[pl->len], name);
        pl->len++;
    }
}

int __compare_strings(const void *a, const void *b) {
    return strcmp(*(const char **)a, *(const char **)b);
}

void pl_sort(PlayList *pl) {
    qsort(pl->paths, pl->len, sizeof(char *), __compare_strings);
}

int pl_from_dir(PlayList *pl, const char *path) {
    DIR *dir = opendir(path);
    if (!dir)
        return -1;

    struct dirent *entry;
    struct stat st;

    while ((entry = readdir(dir)) != NULL) {
        char file_path[1024];
        snprintf(file_path, sizeof(file_path), "%s/%s", path, entry->d_name);
        if (stat(file_path, &st) != 0)
            continue;
        // ignore dot files
        if (entry->d_name[0] == '.')
            continue;
        if (S_ISREG(st.st_mode)) {
            pl_push(pl, file_path);
        }
    }

    closedir(dir);

    return 1;
}

int pl_from_file(PlayList *pl, const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp)
        return -1;

    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = 0;
        pl_push(pl, line);
    }

    fclose(fp);

    return 1;
}

int pl_from_any_path(PlayList *pl, const char *path) {
    struct stat st;

    if (stat(path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            return pl_from_dir(pl, path);
        } else if (S_ISREG(st.st_mode)) {
            return pl_from_file(pl, path);
        }
        return -1;
    } else {
        perror("stat");
        return -1;
    }

    return 1;
}

char *pl_pick(PlayList *pl, int i) {
    if (i >= pl->len || i < 0)
        return NULL;
    pl->prevpi = i;
    return pl->paths[i];
}

char *pl_rand_pick(PlayList *pl) {
    if (pl->len == 0)
        return NULL;
    return pl_pick(pl, rand() % pl->len);
}

char *pl_next_pick(PlayList *pl) {
    int pi = 0;
    if (pl->prevpi + 1 < pl->len)
        pi = pl->prevpi + 1;
    return pl_pick(pl, pi);
}

char *pl_priv_pick(PlayList *pl) {
    int pi = pl->len - 1;
    if (pl->prevpi - 1 > 0)
        pi = pl->prevpi - 1;
    return pl_pick(pl, pi);
}

void pl_free(PlayList *pl) {
    for (int i = 0; i < pl->len; i++) {
        free(pl->paths[i]);
    }

    free(pl->paths);

    pl->paths = NULL;
    pl->len = 0;
    pl->cap = 0;
    pl->prevpi = 0;
}

typedef struct {
    int frames_per_buf;
    AudioQueueRef queue;
    AVFormatContext *fmt_ctx;
    const AVCodec *codec;
    int audio_stream_idx;
    AVCodecContext *codec_ctx;
    SwrContext *swr;
    AVPacket *packet;
    AVFrame *frame;
    int is_auto_play;
    int is_pause;
    float volume; // 0.0 - 1.0
    int64_t last_pts;
    int done;
} PlayerState;

int next_frame(PlayerState *ps, float *buf, int cap) {
    if (ps->done)
        return 0;
    for (;;) {
        int ret = avcodec_receive_frame(ps->codec_ctx, ps->frame);

        if (ret == 0) {
            uint8_t *out_planes[1] = {(uint8_t *)buf};
            int out_samples = swr_convert(
                ps->swr,
                out_planes, cap,
                (const uint8_t **)ps->frame->extended_data, ps->frame->nb_samples);
            if (ps->volume < 1.) {
                for (int i = 0; i < out_samples * 2; i++) {
                    buf[i] *= ps->volume;
                }
            }
            ps->last_pts = ps->frame->pts;
            av_frame_unref(ps->frame);
            return out_samples;
        }

        if (ret != AVERROR(EAGAIN)) {
            ps->done = 1;
            return 0;
        }

        do {
            ret = av_read_frame(ps->fmt_ctx, ps->packet);
            if (ret < 0) {
                avcodec_send_packet(ps->codec_ctx, NULL);
                int out_samples;
                while (avcodec_receive_frame(ps->codec_ctx, ps->frame) == 0) {
                    uint8_t *out_planes[1] = {(uint8_t *)buf};
                    out_samples = swr_convert(
                        ps->swr,
                        out_planes, cap,
                        (const uint8_t **)ps->frame->extended_data, ps->frame->nb_samples);
                    if (ps->volume < 1.) {
                        for (int i = 0; i < out_samples * 2; i++) {
                            buf[i] *= ps->volume;
                        }
                    }
                    av_frame_unref(ps->frame);
                }
                uint8_t *out_planes[1] = {(uint8_t *)buf};
                swr_convert(ps->swr, out_planes, cap, NULL, 0);
                ps->done = 1;
                return out_samples;
            }
        } while (ps->packet->stream_index != ps->audio_stream_idx);

        avcodec_send_packet(ps->codec_ctx, ps->packet);
        av_packet_unref(ps->packet);
    }
}

int next_samples_fill(PlayerState *ps, float *buf, int want_frames) {
    int filled = 0;

    while (filled < want_frames) {
        int remaining = want_frames - filled;
        float *dst = buf + filled * 2;

        int got = next_frame(ps, dst, remaining);
        if (got <= 0)
            break;

        filled += got;
    }

    return filled;
}

static void queue_callback(void *state, AudioQueueRef queue, AudioQueueBufferRef buf) {
    PlayerState *ps = (PlayerState *)state;

    float *dst = (float *)buf->mAudioData;
    int samples = next_samples_fill(ps, dst, ps->frames_per_buf);

    if (samples <= 0) {
        AudioQueueStop(queue, false);
        buf->mAudioDataByteSize = 0;
        AudioQueueEnqueueBuffer(queue, buf, 0, NULL);
        return;
    }

    buf->mAudioDataByteSize = samples * 2 * sizeof(float);
    AudioQueueEnqueueBuffer(queue, buf, 0, NULL);
}

int ps_init(PlayerState *ps, float sample_rate, int frames_per_buf, int is_auto_play, float init_volume) {
    AudioStreamBasicDescription fmt = {
        .mSampleRate = sample_rate,
        .mFormatID = kAudioFormatLinearPCM,
        .mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked,
        .mChannelsPerFrame = 2,
        .mBitsPerChannel = 32,
        .mBytesPerFrame = 2 * sizeof(float),
        .mFramesPerPacket = 1,
        .mBytesPerPacket = 2 * sizeof(float),
    };

    int ret = AudioQueueNewOutput(&fmt, queue_callback, ps, NULL, NULL, 0, &ps->queue);
    if (ret < 0)
        return ret;

    ps->frames_per_buf = frames_per_buf;
    ps->last_pts = AV_NOPTS_VALUE;
    ps->is_auto_play = is_auto_play;
    ps->volume = init_volume;
    if (ps->volume > 1.)
        ps->volume = 1.;
    if (ps->volume < 0.)
        ps->volume = 0.;
    ps->done = 1;

    return 1;
}

int ps_load(PlayerState *ps, const char *path) {
    if (ps->fmt_ctx)
        avformat_close_input(&ps->fmt_ctx);

    int ret;

    ret = avformat_open_input(&ps->fmt_ctx, path, NULL, NULL);
    if (ret < 0)
        return ret;

    ret = avformat_find_stream_info(ps->fmt_ctx, NULL);
    if (ret < 0)
        return ret;

    ret = av_find_best_stream(ps->fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, &ps->codec, 0);
    if (ret < 0)
        return ret;
    ps->audio_stream_idx = ret;

    if (ps->codec_ctx)
        avcodec_free_context(&ps->codec_ctx);

    ps->codec_ctx = avcodec_alloc_context3(ps->codec);
    if (!ps->codec_ctx)
        return -1;

    avcodec_parameters_to_context(
        ps->codec_ctx,
        ps->fmt_ctx->streams[ps->audio_stream_idx]->codecpar);

    ret = avcodec_open2(ps->codec_ctx, ps->codec, NULL);
    if (ret < 0)
        return ret;
    avcodec_flush_buffers(ps->codec_ctx);

    if (ps->swr)
        swr_free(&ps->swr);

    AVChannelLayout out_ch_layout = AV_CHANNEL_LAYOUT_STEREO;
    ret = swr_alloc_set_opts2(&ps->swr,
                              &out_ch_layout, AV_SAMPLE_FMT_FLT, 48000,
                              &ps->codec_ctx->ch_layout, ps->codec_ctx->sample_fmt,
                              ps->codec_ctx->sample_rate,
                              0, NULL);
    if (ret < 0)
        return ret;

    ret = swr_init(ps->swr);
    if (ret < 0)
        return ret;

    if (ps->packet)
        av_packet_free(&ps->packet);
    if (ps->frame)
        av_frame_free(&ps->frame);

    ps->packet = av_packet_alloc();
    ps->frame = av_frame_alloc();

    ps->last_pts = AV_NOPTS_VALUE;
    ps->done = 0;

    // TODO

    printf("\n\n[Track] %s\n", path);
    printf("  Format:      %s\n", ps->fmt_ctx->iformat->long_name);

    if (ps->fmt_ctx->duration != AV_NOPTS_VALUE) {
        int64_t dur_sec = ps->fmt_ctx->duration / AV_TIME_BASE;
        printf("  Duration:    %" PRId64 ":%02" PRId64 ":%02" PRId64 "\n",
               dur_sec / 3600, (dur_sec % 3600) / 60, dur_sec % 60);
    }

    if (ps->fmt_ctx->bit_rate > 0)
        printf("  Bitrate:     %" PRId64 " kb/s\n", ps->fmt_ctx->bit_rate / 1000);

    AVStream *audio_stream = ps->fmt_ctx->streams[ps->audio_stream_idx];
    AVCodecParameters *par = audio_stream->codecpar;

    printf("\n[Audio Stream]\n");
    printf("  Codec:       %s\n", ps->codec->long_name);

    if (par->bit_rate > 0)
        printf("  Bitrate:     %" PRId64 " kb/s\n", par->bit_rate / 1000);

    printf("  Sample rate: %d Hz\n", par->sample_rate);

    char ch_buf[64];
    av_channel_layout_describe(&par->ch_layout, ch_buf, sizeof(ch_buf));
    printf("  Channels:    %d (%s)\n", par->ch_layout.nb_channels, ch_buf);
    printf("  Format:      %s (%d-bit)\n",
           av_get_sample_fmt_name(par->format),
           av_get_bytes_per_sample(par->format) * 8);

    const char *meta_keys[] = {"title", "artist", "album", "date", "track", "genre", NULL};
    int has_meta = 0;
    for (int i = 0; meta_keys[i]; i++) {
        AVDictionaryEntry *tag = av_dict_get(audio_stream->metadata, meta_keys[i], NULL, 0);
        if (!tag)
            tag = av_dict_get(ps->fmt_ctx->metadata, meta_keys[i], NULL, 0);
        if (tag) {
            if (!has_meta) {
                printf("\n[Metadata]\n");
                has_meta = 1;
            }
            printf("  %-10s %s\n", tag->key, tag->value);
        }
    }

    printf("\n");

    return 0;
}

void ps_play(PlayerState *ps) {
    if (!ps->is_pause || ps->done)
        return;
    uint32_t buf_bytes = ps->frames_per_buf * 2 * sizeof(float);
    for (int i = 0; i < BUFFER_COUNT; i++) {
        AudioQueueBufferRef buf;
        AudioQueueAllocateBuffer(ps->queue, buf_bytes, &buf);
        queue_callback(ps, ps->queue, buf);
    }

    AudioQueueStart(ps->queue, NULL);
    ps->is_pause = 0;
}

void ps_pause(PlayerState *ps) {
    if (ps->is_pause)
        return;
    AudioQueueStop(ps->queue, true);
    ps->is_pause = 1;
}

float ps_progress(PlayerState *ps) {
    if (ps->fmt_ctx->duration <= 0)
        return 0.0;

    int64_t pts = ps->last_pts;
    if (pts == AV_NOPTS_VALUE)
        return 0.0;

    AVStream *stream = ps->fmt_ctx->streams[ps->audio_stream_idx];
    float current = pts * av_q2d(stream->time_base);
    float total = ps->fmt_ctx->duration / (float)AV_TIME_BASE;

    return current / total;
}

void ps_free(PlayerState *ps) {
    AudioQueueStop(ps->queue, true);
    AudioQueueDispose(ps->queue, true);

    avcodec_free_context(&ps->codec_ctx);
    avformat_close_input(&ps->fmt_ctx);
    swr_free(&ps->swr);
    av_packet_free(&ps->packet);
    av_frame_free(&ps->frame);
}

typedef enum {
    PE_NONE = 0,
    PE_PAUSE,
    PE_VOLUP,
    PE_VOLDOWN,
    PE_RAND,
    PE_NEXT,
    PE_PREV,
} PlayerEvent;

int ps_handle_event(PlayerState *ps, PlayerEvent e, PlayList *pl) {
    int ret = 1;
    if (e == PE_NONE)
        return ret;
    switch (e) {
    case PE_PAUSE:
        if (ps->is_pause)
            ps_play(ps);
        else
            ps_pause(ps);
        break;
    case PE_VOLUP:
        ps->volume += VOLUME_STEP;
        if (ps->volume > 1.)
            ps->volume = 1.;
        break;
    case PE_VOLDOWN:
        ps->volume -= VOLUME_STEP;
        if (ps->volume < 0.)
            ps->volume = 0.;
        break;
    case PE_RAND:
        ps_pause(ps);
        ret = ps_load(ps, pl_rand_pick(pl));
        ps_play(ps);
        break;
    case PE_NEXT:
        ps_pause(ps);
        ret = ps_load(ps, pl_next_pick(pl));
        ps_play(ps);
        break;
    case PE_PREV:
        ps_pause(ps);
        ret = ps_load(ps, pl_priv_pick(pl));
        ps_play(ps);
        break;
    default:
        break;
    }
    return ret;
}

static struct termios orig_termios;
static int terminal_initialized = 0;

void enter_raw_mode() {
    struct termios raw;

    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) {
        perror("tcgetattr");
        exit(1);
    }
    write(STDOUT_FILENO, "\x1b[?1049h", 8);
    terminal_initialized = 1;
    raw = orig_termios;

    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    // raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        perror("tcsetattr");
        exit(1);
    }
}

void exit_raw_mode() {
    if (terminal_initialized) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
        write(STDOUT_FILENO, "\x1b[?1049l", 8);
        terminal_initialized = 0;
    }
}

typedef enum {
    KEY_NONE = 0,
    KEY_CHAR,
    KEY_CTRL,
    KEY_ESCAPE,
    KEY_ENTER,
    KEY_BACKSPACE,
    KEY_UP,
    KEY_DOWN,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_UNKNOWN,
} KeyType;

typedef struct {
    KeyType type;
    char ch; // KEY_CHAR or KEY_CTRL
} KeyEvent;

int read_key(KeyEvent *ev) {
    char buf[8];
    int n;

    memset(ev, 0, sizeof(*ev));

    n = read(STDIN_FILENO, buf, 1);
    if (n <= 0) {
        ev->type = KEY_NONE;
        return n;
    }

    if (buf[0] == 0x1B) {
        int m = read(STDIN_FILENO, buf + 1, sizeof(buf) - 1);
        if (m <= 0) {
            ev->type = KEY_ESCAPE;
            return 1;
        }

        if (buf[1] == '[') {
            if (buf[2] == 'A')
                ev->type = KEY_UP;
            else if (buf[2] == 'B')
                ev->type = KEY_DOWN;
            else if (buf[2] == 'C')
                ev->type = KEY_RIGHT;
            else if (buf[2] == 'D')
                ev->type = KEY_LEFT;
            else
                ev->type = KEY_UNKNOWN;

        } else {
            ev->type = KEY_UNKNOWN;
        }
        return 1;
    }

    if (buf[0] == '\r' || buf[0] == '\n') {
        ev->type = KEY_ENTER;
        return 1;
    }

    if (buf[0] == 127 || buf[0] == 8) {
        ev->type = KEY_BACKSPACE;
        return 1;
    }

    if (buf[0] >= 1 && buf[0] <= 26) {
        ev->type = KEY_CTRL;
        ev->ch = 'a' + buf[0] - 1;
        return 1;
    }

    ev->type = KEY_CHAR;
    ev->ch = (char)buf[0];

    return 1;
}

typedef struct {
    char *input;
} Args;

#define VERSION "cpod 0.1.0 [https://github.com/nlkli/cpod]"
static const char *HELP_MSG_LINES[] = {
    "",
    "minimal c audio player",
    "https://github.com/nlkli/cpod",
    "Options:",
    "  -i, --input <path>   Input playlist path (dir or file)",
    "  -h, --help           Show this help message",
    "  -V, --version        Show this help message",
    "Keymaps:",
    "  j    Next",
    "  k    Prev",
    "  J    Rand",
    "  p    Pause",
    "  +    Vol up",
    "  -    Vol down",
    "",
    NULL};
static void print_help_msg() {
    for (int l = 0; HELP_MSG_LINES[l] != NULL; l++) {
        printf("%s\n", HELP_MSG_LINES[l]);
    }
}

void parse_args(Args *args, int argc, char *argv[]) {
    uint8_t last = 0;
    for (int i = 0; i < argc; i++) {
        char *arg = argv[i];
        int n = strlen(arg);
        if (n > 2 && strncmp(arg, "--", 2) == 0) {
            if (strcmp(arg + 2, "input") == 0) {
                last = 'i';
            }
            if (strcmp(arg + 2, "help") == 0) {
                print_help_msg();
                exit(0);
            }
            if (strcmp(arg + 2, "version") == 0) {
                printf("%s\n", VERSION);
                exit(0);
            }
        } else if (arg[0] == '-') {
            for (int j = 1; j < n; j++) {
                if (arg[j] == 'i')
                    last = 'i';
                if (arg[j] == 'h') {
                    print_help_msg();
                    exit(0);
                }
                if (arg[j] == 'V') {
                    printf("%s\n", VERSION);
                    exit(0);
                }
            }
        } else {
            if (last) {
                if (last == 'i')
                    args->input = arg;
                last = 0;
            }
        }
    }
}

int main(int argc, char *argv[]) {
    Args args = {0};
    parse_args(&args, argc, argv);

    if (!args.input) {
        perror("input playlist path required");
        exit(1);
    }

    srand(time(NULL));

    av_log_set_level(AV_LOG_QUIET);

    enter_raw_mode();
    write(STDOUT_FILENO, "\x1b[2J\x1b[H", 7);

    PlayerState ps = {0};
    ps_init(&ps, DEFAULT_SAMPLE_RATE, DEFAULT_FRAMES_PER_BUF, 1, 0.9);

    PlayList pl = {0};
    pl_init(&pl, 0);

    pl_from_any_path(&pl, args.input);
    pl_sort(&pl);

    int n = 0;
    PlayerEvent pe = PE_NONE;
    KeyEvent ke;
    for (;;) {
        read_key(&ke);

        if (ke.type == KEY_CHAR) {
            if (ke.ch == 'q')
                break;
            switch (ke.ch) {
            case 'j':
                pe = PE_NEXT;
                break;
            case 'k':
                pe = PE_PREV;
                break;
            case 'J':
                pe = PE_RAND;
                break;
            case 'p':
                pe = PE_PAUSE;
                break;
            case '+':
                pe = PE_VOLUP;
                break;
            case '-':
                pe = PE_VOLDOWN;
                break;
            }
        }

        ps_handle_event(&ps, pe, &pl);

        if (ps.done && ps.is_auto_play) {
            // wait for last buf
            usleep(300000);
            ps_handle_event(&ps, PE_NEXT, &pl);
        }

        if (n % 10 == 0 && !ps.is_pause) {
            float progress = ps_progress(&ps);
            int filled = (int)(progress * 30);
            printf("\r  [");
            for (int i = 0; i < 30; i++)
                putchar(i < filled ? '#' : '-');
            printf("]  %3d%%", (int)(progress * 100));
            fflush(stdout);
        }

        usleep(20000);

        pe = PE_NONE;
        n++;
    }

    exit_raw_mode();

    ps_free(&ps);
    pl_free(&pl);

    return 0;
}
