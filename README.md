# cpod

minimal c audio player

## Platform

- macOS only

## Dependencies

- FFmpeg libraries:
    - libavformat
    - libavcodec
    - libavutil
    - libswresample

```sh
brew install ffmpeg
```

## Build from source

```sh
git clone https://github.com/nlkli/cpod
cd cpod
chmod +x build.sh
./build.sh && ./cpod -i ./resources
```

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

```text
minimal c audio player
https://github.com/nlkli/cpod
Options:
  -i, --input <path>   Input playlist path (dir or file)
  -h, --help           Show this help message
  -V, --version        Show this help message
Keymaps:
  j    Next
  k    Prev
  J    Rand
  p    Pause
  +    Vol up
  -    Vol down
```

## Usage

### Play from playlist file

```sh
cpod -i ./resources/.playlist
```

### Play from directory

```sh
cpod -i ./resources
```
