#include <stdio.h>
#include <stdbool.h>
#include <SDL2/SDL.h>
#include <string.h>
#include "display.h"

#define DEBUG 0 
#define WIDTH  64
#define HEIGHT 32
#define SCALE 10
#define FPS 60
#define NUM_KEYS 16
#define NO_KEY 16
#define TONE_HZ 430
#define VOLUME 3000

static SDL_Window *window;
static SDL_Renderer *render;
//Bitmap display
static bool display[WIDTH][HEIGHT];

static Uint64 frame_start;

//keyboard status up or down
static bool keyboard[NUM_KEYS];
//Last key that was triggered by key up event
static uint8_t key_up = NO_KEY;
static const char const keys[] = {'X','1','2','3','Q','W','E','A','S','D','Z','C','4','R','F','V'}; 

//Square wave with a period of TONE_HZ and amplitude VOLUME
static void* beep_buffer;
static Uint32 beep_buffer_byte_len;

/*
    Setup audio and display
    @returns zero for failure and 1 for success in initialization
*/
bool initialize_display(void) {

    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        if(DEBUG) {
            printf("%s", "SDL2 not initialized");
        }
        return false;
    }

    window = SDL_CreateWindow("CHIP-8", SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,WIDTH * SCALE,HEIGHT * SCALE,SDL_WINDOW_BORDERLESS);
    render = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    if(!window) {
        if(DEBUG) {
            printf("%s", "Window not initialized");
        }
        return false;
    }

    if(!render){
        if(DEBUG) {
            printf("%s", "Render not initialized");
        }
        return false;
    }

    SDL_RenderSetLogicalSize(render, WIDTH, HEIGHT);

    SDL_SetRenderDrawColor(render, 0, 0, 0, 0);

    SDL_AudioSpec wanted_spec;
    SDL_zero(wanted_spec);

    wanted_spec.freq = 48000;
    wanted_spec.format = AUDIO_S16LSB;
    wanted_spec.channels = 1;
    wanted_spec.samples = 4096; 
    wanted_spec.callback = NULL;

    SDL_AudioSpec gotten_spec;
    if(SDL_OpenAudio(&wanted_spec, &gotten_spec)) {
        if(DEBUG) {
            printf("%s", "Audio not initialized");
        }
        return false;
    }

    if(gotten_spec.channels != 1 || gotten_spec.format != AUDIO_S16LSB) {
        if(DEBUG) {
            printf("%s", "Non compatible audio specification");
        }
        return false;
    }

    beep_buffer_byte_len = sizeof(int16_t) * (gotten_spec.freq / FPS);
    beep_buffer = malloc(beep_buffer_byte_len);

    int16_t* it = (int16_t*) beep_buffer;
    int i = 0;
    //Initializes beep buffer with square wave
    for(;i < (gotten_spec.freq / FPS);i++) {
        *it++ = ((i / (gotten_spec.freq / TONE_HZ / 2)) % 2) ? VOLUME : -VOLUME;
    }

    SDL_PauseAudio(0);

    SDL_RenderPresent(render);

    frame_start = SDL_GetPerformanceCounter();

    return true;
}

/*
    Closes all system resources
*/
void close_display(void) {
    SDL_DestroyRenderer(render);
    SDL_CloseAudio();
    SDL_DestroyWindow(window);
    SDL_Quit();
}

/*
    Redraws entire screen using the display bitmap
*/
void draw(void) { 
    SDL_RenderClear(render);

    int y = 0;
    for(;y < HEIGHT; y++) {
        int x = 0;
        for(;x < WIDTH; x++) {
            if(display[x][y]) {
                //Draw white pixel
                SDL_SetRenderDrawColor(render, 255, 255, 255, 255);
            }
            else {
                //Draw black pixel
                SDL_SetRenderDrawColor(render, 0, 0, 0, 0);
            }

            SDL_RenderDrawPoint(render, x, y);
        }
    }

    SDL_RenderPresent(render);
}

/*
    Clears display bitmap and screen    
*/
void clear_screen(void) { 
    SDL_RenderClear(render);
    SDL_SetRenderDrawColor(render, 0, 0, 0, 0);

    int y = 0;
    for(;y < HEIGHT; y++) {
        int x = 0;
        for(;x < WIDTH; x++) {
            display[x][y] = 0;
        }
    }

    SDL_RenderPresent(render);
}

/*
    Toggles pixel at (x, Top Left - y)
    @returns previous state of pixel
*/
bool toggle_pixel(int x, int y) {
    return !(display[x][y] = !display[x][y]);
}

/*
    Gets status of key (pressed or depressed). Status updated by handle_events().
    @param key Hexadecimal of key to get status of
    @returns true if key is pressed down and otherwise false
*/
bool key_down(uint8_t key) {
    return keyboard[key];
}

/*
    Checks if any key was depressed. Status updated by handle_events().
    @param key Sets key to the latest key(hexadecimal) that was depressed
    @returns True if any key was depressed
*/
bool key_pressed(uint8_t* key) {
    if(key_up != NO_KEY) {
        if(key) {
            *key = key_up;
        }

        key_up = NO_KEY;
        return true;
    }
    
    return false;
    
}

/*
    Converts scancode to hexadecimal key compatible with Chip8
    @param s SDL_Scancode to convert to hexadecimal key
    @returns Hexadecimal converted from s. Returns -1 if key not defined
*/
static int scancode_to_index(SDL_Scancode s) {
    const char* key = SDL_GetScancodeName(s);

    if(strlen(key) != 1) {
        return -1;
    }

    int i = 0;
    for(;i < NUM_KEYS;i++) {
        if(*key == keys[i]) {
            return i;
        }
    }

    return -1;
}

/*
    Handles all events related to GUI including closing window and handling key presses
*/
void handle_events(void){
    SDL_Event event;

    key_up = NO_KEY;
    while(SDL_PollEvent(&event)) {
        if(event.type == SDL_QUIT) {
            close_display();
            exit(0);
        }

        else if(event.type == SDL_KEYDOWN) {
            int key_index = scancode_to_index(event.key.keysym.scancode);
            
            
            if(key_index != -1) {
                //updates key to be pressed
                keyboard[key_index] = true;
            }
            break;
        }

        else if(event.type == SDL_KEYUP) {
            int key_index = scancode_to_index(event.key.keysym.scancode);
            
            if(key_index != -1) {
                //updates key to be depressed
                keyboard[key_index] = false;
                key_up = key_index;
            }
            break;
        }
    }
}

/*
    Checks if 1/60 of a second past
    @returns True if 1/60 of a second past
*/
bool frame_drawn(void) {
    Uint64 frame_end = SDL_GetPerformanceCounter();

    float time_dif = (frame_end - frame_start) / (float) SDL_GetPerformanceFrequency() * 1000.0f;

    if(time_dif > 1.0 / FPS) {
        frame_start = SDL_GetPerformanceCounter();
        return true;
    }

    return false;
}

/*
    Plays beep_buffer sound
*/
void play_beep(void) {
    SDL_QueueAudio(1, beep_buffer, beep_buffer_byte_len);
}

#undef WIDTH  
#undef HEIGHT 
#undef SCALE
#undef FPS
#undef NUM_KEYS
#undef NO_KEY
#undef TONE_HZ
#undef VOLUME
#undef DEBUG