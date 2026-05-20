# cpod

minimal c audio player

## Resources

- [FFmpeg Documentation](https://ffmpeg.org/doxygen/8.0/index.html)
- Apple Core Audio [Documentation Archive](https://developer.apple.com/library/archive/navigation/)

## Road map

- [x] ffmpeg decoder (PCM)
- [x] frame resampling
- [x] Core Audio (play)
- [x] PlayList
- [x] CLI args

## Help message

```c
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
```
