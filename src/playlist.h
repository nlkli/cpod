#pragma once
#include <stddef.h>

typedef struct {
    char *title;
    char *artist;
    char *album;
    size_t duration;
} PL_TMetaData;

typedef struct {
    char *uri;
    PL_TMetaData *metadata;
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

int PL_track_name(PL_Track *track, char *name, size_t name_size);

void PL_init(PL_PlayList *pl, int cap);

PL_Track *PL_alloc_track(PL_PlayList *pl);

void PL_remove_track(PL_PlayList *pl, int ti);

void PL_clear(PL_PlayList *pl);

void PL_free(PL_PlayList *pl);
