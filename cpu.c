#include "chip8.h"

#define BEEP_DURATION 8

static inline void jump(uint16_t addr, uint16_t offset) {PC = addr + offset;}

uint8_t fontset[80] = {
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

status_code status;

void chip8_init(int keep_memory) {
    srand(time(NULL));
    clrs();

    if (!keep_memory)
        memset(MEM, 0, MEM_SIZE);
    memset(V, 0, 16);
    memset(STACK, 0, 16);
    memset(KEY, 0, 16);
    I = DT = ST = SP = 0;
    PC = 0x200;

    for (int i=0; i<80; i++)
        MEM[i] = fontset[i];
}

int chip8_load_rom(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        printf("Unable to open ROM\n");
        return 1;
    }

    fseek(f, 0L, SEEK_END);
    size_t fsize = ftell(f);
    if (fsize > MEM_SIZE - PC_START) {
        printf("ROM too large\n");
        return 2;
    }

    rewind(f);
    fread(MEM + PC_START, 1, fsize, f);
    fclose(f);
    return 0;
}

status_code chip8_cycle() {
    if (PC < 0x200 || PC > MEM_SIZE - 2)
        return OUT_OF_BOUNDS;
    
    // Read a two byte opcode and execute it
    uint16_t opcode = MEM[PC] << 8 | MEM[PC+1];
    PC += 2;
    chip8_execute_opcode(opcode); // Updates status code if there were errors
    return status;
}

void chip8_update_timers() {
    static int lastBeep = 0;

    if (DT > 0) --DT;
    if (ST > 0) {
        if (soundInitialized && sound) {
            if (!lastBeep || lastBeep - ST >= BEEP_DURATION) {
                Mix_PlayChannel(-1, beep, 0);
                lastBeep = ST;
            }
        }
        
        --ST;

        if (ST == 0)
            lastBeep = 0;
    }
}

static void chip8_execute_opcode(uint16_t opcode) {
    uint16_t tmp;

    uint8_t X = X(opcode);
    uint8_t Y = Y(opcode);
    uint8_t N = N(opcode);
    uint8_t NN = NN(opcode);
    uint16_t NNN = NNN(opcode);
    status = OK;

    switch (PREFIX(opcode)) {

    case 0x0000:
        if (NN == 0xE0) 
            clrs();
        else if (NN == 0xEE) 
            ret();
        break;

    case 0x1000:
        jump(NNN, 0);
        break;

    case 0x2000:
        call(NNN);
        break;

    case 0x3000:
        if (V[X] == NN)
            PC += 2;
        break;

    case 0x4000:
        if (V[X] != NN)
            PC += 2;
        break;

    case 0x5000:
        if (V[X] == V[Y])
            PC += 2;
        break;

    case 0x6000:
        V[X] = NN;
        break;

    case 0x7000:
        V[X] += NN;
        break;

    case 0x8000:
        exec_8xxx(opcode);
        break;

    case 0x9000:
        if (V[X] != V[Y])
            PC += 2;
        break;

    case 0xA000:
        I = NNN;
        break;

    case 0xB000:
        jump(NNN, V[0]);
        break;

    case 0xC000:
        V[X] = (rand() % 256) & NN;
        break;

    case 0xD000:
        draw(X, Y, N);
        break;

    case 0xE000:
        if (V[X] > 0xF)
            break;

        if (NN == 0x9E && KEY[V[X]])
            PC += 2;
        else if (NN == 0xA1 && !KEY[V[X]])
            PC += 2;
        break; 

    case 0xF000:
        exec_fxxx(opcode);
        break;

    default:
        break;
    }
}

static void exec_8xxx(uint16_t opcode) {
    uint16_t res;
    uint8_t X = X(opcode);
    uint8_t Y = Y(opcode);
    switch(N(opcode)) {

    case 0x0000:
        V[X] = V[Y];
        break;

    case 0x0001:
        V[X] |= V[Y];
        break;
        
    case 0x0002:
        V[X] &= V[Y];
        break;

    case 0x0003:
        V[X] ^= V[Y];
        break;

    case 0x0004:
        res = V[X] + V[Y];
        VF = (res > 0xFF);
        V[X] = res;
        break;

    case 0x0005:
        VF = (V[X] > V[Y]);
        V[X] -= V[Y];
        break;

    case 0x0006:
        if (instruction_mode == INST_MODE_OLD)  {
            VF = V[Y] & 1;
            V[X] = V[Y] >> 1;
        } else {
            VF = V[X] & 1;
            V[X] >>= 1;
        }
        break;

    case 0x0007:
        res = V[Y] - V[X];
        VF = (V[Y] > V[X]);
        V[X] = res;
        break;

    case 0x000E:
        if (instruction_mode == INST_MODE_OLD) {
            VF = V[Y] >> 7;
            V[X] = V[Y] << 1;
        }
        else {
            VF = V[X] >> 7;
            V[X] <<= 1;
        }
        break;

    default:
        break;
    }
}

static void exec_fxxx(uint16_t opcode) {
    int keypress = 0;
    uint8_t X = X(opcode);
    switch(NN(opcode)) {
    case 0x0007:
        V[X] = DT;
        break;

    case 0x000A:
        for (int i=0; i<16; i++) {
            if (KEY[i]) {
                keypress = 1;
                V[X] = i;
            }
        }

        if (!keypress)
            PC -= 2;
        
        break;

    case 0x0015:
        DT = V[X];
        break;

    case 0x0018:
        ST = V[X];
        break;

    case 0x001E:
        I += V[X];
        break;
        
    case 0x0029:
        I = V[X]*5;
        break;

    case 0x0033:
        CHECK_ADDR_BREAK(I, 3);

        MEM[I] = V[X] / 100;
        MEM[I+1] = (V[X] / 10) % 10;
        MEM[I+2] = V[X] % 10;
        break;

    case 0x0055:
        CHECK_ADDR_BREAK(I, X+1);

        for (int i=0; i<=X; i++)
            MEM[I+i] = V[i];
        
        if (instruction_mode == INST_MODE_OLD)
            I += X+1;
        break;

    case 0x0065:
        CHECK_ADDR_BREAK(I, X+1);

        for (int i=0; i<=X; i++)
            V[i] = MEM[I+i];

        if (instruction_mode == INST_MODE_OLD)
            I += X+1;
        break;
    }
}

static void clrs() {
    for (int i=0; i<32; i++)
        memset(GFX[i], 0, 64);
    renderFlag = 1;
    drawFlag = 0;
}

static void ret() {
    if (SP <= 0) 
        status = STACK_EMPTY;
     else
        PC = STACK[--SP];
}

static void call(uint16_t addr) {
    if (SP >= 16) {
        status = STACK_OVF;
    } else {
        STACK[SP++] = PC;
        jump(addr, 0);
    }
}

static void draw(uint8_t RX, uint8_t RY, uint8_t N) {
    CHECK_ADDR_RETN(I, N);

    // Set the flags and get the position from registers RX, RY
    renderFlag = 1;
    drawFlag = 1;
    int x = V[RX];
    int y = V[RY];
    SDL_Rect rect = {.x = 0, .y = 0, .w = PIXEL_D, .h = PIXEL_D};

    int wx, wy; // (Optionally) wrapped values of x,y
    
    VF = 0;
    for (int row=0; row<N; row++) {

        // Wrap value or don't draw off screen
        wy = y + row;
        if (screenWrap)
            wy %= 32;
        else if (wy >= 32)
            break;

        for (int col=0; col<8; col++) {

            wx = x + col;
            if (screenWrap)
                wx %= 64;
            else if (wx >= 64)
                break;

            // Extract the next bit of the sprite, skip if zero
            int bit = MEM[I+row] & (0x80 >> col); 
            if (!bit)
                continue;

            VF = (GFX[wy][wx] == 1); // Set to 1 if a pixel was unset
            GFX[wy][wx] ^= 1; // Toggle pixel
        }
    }
}