#include <stdbool.h>

bool initialize_display(void);
void close_display(void);
void clear_screen(void);
void draw(void);
bool toggle_pixel(int x, int y);
bool key_down(uint8_t key);
bool key_pressed(uint8_t* key);
void handle_events(void);
bool frame_drawn(void);
void play_beep(void);