// #include <cstring>
#include <dirent.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// #include "playlist.h"

typedef struct {
    char *title;
    char *artist;
    char *album;
    size_t duration;
} PL_TMetaData;

typedef struct {
    char *uri;
    PL_TMetaData *metadata;
    char *fullname;
} PL_Track;

typedef struct {
    char *src;
    PL_Track *tracks;
    int len;
    int cap;
    int curr_ti;
} PL_PlayList;

void PL_track_init(PL_Track *track, const char *uri, PL_TMetaData *metadata);

PL_TMetaData *PL_metadata_new(
    const char *title,
    const char *artist,
    const char *album,
    size_t duration);

void PL_metadata_free(PL_TMetaData *md);

void PL_track_free(PL_Track *track);

size_t PL_track_fullname(
    const PL_Track *track,
    char *buf,
    size_t buf_size);

void PL_init(PL_PlayList *pl, int cap);

PL_Track *PL_alloc_track(PL_PlayList *pl);

void PL_remove_track(PL_PlayList *pl, int ti);

void PL_clear(PL_PlayList *pl);

void PL_free(PL_PlayList *pl);

void PL_track_init(PL_Track *track, const char *uri, PL_TMetaData *metadata) {
    track->uri = uri ? strdup(uri) : NULL;
    track->metadata = metadata;
    if (track->metadata) {
        return;
    }
}

PL_TMetaData *PL_metadata_new(
    const char *title,
    const char *artist,
    const char *album,
    size_t duration) {
    PL_TMetaData *metadata = malloc(sizeof(PL_TMetaData));
    metadata->title = title ? strdup(title) : NULL;
    metadata->artist = artist ? strdup(artist) : NULL;
    metadata->album = album ? strdup(album) : NULL;
    metadata->duration = duration;
    return metadata;
}

void PL_metadata_free(PL_TMetaData *md) {
    if (!md)
        return;
    free(md->title);
    free(md->artist);
    free(md->album);
    free(md);
}

void PL_track_free(PL_Track *track) {
    if (!track)
        return;
    free(track->uri);
    PL_metadata_free(track->metadata);
    free(track->fullname);
}

size_t PL_track_fullname(
    const PL_Track *track,
    char *buf,
    size_t buf_size) {
    const char *start = "unknown";
    size_t len = 7;

    if (track) {
        const PL_TMetaData *md = track->metadata;

        // 1. metadata: artist - title
        if (md && md->artist && md->title) {
            int n = snprintf(NULL, 0, "%s - %s", md->artist, md->title);
            if (n < 0)
                return 0;

            len = (size_t)n;

            if (buf && buf_size > 0)
                snprintf(buf, buf_size, "%s - %s", md->artist, md->title);

            return len;
        }

        // 2. metadata: title only
        if (md && md->title) {
            start = md->title;
            len = strlen(start);
        }
        // 3. uri fallback
        else if (track->uri) {
            const char *uri = track->uri;
            size_t n = strlen(uri);

            // trim trailing '/'
            while (n > 0 && uri[n - 1] == '/')
                n--;

            if (n > 0) {
                const char *last = NULL;

                for (size_t i = n; i > 0; --i) {
                    if (uri[i - 1] == '/') {
                        last = uri + i;
                        break;
                    }
                }

                if (last) {
                    start = last;
                    len = n - (size_t)(last - uri);
                } else {
                    start = uri;
                    len = n;
                }
            } else {
                start = uri;
                len = strlen(uri);
            }
        }
    }

    // safe copy (like snprintf behaviour)
    if (buf && buf_size > 0) {
        size_t copy_len = (len >= buf_size)
                              ? buf_size - 1
                              : len;

        memcpy(buf, start, copy_len);
        buf[copy_len] = '\0';
    }

    return len;
}

void PL_init(PL_PlayList *pl, int cap) {
    cap = (!cap) ? 1 : cap;
    pl->tracks = malloc(cap * sizeof(PL_Track));
    pl->len = 0;
    pl->cap = cap;
    pl->curr_ti = -1;
}

PL_Track *PL_alloc_track(PL_PlayList *pl) {
    if (pl->len >= pl->cap) {
        pl->cap *= 2;
        PL_Track *new_tracks =
            realloc(pl->tracks, pl->cap * sizeof(PL_Track));
        if (!new_tracks)
            return NULL;
        pl->tracks = new_tracks;
    }
    PL_Track *track = &pl->tracks[pl->len];
    pl->len++;
    return track;
}

void PL_remove_track(PL_PlayList *pl, int ti) {
    PL_track_free(&pl->tracks[ti]);
    for (int i = ti; i < pl->len - 1; i++) {
        pl->tracks[i] = pl->tracks[i + 1];
    }
    pl->len--;
}

void PL_clear(PL_PlayList *pl) {
    for (int i = 0; i < pl->len; i++) {
        PL_track_free(&pl->tracks[i]);
    }
    pl->len = 0;
    pl->curr_ti = -1;
}

void PL_free(PL_PlayList *pl) {
    PL_clear(pl);
    free(pl->tracks);
    pl->tracks = NULL;
    pl->cap = 0;
    pl->len = 0;
    pl->curr_ti = -1;
}

// static int __compare_track_title(const void *a, const void *b) {
//     return strcmp(*(PL_Track *)a, (PL_Track *)b->title);
// }
//
// void PL_sort(PL_PlayList *pl) {
//     qsort(pl->tracks, pl->len, sizeof(char *), __compare_strings);
// }
//
// int PL_from_dir(PL_PlayList *pl, const char *path) {
//     DIR *dir = opendir(path);
//     if (!dir)
//         return -1;
//
//     struct dirent *entry;
//     struct stat st;
//
//     while ((entry = readdir(dir)) != NULL) {
//         char file_path[1024];
//         snprintf(file_path, sizeof(file_path), "%s/%s", path, entry->d_name);
//         if (stat(file_path, &st) != 0)
//             continue;
//         // ignore dot files
//         if (entry->d_name[0] == '.')
//             continue;
//         if (S_ISREG(st.st_mode)) {
//             PL_push(pl, file_path);
//         }
//     }
//
//     closedir(dir);
//
//     return 1;
// }
//
// int PL_from_file(PL_PlayList *pl, const char *path) {
//     FILE *fp = fopen(path, "r");
//     if (!fp)
//         return -1;
//
//     char line[1024];
//     while (fgets(line, sizeof(line), fp)) {
//         line[strcspn(line, "\n")] = 0;
//         PL_push(pl, line);
//     }
//
//     fclose(fp);
//
//     return 1;
// }
//
// int PL_from_any_path(PL_PlayList *pl, const char *path) {
//     struct stat st;
//
//     if (stat(path, &st) == 0) {
//         if (S_ISDIR(st.st_mode)) {
//             return PL_from_dir(pl, path);
//         } else if (S_ISREG(st.st_mode)) {
//             return PL_from_file(pl, path);
//         }
//         return -1;
//     } else {
//         perror("stat");
//         return -1;
//     }
//
//     return 1;
// }
//
// char *PL_pick(PL_PlayList *pl, int i) {
//     if (i >= pl->len || i < 0)
//         return NULL;
//     pl->curr_ti = i;
//     return pl->tracks[i];
// }
//
// char *PL_rand_pick(PL_PlayList *pl) {
//     if (pl->len == 0)
//         return NULL;
//     return PL_pick(pl, rand() % pl->len);
// }
//
// char *PL_next_pick(PL_PlayList *pl) {
//     int pi = 0;
//     if (pl->curr_ti + 1 < pl->len)
//         pi = pl->curr_ti + 1;
//     return PL_pick(pl, pi);
// }
//
// char *PL_priv_pick(PL_PlayList *pl) {
//     int pi = pl->len - 1;
//     if (pl->curr_ti - 1 > 0)
//         pi = pl->curr_ti - 1;
//     return PL_pick(pl, pi);
// }
//
// void PL_free(PL_PlayList *pl) {
//     for (int i = 0; i < pl->len; i++) {
//         free(pl->tracks[i]);
//     }
//
//     free(pl->tracks);
//
//     pl->tracks = NULL;
//     pl->len = 0;
//     pl->cap = 0;
//     pl->curr_ti = 0;
// }
//
// int main() {
//     printf("Hello, world!\n");
//     return 0;
// }
