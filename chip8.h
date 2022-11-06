#ifndef CHIP8_H
#define CHIP8_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <getopt.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>

#define PIXEL_D                       16
#define WIN_WIDTH             64*PIXEL_D
#define WIN_HEIGHT            32*PIXEL_D
#define COLOR_ON           255, 255, 255
#define COLOR_OFF            0,   0,   0

#define CYCLES_PER_FRAME               8          
#define TICKS                         60 

/* The CHIP-8 has 4kb of memory, program counter starts at 0x200 */
#define MEM_SIZE                    4096
#define PC_START                   0x200

/* Extract a 4 bit argument from the opcode */
#define PREFIX(opcode)      (opcode & 0xF000)
#define X(opcode)          ((opcode & 0x0F00) >>  8)
#define Y(opcode)          ((opcode & 0x00F0) >>  4)

/* Extract an immediate value from the opcode */
#define N(opcode)          (opcode & 0x000F) //  4 bits
#define NN(opcdoe)         (opcode & 0x00FF) //  8 bits
#define NNN(opcode)        (opcode & 0x0FFF) // 12 bits


#define CHECK_ADDR_RETN(addr, len) if (addr+len > MEM_SIZE) { \
                                        status = ILLEGAL_MEM_ACCESS; \
                                        return; }
#define CHECK_ADDR_BREAK(addr, len) if (addr+len > MEM_SIZE) { \
                                        status = ILLEGAL_MEM_ACCESS; \
                                        break; } 

typedef enum chip8_status {
    OK = 0,
    OUT_OF_BOUNDS,
    STACK_EMPTY,
    STACK_OVF,
    ILLEGAL_MEM_ACCESS
} status_code;

uint8_t MEM[MEM_SIZE];

uint8_t V[16]; // 16 8-bit registers
#define VF V[15] // V[15] is often used to store flags
uint16_t I; // 16 bit address register

/* The CHIP-8 has two timers, both decrement at 60hz */
uint8_t DT; // Delay timer
uint8_t ST; // Sound timer

uint16_t PC; // Program counter, each cycle increases by two (instructions are 2-bytes)
uint16_t STACK[16]; // Program stack of 16-bit addresses
uint8_t SP; // Stack pointer

/* In addition, an hex keyboard and a 64 x 32 display, XOR Graphics */
uint8_t KEY[16];
uint8_t GFX[32][64]; // 32 columns, 64 rows.

Mix_Chunk *beep;

int renderFlag; // non-zero when a new frame should be rendered
int drawFlag; // non-zero when frame the next frame is not empty
int screenWrap; // wrap sprites around screen edges.
int soundInitialized; // non-zero if sound is initialized
int sound; // non-zero if sound is on

/* Old and new implementations have differences in some instructions */
#define INST_MODE_OLD      1   // Original behavior of CHIP-8
#define INST_MODE_NEW      0   // Modern implementations and games use this
int instruction_mode;

/* Inits SDL, main window and renderer. returns 0 on success, -1 on error */
static int SDL_Startup();

/* Opens audio device and prepares sound files. returns 1 on success, otherwise 0. */
static int SDL_SetupAudio();

/* Clears the SDL buffer with the background color */
static void SDL_ClearBuffer();

/* Makes all draw calls for the next frame */
static void SDL_RenderFrame();

/* Reads and processes input event */
static void SDL_HandleInput();

/* Free all resources */
static void Destroy();


/*** CHIP-8 Opcode execution ***/ 

/* Executes an opcode */
static void chip8_execute_opcode(uint16_t opcode);

/* Executes 8xxx opcodes */
static void exec_8xxx(uint16_t opcode);

/* Executes fxxx opcodes */
static void exec_fxxx(uint16_t opcode);

/* Clears the CHIP-8 graphics buffer and sets the render flag */ 
static void clrs();

/* Returns from a subroutine 
   Sets the program counter to top of the stack
   and decrements the stack pointer */
static void ret();

/* Calls a subroutine starting the memory address addr
   Increments the stack pointer, pushes the current 
   program counter to the stack and sets PC to addr */
static void call(uint16_t addr);

/* XORs the sprite stored at memory address starting at I of height N
   to the CHIP-8 GFX buffer at the position stored at registers RX and RY. */
static void draw(uint8_t RX, uint8_t RY, uint8_t N);

/* Inits the CHIP-8 memory and registers and loads fonts 
   if keep_memory is non zero, previously stored memory will be kept */
void chip8_init(int keep_memory);

/* Loads a program from path to the memory
   returns 0 on success, or non-zero on error */
int chip8_load_rom(const char *path);

/* Emulates a cpu cycle
   Returns a code representing success or type of error */
status_code chip8_cycle();

/* Updates the delay and sound timers
   As long as the sound timer is on, a beep is played */
void chip8_update_timers();

#endif