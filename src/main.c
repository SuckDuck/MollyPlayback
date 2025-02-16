#define _XOPEN_SOURCE 500
#define MINIAUDIO_IMPLEMENTATION
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>
#include <stdint.h>
#include <math.h>
#include <miniaudio.h>
#include "liteinput.h"

#include "tinyfiledialogs.h"

#define CHUNK_SIZE  256
#define BLOCK_SIZE  256

#define PLAYPAUSE_BUTTON_CODE   65300
#define REWIND_BUTTON_CODE      65302
#define FASTFORWARD_BUTTON_CODE 65303

#define SPEEDUP_BUTTON_CODE     65362
#define SPEEDDOWN_BUTTON_CODE   65364

#define VOLUMEUP_BUTTON_CODE    93
#define VOLUMEDOWN_BUTTON_CODE  47

void handle_audio( ma_device* pDevice,void *out_buf, const void *in_buf,ma_uint32 samples_c );

event_listener listener;
typedef int PLAYBACK_STATUS_t;
enum PLAYBACK_STATUS{
    PLAYBACK_PAUSE,
    PLAYBACK_RUN,
    PLAYBACK_REWIND,
    PLAYBACK_FASTFORWARD
};

PLAYBACK_STATUS_t playback_status = PLAYBACK_PAUSE;

ma_decoder decoder;
ma_device device;
// contents on these "chunks" are just interleaved samples.
//
// blocks referes to the quantity of samples that portAudio 
// process on one callback invoke
//
// chunks are groups of blocks, when the callback starts reading
// the samples of one chunk, the content of the following one
// gets loaded.

typedef struct chunk{
    int id;
    float samples[CHUNK_SIZE * BLOCK_SIZE * 2];
} chunk;

chunk chunk_a, chunk_b, chunk_c;
chunk* chunks[] = { &chunk_a, &chunk_b, &chunk_c };
ma_uint64 frame_count = 0;

// SPEED MANIPULATION
int repeat_counter = 0;
int repeat_target = 0;
float play_back_speed = 1.0f;

float volume = 1.0f;

int current_chunk;
int current_sample;
ma_uint64 current_sample_abs;

int sound_len_h, sound_len_m, sound_len_s = 0;
int file_open = 0;
int running = 1;

extern int gui_init();
extern int gui_free();
int gui_poll(float* vol, float* sr, int h, int m, int s, int dh, int dm, int ds, void (*play_callback)());

const char* playback_status_str(PLAYBACK_STATUS_t s){
    switch (s){
        case PLAYBACK_RUN:         return "RUN";         break;
        case PLAYBACK_PAUSE:       return "PAUSE";       break;
        case PLAYBACK_REWIND:      return "REWIND";      break;
        case PLAYBACK_FASTFORWARD: return "FASTFORWARD"; break;
    }

    return "";
}

void get_sample_time(ma_decoder* d,int sample_i,int* h, int* m, int* s){
    int total_seconds = sample_i/d->outputSampleRate;
    *h = *m = *s = 0;
    *m = total_seconds/60;
    *s = total_seconds - (*m*60);
    *h = *m/60;
    *m = *m - (*h*60);
}

void load_chunk( int i, int id ){
    if(i > 2) i=0;
    if(i < 0) i=2;
    float* c = chunks[i]->samples;

    if(id > -1) chunks[i]->id = id;
    else{
        if(playback_status == PLAYBACK_REWIND) chunks[i]->id = chunks[current_chunk]->id - 1;
        else chunks[i]->id = chunks[current_chunk]->id + 1;
    }
    
    // READ AN ENTIRE CHUNK
    ma_uint64 frames_read = 0;
    ma_decoder_seek_to_pcm_frame(&decoder, BLOCK_SIZE * CHUNK_SIZE * chunks[i]->id);
    ma_decoder_read_pcm_frames(&decoder, c, BLOCK_SIZE * CHUNK_SIZE,&frames_read);
}

void next_sample(){
    current_sample += playback_status == PLAYBACK_REWIND ? -1:1;
    current_sample_abs += playback_status == PLAYBACK_REWIND ? -1:1;
    if(current_sample_abs < 0){
        current_sample_abs = 0;
        playback_status = PLAYBACK_PAUSE;
        return;
    }

    if(current_sample_abs > frame_count-1){
        current_sample_abs = frame_count-1;
        playback_status = PLAYBACK_PAUSE;
        return;
    }

    //forward
    if(current_sample > CHUNK_SIZE * BLOCK_SIZE){
        current_sample = 0;
        current_chunk++;
        if(current_chunk > 2)
            current_chunk=0;
    }

    //backward
    if(current_sample < 0){
        current_sample = CHUNK_SIZE * BLOCK_SIZE;
        current_chunk--;
        if(current_chunk < 0)
            current_chunk=2;
    }

}

void reset_audio(){
    load_chunk(0,0); load_chunk(1,1);
    current_chunk = 0;
    current_sample = 0;
    current_sample_abs = 0;
}

int load_audio(char* p){
    if(file_open){
        ma_device_stop(&device);
        ma_device_uninit(&device);
    }

    //DOUBLE INIT BECAUSE IT NEEDS TO SET THE SAMPLES FORMAT
    int rs = ma_decoder_init_file(p, NULL, &decoder);
    if (rs != MA_SUCCESS) return 1;
    
    ma_decoder_config decoder_config = ma_decoder_config_init(ma_format_f32, decoder.outputChannels, decoder.outputSampleRate);    
    rs = ma_decoder_init_file(p, &decoder_config, &decoder);
    if (rs != MA_SUCCESS) return 1;
    
    // GET THE AUDIO FULL LENGHT
    rs = ma_decoder_get_length_in_pcm_frames(&decoder,&frame_count);
    if (rs != MA_SUCCESS) return 1;

    printf("%lu %lu\n",decoder.outputChannels,decoder.outputSampleRate);
    
    get_sample_time(&decoder,frame_count,&sound_len_h,&sound_len_m,&sound_len_s);
    reset_audio();
    
    ma_device_config config  = ma_device_config_init(ma_device_type_playback);
    config.playback.format   = ma_format_f32;             // Set to ma_format_unknown to use the device's native format.
    config.playback.channels = decoder.outputChannels;    // Set to 0 to use the device's native channel count.
    config.sampleRate        = decoder.outputSampleRate;  // Set to 0 to use the device's native sample rate.
    config.dataCallback      = handle_audio;              // This function will be called when miniaudio needs more data.
    config.pUserData         = NULL;                      // Can be accessed from the device object (device.pUserData).
    
    if (ma_device_init(NULL, &config, &device) != MA_SUCCESS)
        return -1;  // Failed to initialize the device.

    ma_device_start(&device);
    file_open = 1;
    return 0;
}

void play_pause_audio(){
    if(playback_status != PLAYBACK_PAUSE) playback_status = PLAYBACK_PAUSE;
    else{
        if(current_sample_abs >= frame_count-1){
            reset_audio();
        }
        playback_status = PLAYBACK_RUN;
    }
}

void handle_audio( ma_device* pDevice,
                   void *out_buf, const void *in_buf,
                   ma_uint32 samples_c 
                ){

    float *out = (float*)out_buf;
    (void) in_buf; //Prevent unused variable warning.

    for(int i=0; i<samples_c; i++ ){
        if(playback_status == PLAYBACK_PAUSE){
            for(int i=0; i<decoder.outputChannels; i++){
                *out++ = 0;
            }
            continue; 
        }

        if(decoder.outputChannels == 1){
            *out++ = chunks[current_chunk]->samples[current_sample] * volume;
        }
        else{
            *out++ = chunks[current_chunk]->samples[current_sample*2] * volume;
            *out++ = chunks[current_chunk]->samples[current_sample*2+1] * volume;
        }
        

        if(repeat_target > 0){
            repeat_counter++;
            if(repeat_counter > repeat_target){
                repeat_counter = 0;
                continue; //repeats one sample
            }
        }

        else if(repeat_target == -99){//max speed
            next_sample();
        }

        else if(repeat_target < 0){
            repeat_counter--;
            if(repeat_counter < repeat_target){
                repeat_counter = 0;
                next_sample(); //skips one sample
            }
        }

        next_sample();

    }
}

void handle_keyboard(keyboard_event evt){
	if(!file_open) return;
    if(evt.pressed){
        if(evt.keycode == PLAYPAUSE_BUTTON_CODE){
            play_pause_audio();
        }

        if(evt.keycode == REWIND_BUTTON_CODE)
            if(playback_status == PLAYBACK_RUN) playback_status = PLAYBACK_REWIND;

        if(evt.keycode == FASTFORWARD_BUTTON_CODE)
            if(playback_status == PLAYBACK_RUN) playback_status = PLAYBACK_FASTFORWARD;

        /*if(evt.keycode == SPEEDUP_BUTTON_CODE){
            play_back_speed += play_back_speed < 1 ? 0.25f:0.5f;
            if(play_back_speed > 2.0f) play_back_speed = 2.0f;
        }
            
        if(evt.keycode == SPEEDDOWN_BUTTON_CODE){
            play_back_speed -= play_back_speed > 1 ? 0.5f:0.25f;
            if(play_back_speed < 0.5f) play_back_speed = 0.5f;
        }*/

    }

    /*if(evt.keycode == VOLUMEDOWN_BUTTON_CODE)
        volume -= 0.01f;
        if(volume < 0.0f) volume=0.0f;
        

    if(evt.keycode == VOLUMEUP_BUTTON_CODE)
        volume += 0.01f;
        if(volume > 1.0f) volume=1.0f;*/
	
    if(evt.released){
        if(evt.keycode == REWIND_BUTTON_CODE)
            if(playback_status == PLAYBACK_REWIND) playback_status = PLAYBACK_RUN;

        if(evt.keycode == FASTFORWARD_BUTTON_CODE)
            if(playback_status == PLAYBACK_FASTFORWARD) playback_status = PLAYBACK_RUN;
    }
}

int main_loop(){
    int last_chunk = 0;
    while (running){
        event_listener_poll(&listener,handle_keyboard,NULL,NULL);
        if(current_chunk != last_chunk){
            last_chunk = current_chunk;
            if(playback_status == PLAYBACK_REWIND) load_chunk(current_chunk - 1,-1);
            else load_chunk(current_chunk + 1,-1);
        }

        // UPDATE PLAYBACK SPEED
        switch((int)(play_back_speed*100)){
            case 50:  repeat_target = 1;   break;
            case 75:  repeat_target = 2;   break;
            case 100: repeat_target = 0;   break;
            case 150: repeat_target = -1;  break;
            case 200: repeat_target = -99; break;
        }

        if(playback_status == PLAYBACK_REWIND && play_back_speed < 1.5f) repeat_target = -1;
        else if(playback_status == PLAYBACK_FASTFORWARD) repeat_target = -99;
    
        // SIMPLE MENU SECTION
        int h,m,s;
        h = m = s = 0;
        if(file_open)
            get_sample_time(&decoder,current_sample_abs,&h,&m,&s);

        int rs = gui_poll(&volume, &play_back_speed,h,m,s,sound_len_h,sound_len_m,sound_len_s,play_pause_audio);
        if(rs == 1){
            running = 0;
        }

        usleep(50);
    }
    
    return 0;
}

int main(int args_c, char** args){
    if (geteuid() != 0) {
        char display[100];
        char xauthority[100];
        sprintf(display,"DISPLAY=%s",getenv("DISPLAY"));
        sprintf(xauthority,"XAUTHORITY=%s",getenv("XAUTHORITY"));

        char full_bin_path[100];
        int r = readlink("/proc/self/exe", full_bin_path, 100);
        full_bin_path[r] = '\0';

        execlp("pkexec", "pkexec", "env", display, xauthority, full_bin_path,NULL);
        exit(EXIT_FAILURE);
    }
    event_listener_init(&listener);

    gui_init();
    int rs = main_loop();
    gui_free();
    ma_device_stop(&device);
    ma_device_uninit(&device);
    ma_decoder_uninit(&decoder);

    return rs;

}