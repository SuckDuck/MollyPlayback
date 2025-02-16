#include <stdio.h>
#include <stdint.h>
#include <sys/time.h>
#include <SDL2/SDL.h>
#include <SDL_ttf.h>
#include "microui.h"
#include "tinyfiledialogs.h"
#include "sprite_sheet.h"
#include "monomaniac.h"

#define MIN_WINDOW_W    455
#define MIN_WINDOW_H    430
#define TAPE_W          377
#define TAPE_H          267
#define PIVOT_W          73
#define PIVOT_H          73
#define PIVOT_X_OFFSET   -5
#define PIVOT_Y_OFFSET   -5
#define PIVOT_X_DISTANCE 72
#define WHEEL_W          99
#define WHEEL_H          99

#define MU_WIN_W         455
#define MU_WIN_H         100
#define MU_WIN_Y_OFFSET   55
#define BUTTONS_BORDER     2

SDL_Window*  window          = NULL;
SDL_Surface* main_surface    = NULL;
SDL_Surface* sprite_sheet    = NULL;
SDL_Surface* bw_sprite_sheet = NULL;
SDL_Surface* sprite_sets[2];
TTF_Font*    main_font       = NULL;


SDL_Rect tape = { .w=TAPE_W, .h=TAPE_H, .x=0, .y=0 };
SDL_Rect wheel = { .w=WHEEL_W, .h=WHEEL_H, .x=TAPE_W+(PIVOT_W*2), .y=0 };
SDL_Rect pivot[] = {
    { .w=PIVOT_W, .h=PIVOT_H, .x=TAPE_W+(PIVOT_W*0), .y=PIVOT_H*0 },
    { .w=PIVOT_W, .h=PIVOT_H, .x=TAPE_W+(PIVOT_W*0), .y=PIVOT_H*1 },
    { .w=PIVOT_W, .h=PIVOT_H, .x=TAPE_W+(PIVOT_W*0), .y=PIVOT_H*2 },
    { .w=PIVOT_W, .h=PIVOT_H, .x=TAPE_W+(PIVOT_W*0), .y=PIVOT_H*3 },
    { .w=PIVOT_W, .h=PIVOT_H, .x=TAPE_W+(PIVOT_W*1), .y=PIVOT_H*0 },
    { .w=PIVOT_W, .h=PIVOT_H, .x=TAPE_W+(PIVOT_W*1), .y=PIVOT_H*1 },
    { .w=PIVOT_W, .h=PIVOT_H, .x=TAPE_W+(PIVOT_W*1), .y=PIVOT_H*2 },
    { .w=PIVOT_W, .h=PIVOT_H, .x=TAPE_W+(PIVOT_W*1), .y=PIVOT_H*3 },
};

mu_Context mu_ctx;
static const char button_map[256] = {
  [ SDL_BUTTON_LEFT   & 0xff ] =  MU_MOUSE_LEFT,
  [ SDL_BUTTON_RIGHT  & 0xff ] =  MU_MOUSE_RIGHT,
  [ SDL_BUTTON_MIDDLE & 0xff ] =  MU_MOUSE_MIDDLE,
};

static const char key_map[256] = {
  [ SDLK_LSHIFT       & 0xff ] = MU_KEY_SHIFT,
  [ SDLK_RSHIFT       & 0xff ] = MU_KEY_SHIFT,
  [ SDLK_LCTRL        & 0xff ] = MU_KEY_CTRL,
  [ SDLK_RCTRL        & 0xff ] = MU_KEY_CTRL,
  [ SDLK_LALT         & 0xff ] = MU_KEY_ALT,
  [ SDLK_RALT         & 0xff ] = MU_KEY_ALT,
  [ SDLK_RETURN       & 0xff ] = MU_KEY_RETURN,
  [ SDLK_BACKSPACE    & 0xff ] = MU_KEY_BACKSPACE,
};

//repeated from main because i dont want to make a .h right now
enum PLAYBACK_STATUS{
    PLAYBACK_PAUSE,
    PLAYBACK_RUN,
    PLAYBACK_REWIND,
    PLAYBACK_FASTFORWARD
};

int64_t last_frame_time;
float delta_time;
float pivots_turning_speed[] = {5.0f,7.5f,10.0f,15.0f,20.0f};
float turning_counter = 0;
int pivot_sprite = 0;
float left_wheel_s  = 1.0f;
float right_wheel_s = 1.0f;
float wheels_size_range[] = {0.9f, 1.6f};
int mu_x_offset;
int mu_y_offset;
float mu_volume = 1;
int close_popup = 0;
int popup_is_open = 0;

char* speed_tags[] = {"50%","75%","100%","150%","200%"};
float speed_levels[] = {0.5f, 0.75f, 1.0f, 1.5f, 2.0f};
int speed_tag_i = 2;

char* play_button_tags[] = {"PLAY", "PAUSE"};

int file_open_trigger = 0;

extern int file_open;
extern int playback_status;
extern unsigned long long frame_count;
extern unsigned long long current_sample_abs;
extern int load_audio(char* p);

int64_t get_system_time(){
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)(tv.tv_sec) * 1000000 + tv.tv_usec;
}

void update_delta_time(){
    int64_t currentTime = get_system_time();
    delta_time = (currentTime - last_frame_time)/1000000.0f;
    last_frame_time = currentTime;
}

int error_exit(const char* error,char* message){
    printf("%s: %s",message,error);
    return 1;
}

int get_text_width(mu_Font font, const char *str, int len){
    SDL_Color text_color = {0,0,0};

    SDL_Surface* t = TTF_RenderText_Solid(main_font, str, text_color);

    int w = t->w;
    SDL_FreeSurface(t);
    return w;
}

int get_text_height(mu_Font font){
    return 30;
}

void desaturate_surface(SDL_Surface* s){
    SDL_LockSurface(s);
    Uint32 *pixels = (Uint32 *)s->pixels;
    int pixelCount = (s->w * s->h);

    for (int i = 0; i < pixelCount; i++) {
        Uint8 r, g, b, a;
        SDL_GetRGBA(pixels[i], s->format, &r, &g, &b, &a);
        Uint8 gray = (Uint8)(0.3 * r + 0.59 * g + 0.11 * b);
        
        pixels[i] = SDL_MapRGB(s->format, gray, gray, gray);
        pixels[i] = SDL_MapRGBA(s->format, gray, gray, gray, a);
    }

    SDL_UnlockSurface(s);
}

static mu_Rect expand_rect(mu_Rect rect, int n) {
    return mu_rect(rect.x - n, rect.y - n, rect.w + n * 2, rect.h + n * 2);
}

static void draw_frame(mu_Context *ctx, mu_Rect rect, int colorid) {
    /* draw border */
    if (ctx->style->colors[MU_COLOR_BORDER].a) {
        mu_draw_rect(ctx, expand_rect(rect, BUTTONS_BORDER), ctx->style->colors[MU_COLOR_BORDER]);
    }

    mu_draw_rect(ctx, rect, ctx->style->colors[colorid]);
}

float get_left_wheel_scale() {
    if (frame_count == 0) return wheels_size_range[0];
    return wheels_size_range[0] + ((float)current_sample_abs / (float)frame_count) * (wheels_size_range[1] - wheels_size_range[0]);
}

float get_right_wheel_scale() {
    if (frame_count == 0) return wheels_size_range[1];
    return wheels_size_range[1] - ((float)current_sample_abs / (float)frame_count) * (wheels_size_range[1] - wheels_size_range[0]);
}

int draw_tape(){
    
	int rs;
    main_surface = SDL_GetWindowSurface(window);
    SDL_FillRect( main_surface, NULL, SDL_MapRGB( main_surface->format, 230, 230, 230 ) );

    SDL_Rect color_bg;
    color_bg.h = 100;
    color_bg.w = 300;
    color_bg.x = main_surface->w/2 - color_bg.w/2;
    color_bg.y = main_surface->h/2 - color_bg.h/2;

    Uint32 bg_color = SDL_MapRGB( main_surface->format, file_open?251:180, file_open?145:180, file_open?145:180 );
    SDL_FillRect( main_surface, &color_bg, bg_color );

    // RIGHT WHEEL SCALE AND DRAW
    SDL_Rect wheel_dst;
    float scale = get_right_wheel_scale();
    wheel_dst.w = WHEEL_W *scale;
    wheel_dst.h = WHEEL_H *scale;
    wheel_dst.x = main_surface->w/2 - wheel_dst.w/2 + PIVOT_X_DISTANCE + PIVOT_X_OFFSET; 
    wheel_dst.y = main_surface->h/2 - wheel_dst.h/2 + PIVOT_Y_OFFSET;
    rs = SDL_BlitScaled(sprite_sets[file_open],&wheel,main_surface,&wheel_dst);

    // LEFT WHEEL SCALE AND DRAW
    scale = get_left_wheel_scale();
    wheel_dst.w = WHEEL_W *scale;
    wheel_dst.h = WHEEL_H *scale;
    wheel_dst.x = main_surface->w/2 - wheel_dst.w/2 - PIVOT_X_DISTANCE + PIVOT_X_OFFSET;
    wheel_dst.y = main_surface->h/2 - wheel_dst.h/2 + PIVOT_Y_OFFSET;
    rs = SDL_BlitScaled(sprite_sets[file_open],&wheel,main_surface,&wheel_dst);


    // SCALE AND DRAW THE PIVOTS
    SDL_Rect pivot_dst;
    pivot_dst.x = main_surface->w/2 - PIVOT_W/2 + PIVOT_X_DISTANCE + PIVOT_X_OFFSET; 
    pivot_dst.y = main_surface->h/2 - PIVOT_H/2 + PIVOT_Y_OFFSET;
    pivot_dst.w = PIVOT_W;
    pivot_dst.h = PIVOT_H;
    // right pivot
    rs = SDL_BlitSurface(sprite_sets[file_open],pivot+pivot_sprite,main_surface,&pivot_dst);
    // left pivot
    pivot_dst.x = main_surface->w/2 - PIVOT_W/2 - PIVOT_X_DISTANCE + PIVOT_X_OFFSET; 
    rs = SDL_BlitSurface(sprite_sets[file_open],pivot+pivot_sprite,main_surface,&pivot_dst);

    //DRAW THE TAPE 
    SDL_Rect tape_dst;
    tape_dst.w = TAPE_W;
    tape_dst.h = TAPE_H;
    tape_dst.x = main_surface->w/2 - tape_dst.w/2; 
    tape_dst.y = main_surface->h/2 - tape_dst.h/2;
    rs = SDL_BlitSurface(sprite_sets[file_open],&tape,main_surface,&tape_dst);


    //CALC THE OFFSET FOR THE WIDGETS
    // (it does it here for sync)
    mu_x_offset = (main_surface->w/2) - (MU_WIN_W/2);
    mu_y_offset = (main_surface->h/2) - (MU_WIN_H/2) + TAPE_H/2 + MU_WIN_Y_OFFSET;

    return 0;
}

int draw_widgets(){
    mu_Command* cmd = NULL;
    while (mu_next_command(&mu_ctx, &cmd)) {
        if(cmd->type == MU_COMMAND_TEXT){  
            
            if(strcmp(cmd->text.str,"MU_WINDOW") != 0){
                SDL_Color text_color = {cmd->text.color.r,cmd->text.color.g,cmd->text.color.b};
                SDL_Surface* t = TTF_RenderText_Solid(main_font, cmd->text.str,text_color);
                SDL_Rect tr = {.x=cmd->text.pos.x + mu_x_offset, .y=cmd->text.pos.y + mu_y_offset};
                SDL_BlitSurface(t,NULL,main_surface,&tr);
                SDL_FreeSurface(t);
            }
        }
        if(cmd->type == MU_COMMAND_RECT){
            SDL_Rect r = { .h=cmd->rect.rect.h, .w=cmd->rect.rect.w, .x=cmd->rect.rect.x+mu_x_offset, .y=cmd->rect.rect.y+mu_y_offset};
            if(r.x != (main_surface->w/2)-(MU_WIN_W/2) && r.y != (main_surface->h/2)-(MU_WIN_H/2)){
                SDL_FillRect( main_surface, &r, SDL_MapRGB( main_surface->format, cmd->rect.color.r, cmd->rect.color.g, cmd->rect.color.b ) );            
            }
        }
    }
}

void pivots_animation(){
    if(playback_status == PLAYBACK_PAUSE) return;
    if(playback_status == PLAYBACK_FASTFORWARD)
        turning_counter += pivots_turning_speed[4] * delta_time;
    else if(playback_status == PLAYBACK_REWIND)
        turning_counter += pivots_turning_speed[3] * delta_time;
    else turning_counter += pivots_turning_speed[speed_tag_i] * delta_time;
    while(turning_counter >= 1.0f){
        turning_counter -= 1.0f;

        int turning_dir = 1;
        if(playback_status == PLAYBACK_REWIND) turning_dir = -1;
        pivot_sprite += turning_dir;
        if(pivot_sprite >= 8)
            pivot_sprite = 0;

        if(pivot_sprite < 0)
            pivot_sprite = 7;

        draw_tape();
    }
}

int gui_poll(float* vol, float* sr, int h, int m, int s, int dh, int dm, int ds, void (*play_callback)()){
    update_delta_time();
    SDL_UpdateWindowSurface(window);
    pivots_animation();
    
    SDL_Event e;
    while(SDL_PollEvent(&e) != 0){
        // FILE OPEN DIALOG HORRIBLE PATCH
        if(e.type == SDL_MOUSEBUTTONUP && file_open_trigger){
            int bckp = playback_status;
            playback_status = PLAYBACK_PAUSE;
            
            char const* supported_ext[3]={"*.wav","*.mp3","*.flac"};
            char* file_name = tinyfd_openFileDialog("SELECT FILE",NULL,3,supported_ext,"audio files",0);
            if(file_name){
                int rs = load_audio(file_name);
                printf("debug point 0 -> %i\n",rs);
            }
            playback_status = bckp;
            file_open_trigger = 0;
        }

        switch(e.type){
            case SDL_QUIT: return 1; break;
            case SDL_WINDOWEVENT: draw_tape(); break;
            case SDL_MOUSEWHEEL:  mu_input_scroll(&mu_ctx, 0, e.wheel.y * -30); break;
            case SDL_TEXTINPUT:   mu_input_text(&mu_ctx, e.text.text); break;
        
            case SDL_MOUSEBUTTONDOWN:
                if(playback_status == PLAYBACK_PAUSE && popup_is_open) close_popup = 1;
            case SDL_MOUSEBUTTONUP: 
                int b = button_map[e.button.button & 0xff];
                if (b && e.type == SDL_MOUSEBUTTONDOWN) mu_input_mousedown(&mu_ctx, e.button.x+mu_x_offset, e.button.y+mu_y_offset, b);
                if (b && e.type ==   SDL_MOUSEBUTTONUP){
                    mu_input_mouseup(&mu_ctx, e.button.x+mu_x_offset, e.button.y+mu_y_offset, b);
                    if(playback_status == PLAYBACK_REWIND) playback_status = PLAYBACK_RUN;
                    if(playback_status == PLAYBACK_FASTFORWARD) playback_status = PLAYBACK_RUN;
                }
            break;

            case SDL_KEYDOWN:
            case SDL_KEYUP: 
                int c = key_map[e.key.keysym.sym & 0xff];
                if (c && e.type == SDL_KEYDOWN) mu_input_keydown(&mu_ctx, c);
                if (c && e.type ==   SDL_KEYUP) mu_input_keyup(&mu_ctx, c);
            break;
        }
    }

    // ------ DROP DOWN MENU CLOSE WORKAROUND ------
    if(close_popup == 1){
        close_popup = 2;
        mu_input_mousedown(&mu_ctx, 5000,5000, MU_MOUSE_LEFT);
    }

    else{
        SDL_GetMouseState(&mu_ctx.mouse_pos.x,&mu_ctx.mouse_pos.y);
        mu_ctx.mouse_pos.x -= mu_x_offset;
        mu_ctx.mouse_pos.y -= mu_y_offset;
        if(close_popup == 2){
            close_popup = 0;
            popup_is_open = 0;
            mu_input_mouseup(&mu_ctx, mu_ctx.mouse_pos.x,mu_ctx.mouse_pos.y, MU_MOUSE_LEFT);
            draw_tape();
        }
    }

    // --------------------------------------------
    
    mu_begin(&mu_ctx);

    if (mu_begin_window(&mu_ctx, "MU_WINDOW", mu_rect(0,0, MU_WIN_W, MU_WIN_H))) {
        mu_Container* mu_win = mu_get_container(&mu_ctx,"MU_WINDOW");
        #pragma region // ------------------ UI ---------------------- //
        
        mu_layout_row(&mu_ctx, 6, (int[]) { 50, 70, 50, 50, 50, 100 }, 0);

        if(mu_button(&mu_ctx, "OPEN")){
            file_open_trigger = 1;
        }

        if(mu_button(&mu_ctx, play_button_tags[playback_status == PLAYBACK_PAUSE ? 0:1]) && file_open) {
            play_callback();
        }

        if(mu_button(&mu_ctx, "<<")){
            if(file_open && playback_status == PLAYBACK_RUN) playback_status = PLAYBACK_REWIND;
        }

        if(mu_button(&mu_ctx, ">>")){
            if(file_open && playback_status == PLAYBACK_RUN) playback_status = PLAYBACK_FASTFORWARD;
        }

        if (mu_button(&mu_ctx, speed_tags[speed_tag_i])){
            mu_open_popup(&mu_ctx, "SPEED_SELECT");
            popup_is_open = 1;
        }

        mu_slider(&mu_ctx, &mu_volume, 0, 1);
        if (mu_begin_popup(&mu_ctx, "SPEED_SELECT")) {
            if(mu_button(&mu_ctx, "50%")){
                //close the pop up on the next frame
                speed_tag_i = 0;
                close_popup = 1;
            };
            
            if(mu_button(&mu_ctx, "75%")){
                speed_tag_i = 1;
                close_popup = 1;
            }

            if(mu_button(&mu_ctx, "100%")){
                speed_tag_i = 2;
                close_popup = 1;
            }

            if(mu_button(&mu_ctx, "150%")){
                speed_tag_i = 3;
                close_popup = 1;
            }
            
            if(mu_button(&mu_ctx, "200%")){
                speed_tag_i = 4;
                close_popup = 1;
            }

            mu_end_popup(&mu_ctx);
        }

        mu_layout_row(&mu_ctx, 1, (int[]) { 50 }, 0);
        char time_indicator[25];
        sprintf(time_indicator,"%02i:%02i:%02i / %02i:%02i:%02i",h,m,s,dh,dm,ds);
        mu_label(&mu_ctx,time_indicator);

        #pragma endregion
        mu_end_window(&mu_ctx);
        
    }
    mu_end(&mu_ctx);

    draw_widgets();

    *vol = mu_volume;
    *sr = speed_levels[speed_tag_i];

    return 0;
}

int gui_init() {
	if(SDL_Init( SDL_INIT_EVERYTHING ) < 0) return error_exit(SDL_GetError(),"Error initializing SDL");
    if(TTF_Init() < 0) return error_exit(TTF_GetError(),"Error initializing SDL_ttf");

	// WINDOW CREATION
	window = SDL_CreateWindow( 
        "MollyPlayback",                          // WINDOW TITLE
        SDL_WINDOWPOS_UNDEFINED,                  // WINDOW POS X
        SDL_WINDOWPOS_UNDEFINED,                  // WINDOW POS Y
        1280, 720,                                // WINDOW SIZE
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE   // WINDOW FLAGS
    );
	if(window == NULL)
        return error_exit(SDL_GetError(),"Error creating window");
    SDL_SetWindowMinimumSize(window,MIN_WINDOW_W,MIN_WINDOW_H);

    SDL_RWops* rw = SDL_RWFromMem(sprite_sheet_bmp,sprite_sheet_bmp_len);
    sprite_sheet   = SDL_LoadBMP_RW(rw,0); SDL_RWseek(rw,0,RW_SEEK_SET);
    bw_sprite_sheet = SDL_LoadBMP_RW(rw,1);
    desaturate_surface(bw_sprite_sheet);
    sprite_sets[0] = bw_sprite_sheet;
    sprite_sets[1] = sprite_sheet;
    
    rw = SDL_RWFromMem(monomaniac_ttf,monomaniac_ttf_len);
    main_font = TTF_OpenFontRW(rw,1,17);
    
    mu_init(&mu_ctx);
    mu_ctx.text_width = get_text_width;
    mu_ctx.text_height = get_text_height;
    mu_ctx.style->title_height = 0;
    mu_ctx.style->padding = 10;
    mu_ctx.style->spacing = 10;
    mu_ctx.draw_frame = draw_frame;

    //TEXT COLOR
    mu_ctx.style->colors[MU_COLOR_TEXT].r = 136;
    mu_ctx.style->colors[MU_COLOR_TEXT].g = 136;
    mu_ctx.style->colors[MU_COLOR_TEXT].b = 136;

    //BORDERS COLOR
    mu_ctx.style->colors[MU_COLOR_BORDER].r = 208;
    mu_ctx.style->colors[MU_COLOR_BORDER].g = 208;
    mu_ctx.style->colors[MU_COLOR_BORDER].b = 208;

    //WINDOWS BG COLOR
    mu_ctx.style->colors[MU_COLOR_WINDOWBG].r = 230;
    mu_ctx.style->colors[MU_COLOR_WINDOWBG].g = 230;
    mu_ctx.style->colors[MU_COLOR_WINDOWBG].b = 230;

    //BUTTONS BG COLOR
    mu_ctx.style->colors[MU_COLOR_BUTTON].r = 242;
    mu_ctx.style->colors[MU_COLOR_BUTTON].g = 242;
    mu_ctx.style->colors[MU_COLOR_BUTTON].b = 242;

    mu_ctx.style->colors[MU_COLOR_BUTTONHOVER].r = 255;
    mu_ctx.style->colors[MU_COLOR_BUTTONHOVER].g = 255;
    mu_ctx.style->colors[MU_COLOR_BUTTONHOVER].b = 255;

    mu_ctx.style->colors[MU_COLOR_BUTTONFOCUS].r = 208;
    mu_ctx.style->colors[MU_COLOR_BUTTONFOCUS].g = 208;
    mu_ctx.style->colors[MU_COLOR_BUTTONFOCUS].b = 208;
    
    //BASE = SLIDER
    mu_ctx.style->colors[MU_COLOR_BASE].r = 219;
    mu_ctx.style->colors[MU_COLOR_BASE].g = 219;
    mu_ctx.style->colors[MU_COLOR_BASE].b = 219;

    mu_ctx.style->colors[MU_COLOR_BASEHOVER].r = 219;
    mu_ctx.style->colors[MU_COLOR_BASEHOVER].g = 219;
    mu_ctx.style->colors[MU_COLOR_BASEHOVER].b = 219;

    mu_ctx.style->colors[MU_COLOR_BASEFOCUS].r = 219;
    mu_ctx.style->colors[MU_COLOR_BASEFOCUS].g = 219;
    mu_ctx.style->colors[MU_COLOR_BASEFOCUS].b = 219;

    draw_tape();
    last_frame_time = get_system_time();
	return 0;
}

int gui_free(){
    SDL_FreeSurface(bw_sprite_sheet);
    SDL_FreeSurface(sprite_sheet);
    SDL_FreeSurface(main_surface);
	SDL_DestroyWindow(window);
	SDL_Quit();
    return 0;
}