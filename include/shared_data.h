#ifndef SHARED_DATA_H
#define SHARED_DATA_H

typedef struct
{
    int width;
    int height;
    int delay;
    int timeout;
    unsigned int seed;
    char *view_path;
    char *player_paths[9];
    int player_count;
} MasterConfig;

#endif