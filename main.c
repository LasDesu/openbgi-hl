#ifdef __APPLE__
#include <SDL/SDL.h>
#else
#include <SDL.h>
#endif

#include <SDL_ttf.h>

#include "engine.h"

TTF_Font *fnt;
SDL_Surface *sText;
int cursor_pos = 0;

void print_line( char *str )
{
    SDL_Color color = {255,255,255,0};
    SDL_Rect dest = {10, 10,0,0};
    SDL_Surface *sStr;
    dest.y += cursor_pos;

    sStr = TTF_RenderText_Solid( fnt, str, color );
    SDL_BlitSurface( sStr, NULL, sText, &dest );
    cursor_pos += sStr->h;
    SDL_FreeSurface( sStr );

    SDL_ExposeEvent expose = { SDL_VIDEOEXPOSE };
    SDL_PushEvent( &expose );
}

SDL_sem *key_sem;
SDL_mutex* sdl_mutex;

void display_put( const char *orig_str )
{
    char *str, *str_mem, *next_str, *last_space;
    int line_cnt;

    str_mem = str = strdup( orig_str );

    last_space = str;
    line_cnt = 0;

    while ( str[line_cnt] )
    {
        if ( str[line_cnt] == ' ' )
            last_space = str + line_cnt;

        if ( str[line_cnt] == '\n' )
        {
            next_str = str + line_cnt + 1;
            str[line_cnt] = '\0';
            print_line( str );
            str = next_str;
            line_cnt = 0;
            continue;
        }

        if ( line_cnt++ > (80 - 3) )
        {
            next_str = last_space + 1;
            *last_space = '\0';
            print_line( str );
            str = next_str;
            last_space = str;
            line_cnt = 0;
            continue;
        }
    }

    if ( line_cnt )
        print_line( str );

    free( str_mem );
}

void display_put_wait( const char *orig_str )
{
    display_put( orig_str );

    SDLMod key_mod = SDL_GetModState();
    if ( !(key_mod & KMOD_CTRL) )
        SDL_SemWait( key_sem );
}

void display_clear()
{
    cursor_pos = 0;
    SDL_FillRect(sText, 0, SDL_MapRGBA(sText->format, 0, 0, 0, 0x70));

    SDL_ExposeEvent expose = { SDL_VIDEOEXPOSE };
    SDL_PushEvent( &expose );
}

SDL_Surface* background;
image_res_t *background_img;

SDL_Surface* sprites[0x20] = { 0 };
sprite_info_t sprites_info[0x20] = { 0 };

int select_opts = 0;
int select_sizes[32][2] = { 0 };
int selected_opt = -1;

void set_sprite( int id, sprite_info_t *spr_info )
{
    SDL_Surface* old = sprites[id];
//printf("%s\n",__FUNCTION__);
    SDL_mutexP(sdl_mutex);

    if ( old )
    {
        SDL_FreeSurface( old );
        free( sprites_info[id].img->data );
        free( sprites_info[id].img );
        sprites[id] = 0;
    }

    if ( spr_info )
    {
        sprites_info[id] = *spr_info;
        SDL_Surface* temp_surf;
        temp_surf = SDL_CreateRGBSurfaceFrom( spr_info->img->data,
                        spr_info->img->width, spr_info->img->height,
                        spr_info->img->bpp, spr_info->img->width * (spr_info->img->bpp / 8),
                        0x00FF0000, 0x0000FF00, 0x000000FF, (spr_info->img->bpp > 24) ? 0xFF000000 : 0 );
        sprites[id] = SDL_DisplayFormatAlpha( temp_surf );
        SDL_FreeSurface( temp_surf );
    }

    SDL_mutexV(sdl_mutex);

    SDL_ExposeEvent expose = { SDL_VIDEOEXPOSE };
    SDL_PushEvent( &expose );
}

void set_background( image_res_t *img )
{
    int i;

    SDL_Surface* old = background;
    image_res_t *old_img = background_img;

    // hide all sprites
    for ( i = 0; i < 0x20; i ++ )
        set_sprite( i, NULL );

    SDL_mutexP(sdl_mutex);

    if ( img )
    {
        SDL_Surface* temp_surf;
        temp_surf = SDL_CreateRGBSurfaceFrom( img->data,
                        img->width, img->height, img->bpp, img->width * (img->bpp / 8),
                        0, 0, 0, 0);
        background = SDL_DisplayFormat( temp_surf );
        background_img = img;
        SDL_FreeSurface( temp_surf );
    }
    else
        background = NULL;

    if ( old )
    {
        SDL_FreeSurface( old );
        free( old_img->data );
        free( old_img );
    }

    SDL_mutexV(sdl_mutex);

    SDL_ExposeEvent expose = { SDL_VIDEOEXPOSE };
    SDL_PushEvent( &expose );
}

int show_select( void *data, int32_t opts[], int n )
{
    int i, sel;

    //select_sizes = calloc( sizeof(int), n );

    for ( i = 0; i < n; i ++ )
    {
        select_sizes[i][0] = cursor_pos + 10;
        display_put( (char *)data + opts[i] );
        select_sizes[i][1] = cursor_pos + 10;
    }

    select_opts = n;

    SDL_SemWait( key_sem );

    sel = selected_opt;
    selected_opt = -1;
    select_opts = 0;

    return sel;
}

void init_engine()
{
    SDL_Thread *thread;
    prepare_ops();
    key_sem = SDL_CreateSemaphore( 0 );
    sdl_mutex = SDL_CreateMutex();
    thread = SDL_CreateThread(script_thread, NULL);
}

int main ( int argc, char** argv )
{
    SDL_Surface* screen;
    //SDL_Rect dstrect;
    int i;
    int done;
    SDL_ExposeEvent expose = { SDL_VIDEOEXPOSE };

    // initialize SDL video
    if ( SDL_Init( SDL_INIT_VIDEO ) < 0 )
    {
        printf( "Unable to init SDL: %s\n", SDL_GetError() );
        return 1;
    }

    // make sure SDL cleans up before exit
    atexit(SDL_Quit);

    TTF_Init();
    atexit(TTF_Quit);

    // create a new window
    screen = SDL_SetVideoMode(800, 600, 16, SDL_HWSURFACE|SDL_DOUBLEBUF);
    if ( !screen )
    {
        printf("Unable to set 640x480 video: %s\n", SDL_GetError());
        return 1;
    }

    fnt = TTF_OpenFont( "DejaVuSansMono.ttf", 16 );
    sText = SDL_CreateRGBSurface( SDL_HWSURFACE|SDL_SRCALPHA, screen->w, screen->h, 32,
                                  0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000 );
    memset( sprites, 0, sizeof(sprites) );
    display_clear();
    init_engine();

    // program main loop
    done = 0;
    while (!done)
    {
        // message processing loop
        SDL_Event event;
        //while (SDL_PollEvent(&event))
        SDL_WaitEvent(&event);
        {
            // check for messages
            switch (event.type)
            {
                // exit if the window is closed
            case SDL_QUIT:
                done = 1;
                break;

                // check for keypresses
            case SDL_MOUSEMOTION:
                {
                    int i;
                    int y = event.button.y;
                    for ( i = 0; i < select_opts; i ++ )
                    {
                        if ( select_sizes[i][0] <= y &&
                             select_sizes[i][1] > y )
                        {
                            if ( selected_opt != i )
                                SDL_PushEvent( &expose );

                            selected_opt = i;
                            break;
                        }
                    }
                }
                break;

                // check for keypresses
            case SDL_KEYDOWN:
                    if ( !select_opts )
                        SDL_SemPost( key_sem );
                    else
                    {
                        switch ( event.key.keysym.sym )
                        {
                            case SDLK_UP:
                                selected_opt --;
                                if ( selected_opt < 0 )
                                    selected_opt = 0;
                                SDL_PushEvent( &expose );
                                break;
                            case SDLK_DOWN:
                                selected_opt ++;
                                if ( selected_opt >= select_opts )
                                    selected_opt = select_opts - 1;
                                SDL_PushEvent( &expose );
                                break;
                            case SDLK_RETURN:
                                if ( selected_opt >= 0 && selected_opt < select_opts )
                                {
                                   SDL_SemPost( key_sem );
                                }
                                break;
                        }
                    }
                    break;

            case SDL_MOUSEBUTTONDOWN:
                    /*if ( (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_RETURN) ||
                         (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == 1) )*/
                    if ( !select_opts )
                        SDL_SemPost( key_sem );
                    else
                    {
                        int i;
                        int y = event.button.y;
                        for ( i = 0; i < select_opts; i ++ )
                        {
                            if ( select_sizes[i][0] <= y &&
                                 select_sizes[i][1] > y )
                            {
                                selected_opt = i;
                                SDL_SemPost( key_sem );
                                break;
                            }
                        }
                    }
                    break;

            case SDL_VIDEOEXPOSE:
            {

                // DRAWING STARTS HERE
        SDL_mutexP(sdl_mutex);

        // clear screen
        SDL_FillRect(screen, 0, SDL_MapRGB(screen->format, 0, 0, 0));

        if ( background )
            SDL_BlitSurface(background, 0, screen, 0);

        i = 0x20;
        while ( i-- )
        {
            if ( sprites[i] )
            {
                SDL_Rect dstrect;
                dstrect.x = (800/2) + sprites_info[i].x - (sprites[i]->w / 2);
                dstrect.y = - sprites_info[i].y;
                //printf("%d %d\n",dstrect.x,dstrect.y);
                //SDL_SetAlpha( sprites[i], SDL_SRCALPHA, 0x40 );
                SDL_BlitSurface( sprites[i], 0, screen, &dstrect );
            }
        }

        if ( selected_opt >= 0 )
        {
            SDL_Rect rect;

            rect.x = 0;
            rect.w = screen->w;
            rect.y = select_sizes[selected_opt][0];
            rect.h = select_sizes[selected_opt][1] - select_sizes[selected_opt][0];

            SDL_FillRect(screen, &rect, SDL_MapRGBA(screen->format, 0x80, 0x00, 0x00, 0x80) );
        }
        SDL_BlitSurface(sText, 0, screen, 0);


        // DRAWING ENDS HERE
        SDL_mutexV(sdl_mutex);

        // finally, update the screen :)
        SDL_Flip(screen);

            }
            } // end switch
        } // end of message processing


    } // end main loop

    // free loaded bitmap
    if ( background )
        SDL_FreeSurface(background);

    TTF_CloseFont( fnt );

    SDL_DestroyMutex(sdl_mutex);

    // all is well ;)
    printf("Exited cleanly\n");

    return 0;
}
