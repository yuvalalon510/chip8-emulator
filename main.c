#include "chip8.h"

char *status_msg[] = {
    "NONE",
    "Illegal PC address",
    "No return address",
    "Stack overflow",
    "Illegal memory access"
};

uint8_t keycodes[] = {
    '1', '2', '3', '4', // 1 2 3 c
    'q', 'w', 'e', 'r', // 4 5 6 d
    'a', 's', 'd', 'f', // 7 8 9 e
    'z', 'x', 'c', 'v'  // a 0 b f
};

uint8_t keymap[] = {
    0x01, 0x02, 0x03, 0x0c, // 1 2 3 c
    0x04, 0x05, 0x06, 0x0d, // 4 5 6 d
    0x07, 0x08, 0x09, 0x0e, // 7 8 9 e
    0x0a, 0x00, 0x0b, 0x0f  // a 0 b f
};

SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;
int running, paused;

int main(int argc, char *argv[]) {

    int opt;

    if (argc < 2) {
        printf("Usage: %s [-iws] ROM\n", argv[0]);
        printf("-i      Fix for compatibility with older games.\n");
        printf("-w      Disable screen wrapping.\n");
        printf("-s      Start with no sound.\n");
        exit(1);
    }

    instruction_mode = INST_MODE_NEW;
    sound = 1;
    screenWrap = 1;
    while ((opt = getopt(argc, argv, "iws")) != -1) {
        switch (opt) {
        case 'i':
            instruction_mode = INST_MODE_OLD;
            break;
        case 'w':
            screenWrap = 0;
            break;
        case 's':
            sound = 0;
            break;
        default:
            exit(1);    
        }
    }

    if (SDL_Startup() == -1) {
        printf("Could not init SDL: %s\n", SDL_GetError());
        Destroy();
        exit(1);
    }

    soundInitialized = SDL_SetupAudio();

    chip8_init(0);

    if (chip8_load_rom(argv[argc - 1]) != 0) {
        Destroy();
        exit(1);
    }

    /* Main loop: at each iteration, several cycles are performed.
       at the same time, input events are read.
       if render or draw flags are set, a new frame is presented.
       the running time of an iteration is measured, and the loop sleeps
       as long as needed for the target framerate to be reached. */
    int t1, t2, elapsed;
    int interval = 1000 / TICKS;
    running = 1;
    paused = 0;

    while (running) {

        t1 = SDL_GetTicks();

        // Emulate several cycles, targeting (approx) cpu speed of (cycles per refresh * refresh rate)
        for (int i=0; i < CYCLES_PER_FRAME; i++) {
            SDL_HandleInput();

            if (paused)
                continue;

            status_code rt = chip8_cycle();
            if (rt != OK) {
                printf("Execution Error: %s\n", status_msg[rt]);
                running = 0;
            }

        }

        // Draw next frame
        if (renderFlag) {
            SDL_ClearBuffer();
            if (drawFlag) {
                SDL_RenderFrame();
                drawFlag = 0;
            }

            SDL_RenderPresent(renderer);
            renderFlag = 0;
        }

        // Sleep for remaining time
        t2 = SDL_GetTicks();
        elapsed = t2 - t1;
        if (elapsed < interval) {
            SDL_Delay(interval - elapsed);
            elapsed = interval;
        }

        // Update timers and play sound
        chip8_update_timers(); 
        
    }

    Destroy();
}

static int SDL_Startup() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0)
        return -1;

    window = SDL_CreateWindow("CHIP8 Emulator - Running", SDL_WINDOWPOS_UNDEFINED, 
                            SDL_WINDOWPOS_UNDEFINED, WIN_WIDTH, WIN_HEIGHT, 0);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (window == NULL || renderer == NULL) {
        return -1;
    }

    return 0;
}

static int SDL_SetupAudio() {
    int rt;

    if (Mix_Init(0) != 0 || Mix_OpenAudio(MIX_DEFAULT_FREQUENCY, MIX_DEFAULT_FORMAT, 1, 1024) == -1) {
        printf("Could not init audio: %s\n", SDL_GetError());
        rt = 0;
    } else if ((beep = Mix_LoadWAV("beep.wav")) == NULL) {
        printf("Beep sound file is missing\n");
        rt = 0;
    } else {
        rt = 1;
    }

    return rt;
}

static void SDL_HandleInput() {
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        switch(event.type) {

        case SDL_QUIT:
            running = 0;
            break;

        case SDL_KEYDOWN:
        case SDL_KEYUP:

            if (event.type == SDL_KEYUP) {
                if (event.key.keysym.sym == 'p') {
                    paused = !paused;
                    if (paused)
                        SDL_SetWindowTitle(window, "CHIP8 Emulator - Paused");
                    else
                        SDL_SetWindowTitle(window, "CHIP8 Emulator - Running");
                }
                else if (event.key.keysym.sym == 'o') {
                    sound = !sound;
                }
                else if (event.key.keysym.sym == 'i') {
                    chip8_init(1); // restart
                }
            }

            for (int i=0; i<16; i++) {
                if (event.key.keysym.sym == keycodes[i]) {
                    KEY[keymap[i]] = (event.type == SDL_KEYDOWN) ? 1 : 0;
                    break;
                }
            }
            
            break;

        default:
            break;
            
        }
    }
}

static void SDL_ClearBuffer() {
    SDL_SetRenderDrawColor(renderer, COLOR_OFF, 255);
    SDL_RenderClear(renderer);
}

static void SDL_RenderFrame() {
    SDL_SetRenderDrawColor(renderer, COLOR_ON, 255);
    SDL_Rect rect = {.x=0, .y=0, .w=PIXEL_D, .h=PIXEL_D};

    for (int row = 0; row < 32; row++) {

        for (int col = 0; col < 64; col++) {

            if (!GFX[row][col])
                continue;
            rect.x = PIXEL_D * col;
            rect.y = PIXEL_D * row;
            SDL_RenderFillRect(renderer, &rect);

        }
    }
}

static void Destroy() {
    SDL_DestroyWindow(window);
    SDL_DestroyRenderer(renderer);
    Mix_FreeChunk(beep);
    Mix_CloseAudio();
    Mix_Quit();
    SDL_Quit();
}