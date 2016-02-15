#ifndef BGI_ENGINE_H
#define BGI_ENGINE_H

#define BGI_ROOT "/mnt/heap/programming/openBGI/Kira Kira/"

#include <stdio.h>
#include <stdint.h>

void prepare_ops();
int script_thread(void *unused);

typedef struct
{
    FILE *fd;
    uint32_t num;
    struct
    {
        char name[0x10];
        uint32_t off;
        uint32_t size;
        char unkn[0x10-4-4];
    } *res;
} resource_arc_t;

typedef struct
{
    int width;
    int height;
    int bpp;
    uint8_t *data;
} image_res_t;

typedef struct
{
    int x;
    int y;
    image_res_t *img;
} sprite_info_t;

void display_put( const char *str );
void display_clear();
void set_background( image_res_t *img );

resource_arc_t *open_resource( const char *fname );
void close_resource( resource_arc_t *res );
void *get_resource( resource_arc_t *res, const char *name );

#endif /* BGI_ENGINE_H */
