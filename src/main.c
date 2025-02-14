#include "stdio.h"
#include "stdbool.h"
#include "stdint.h"
#include "snooze.h"

snzu_Instance main_inst = { 0 };
snzr_Font main_font = { 0 };
snz_Arena main_fontArena = { 0 };

const char* main_readUntil(FILE* f, char terminator, snz_Arena* arena) {
    SNZ_ARENA_ARR_BEGIN(arena, char);
    char c = 0;
    do {
        int read = fread(&c, 1, 1, f);
        if (read != 1) {
            *SNZ_ARENA_PUSH(arena, char) = 0;
            break;
        }
        *SNZ_ARENA_PUSH(arena, char) = c;
    } while (c != terminator);
    charSlice chars = SNZ_ARENA_ARR_END(arena, char);
    chars.elems[chars.count - 1] = 0; // replace terminator with a NULL
    return chars.elems;
}

const char* main_firstLine = NULL;
snz_Arena main_fileArena = { 0 };

void main_init(snz_Arena* scratch, SDL_Window* window) {
    assert(scratch || !scratch);
    assert(window || !window);

    main_inst = snzu_instanceInit();
    main_fontArena = snz_arenaInit(10000000, "main font arena");
    main_font = snzr_fontInit(&main_fontArena, scratch, "res/OpenSans-Light.ttf", 32);

    main_fileArena = snz_arenaInit(10000000, "main file arena");

    FILE* f = fopen("hotel room sort data_v1.csv", "r");
    SNZ_ASSERT(f, "opening file failed.");
    main_readUntil(f, '\n', scratch);
    main_firstLine = main_readUntil(f, '\n', &main_fileArena);
    fclose(f);
}

void main_loop(float dt, snz_Arena* scratch, snzu_Input inputs, HMM_Vec2 screenSize) {
    assert(dt || !dt);
    assert(scratch || !scratch);
    assert(inputs.mouseScrollY || !inputs.mouseScrollY);
    assert(screenSize.X || !screenSize.X);

    snzu_instanceSelect(&main_inst);
    snzu_frameStart(scratch, screenSize, dt);

    snzu_boxNew("main box");
    snzu_boxFillParent();
    snzu_boxSetColor(HMM_V4(1, 1, 1, 1));
    snzu_boxSetDisplayStr(&main_font, HMM_V4(1, 0, 0, 1), main_firstLine);

    HMM_Mat4 vp = HMM_Orthographic_RH_NO(0, screenSize.X, screenSize.Y, 0, 0, 10000);
    snzu_frameDrawAndGenInteractions(inputs, vp);
}

int main() {
    snz_main("Sorting hat", "res/sort_hat_logo.bmp", main_init, main_loop);
}