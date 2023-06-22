#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "display.h"

#define DEBUG 0
#define MEMORY_SIZE 4096
#define MEMORY_OFFSET 512
#define NIBBLE(n) ((instruction >> (16 - n * 4)) & 0xF)

uint8_t memory[MEMORY_SIZE];
uint8_t* pc = memory + MEMORY_OFFSET;

uint8_t registers[16];

uint16_t index_register;

uint8_t sound_timer = 0;
uint8_t delay_timer = 0;

//Stores locations in memory
uint8_t* stack[16];
uint8_t** stack_ptr = stack;

const uint16_t const font[] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

/*
    Loads file into Chip8 ROM file into memory 
    @param filepath The filepath for the file that needs to be read  
*/
static int initialize_file(char* filepath) {
    FILE* file;

    if(!(file = fopen(filepath, "rb"))){
        printf("Couldn't open file");
        exit(-1);
    }

    uint8_t buf;
    uint8_t* it = memory + MEMORY_OFFSET;
    while(fread(&buf, sizeof(uint8_t),1,file)) {
        *it++ = buf;
    }

    if(ferror(file)){
        printf("Reading file error");
        fclose(file);
        exit(-1);
    }

    fclose(file);

    //Loads font into memory
    int i = 0;
    for(;i < 80;i++) {
        memory[i] = font[i];
    }
}

/*
    Reads instruction and increments program counter 
    @returns The instruction at the program counter
*/
static uint16_t fetch(void) {
    //First byte
    uint16_t t1 = *pc++ << 8;
    //Second byte
    uint16_t t2 =  *pc++;

    return t1 | t2;
}

/*
    Executes instruction or ignores it
    @param Chip8 instruction to execute
*/
void execute_instruction(uint16_t instruction){

    switch (NIBBLE(1)) {
        case 0x0:
            if(instruction == 0xE0) {
                clear_screen();
            }
            //Pop Stack Instruction
            else if(instruction == 0xEE) {
                pc = *--stack_ptr; 
            }
            else if(DEBUG) {
                printf("\nInstruction not found: %x\n", instruction);
            }
            break;

        //Jump Instruction
        case 0x1:
            pc = memory + (instruction & 0xFFF);
            break;
        
        //Push Stack and Jump Instruction
        case 0x2:
            *stack_ptr++ = pc;
            pc = memory + (instruction & 0xFFF);
            break;

        //Skip Instructions(0x3, 0x4, 0x5)
        case 0x3:
            //Skip if the second nibble register equals the last two nibbles 
            if(registers[NIBBLE(2)] == (instruction & 0xFF)) {
                pc += 2 ;
            }
            break;
        
        case 0x4:
            //Skip if the second nibble register does not equal the last two nibbles 
            if(registers[NIBBLE(2)] != (instruction & 0xFF)){
                pc += 2 ;
            }
            break;
        
        case 0x5:
            //Skip if the second nibble register equals the third nibble register
            if(registers[NIBBLE(2)] == registers[NIBBLE(3)]){
                pc += 2;
            }
            break;

        //Set Register Instruction
        case 0x6:
            registers[NIBBLE(2)] = (instruction & 0xFF);
            break;
        
        //Add Register Instruction
        case 0x7:
            registers[NIBBLE(2)] += (instruction & 0xFF);
            break;

        case 0x8:
            switch (instruction & 0xF) {
                //Set Register Instruction 
                case 0x0:
                    //Sets third nibble register to second nibble register
                    registers[NIBBLE(2)] = registers[NIBBLE(3)];
                    break;
                //OR Register Instruction
                case 0x1:
                    registers[NIBBLE(2)] = registers[NIBBLE(2)] | registers[NIBBLE(3)];
                    registers[15] = 0;
                    break;
                
                //AND Register Instruction
                case 0x2:
                    registers[NIBBLE(2)] = registers[NIBBLE(2)] & registers[NIBBLE(3)];
                    registers[15] = 0;
                    break;
                
                //XOR Register Instruction
                case 0x3:
                    registers[NIBBLE(2)] = registers[NIBBLE(2)] ^ registers[NIBBLE(3)];
                    registers[15] = 0;
                    break;

                //ADD Register Instruction
                case 0x4: {
                    uint16_t temp = registers[NIBBLE(2)] + registers[NIBBLE(3)];
                    registers[NIBBLE(2)] = temp & 0xFF;
                    registers[15] = temp >= 256;
                    break;
                }

                //SUB Register(second nibble - third nibble) Instruction 
                case 0x5: {
                    int16_t temp = registers[NIBBLE(2)] - registers[NIBBLE(3)];
                    registers[NIBBLE(2)] = temp;
                    registers[15] = temp >= 0;
                    break;
                }

                //SUB Register(third nibble - second nibble) Instruction
                case 0x7: {
                    int16_t temp = registers[NIBBLE(3)] - registers[NIBBLE(2)];
                    registers[NIBBLE(2)] = temp;
                    registers[15] = temp >= 0;
                    break;
                }
                
                //RIGHT SHIFT Instruction
                case 0x6: {
                    bool temp = registers[NIBBLE(3)] & 0x1;

                    registers[NIBBLE(2)] = registers[NIBBLE(3)];
                    registers[NIBBLE(2)] >>= 1;

                    registers[15] = temp;  
                    break;
                }
                
                //LEFT SHIFT Instruction
                case 0xE: {
                    bool temp = registers[NIBBLE(3)] >> 7;

                    registers[NIBBLE(2)] = registers[NIBBLE(3)];
                    registers[NIBBLE(2)] <<= 1;

                    registers[15] = temp;
                    break;
                }
                    
                default:
                    if(DEBUG) {
                        printf("\nInstruction not found: %x\n",instruction);
                    }
                    
                    break;  
            }
            break;

        //Register NOT Compare Instruction
        case 0x9:
            if(registers[NIBBLE(2)] != registers[NIBBLE(3)]) {
                pc += 2;
            }
            break;
        
        //Set Index Register Instruction
        case 0xA:
            index_register = instruction & 0xFFF;
            break;
        
        //Jump with Offset Instruction
        case 0xB:
            pc = memory + (registers[0] + (instruction & 0xFFF));
            break;
        
        //Random Instruction
        case 0xC:
            registers[NIBBLE(2)] = ((uint8_t)(rand() % 256)) & (instruction & 0xFF);
            break;
        
        //Draw Sprite
        case 0xD: {
            //60 frame cap
            if(!frame_drawn()) {
                pc -= 2;
                break;
            }

            uint8_t x = registers[NIBBLE(2)] & 63;
            uint8_t y = registers[NIBBLE(3)] & 31;
            uint8_t length = NIBBLE(4);

            registers[15] = 0;

            int j = 0;
            for(;j < length && y + j < 32;j++) {
                
                //Sprite stored at index register
                uint8_t row = memory[index_register + j];

                int i = 0;
                for(;i < 8 && x + i < 64;i++) {
                    //Each bit represents either a black or white pixel
                    if((row >> (7 - i)) & 0x1) {
                        if(toggle_pixel(x + i,y + j)) {
                            registers[15] = 1;
                        }
                    }
                }
            }

            draw();
            break;
        }
        
        case 0xE:
            switch (instruction & 0xF)
            {
                //Skip if Key Down Instruction
                case 0xE:
                    if(key_down(registers[NIBBLE(2)])) {
                        pc += 2;
                    }
                    break;

                //Skip if Not Key Down Instruction
                case 0x1:
                    if(!key_down(registers[NIBBLE(2)])) {
                        pc += 2;
                    }
                    break;
                
                default:
                    if(DEBUG) {
                        printf("\nInstruction not found: %x\n",instruction);
                    } 
                    break;  
            }
            break;
        
        case 0xF:
            switch (instruction & 0xFF) {
                //Read Delay Timer Instruction
                case 0x7:
                    registers[NIBBLE(2)] = delay_timer;
                    break;
                
                //Set Delay Timer Instruction
                case 0x15:
                    delay_timer = registers[NIBBLE(2)];
                    break;

                //Set Sound Timer Instruction
                case 0x18:
                    sound_timer = registers[NIBBLE(2)];
                    break;

                //Add Index Register Instruction
                case 0x1E: {
                    index_register += registers[NIBBLE(2)];
                    registers[15] = index_register > 0xFFF;
                    break;
                }
                //Skip if Any Key Not Pressed
                case 0xA:
                    //Writes to nibble 2 register what key was pressed
                    if(!key_pressed(&(registers[NIBBLE(2)]))) {
                        pc -= 2;
                    }
                    break;

                //Set Index Register to Font Character
                case 0x29: 
                    //Font characters start at memory 0 and are 5 bytes in size
                    index_register = registers[(NIBBLE(2))] * 5;
                    break;

                //Binary-coded Decimal Conversion Instruction
                case 0x33: {
                    uint8_t temp = registers[NIBBLE(2)];

                    int i = 2;
                    for(;i >= 0;i--) {
                        memory[index_register + i] = temp % 10;
                        temp /= 10;
                    }
                    break;
                }

                //Store Memory Instruction
                case 0x55: {
                    uint8_t i = 0;
                    uint8_t x = (NIBBLE(2));

                    for(;i <= x;i++) {
                        memory[index_register + i] = registers[i];
                    }

                    index_register += x + 1;
                    break;
                }
                
                //Load Memory Instruction
                case 0x65: {
                    uint8_t i = 0;
                    uint8_t x = (NIBBLE(2));

                    for(;i <= x;i++) {
                        registers[i] = memory[index_register + i];
                    }

                    index_register += x + 1;
                    break;
                } 

                default:
                    if(DEBUG) {
                        printf("\nInstruction not found: %x\n",instruction);
                    }
                    break;            
            }
            break;

        default:
            if(DEBUG) {
                printf("\nInstruction not found: %x\n",instruction);
            }        
            break;
    }
}

/*
    Run Chip8 ROM at filepath
    @param filepath Path to ROM
*/
void run_chip(char* filepath) {
    //Current time
    struct timespec cur;
    //time for when the sound and delay timers where decremented last
    struct timespec past_timers;
    //time for when the main loop was executed last
    struct timespec past_main;

    initialize_file(filepath);
    if(!initialize_display()) {
        if(DEBUG) {
            printf("Display not initialized\n");
        }
        exit(-1);
    }
    
    timespec_get(&past_timers, TIME_UTC);
    timespec_get(&past_main, TIME_UTC);

    srand(time(NULL));

    while(true){
        timespec_get(&cur, TIME_UTC);

        //Timers are decremented at 60 Hz
        if((cur.tv_sec - past_timers.tv_sec) + (cur.tv_nsec - past_timers.tv_nsec) / 1000000000.0 > (1.0 / 60)) { 

            if(sound_timer > 0) {
                sound_timer--;
                play_beep();
                
            }

            if(delay_timer > 0) {
                delay_timer--;
            }

            past_timers = cur;
        }

        //Main loop executes at 700 Hz
        if((cur.tv_sec - past_main.tv_sec) + (cur.tv_nsec - past_main.tv_nsec) / 1000000000.0 > (1.0 / 700)) {

            uint16_t instruction = fetch();
            execute_instruction(instruction);

            past_main = cur;

            //All GUI events
            handle_events();
        }
    }

    close_display();
}

#undef DEBUG
#undef MEMORY_SIZE
#undef MEMORY_OFFSET
#undef NIBBLE
