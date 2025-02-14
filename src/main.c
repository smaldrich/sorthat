#include "stdio.h"
#include "stdbool.h"
#include "stdint.h"
#include "snooze.h"

snzu_Instance main_inst = { 0 };
snzr_Font main_font = { 0 };
snz_Arena main_fontArena = { 0 };

SNZ_SLICE_NAMED(char, CharSlice);
SNZ_SLICE(CharSlice);

// out str is null terminated as well as counted
CharSlice main_readUntil(FILE* f, char terminator, snz_Arena* arena) {
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
    CharSlice chars = SNZ_ARENA_ARR_END_NAMED(arena, char, CharSlice);
    chars.elems[chars.count - 1] = 0; // replace terminator with a NULL
    chars.count--;
    return chars;
}


CharSliceSlice main_readCSVLine(FILE* f, snz_Arena* arena) {
    CharSlice line = main_readUntil(f, '\n', arena);
    SNZ_ARENA_ARR_BEGIN(arena, CharSlice);
    int elemBegin = 0;
    bool escaped = false;
    for (int i = 0; i < line.count; i++) {
        char c = line.elems[i];
        if (c == ',') {
            if (!escaped) {
                int count = i - elemBegin;
                if (count > 0) {
                    *SNZ_ARENA_PUSH(arena, CharSlice) = (CharSlice){
                        .elems = &line.elems[elemBegin],
                        .count = count,
                    };
                }
                elemBegin = i + 1;
            }
        } else if (c == '\"') {
            escaped = !escaped;
        }
    }
    return SNZ_ARENA_ARR_END(arena, CharSlice);
}

typedef struct Person Person;
SNZ_SLICE_NAMED(Person*, PersonPtrSlice);

struct Person {
    CharSliceSlice fileLine;

    CharSlice name;
    HMM_Vec4 genderColor;
    PersonPtrSlice wants;
    PersonPtrSlice adjacents;
    int wantsCount;
};

SNZ_SLICE(Person);

PersonSlice people = { 0 };
snz_Arena main_fileArenaA = { 0 };
snz_Arena main_fileArenaB = { 0 };

void main_init(snz_Arena* scratch, SDL_Window* window) {
    assert(scratch || !scratch);
    assert(window || !window);

    main_inst = snzu_instanceInit();
    main_fontArena = snz_arenaInit(10000000, "main font arena");
    main_font = snzr_fontInit(&main_fontArena, scratch, "res/OpenSans-Light.ttf", 24);

    main_fileArenaA = snz_arenaInit(10000000, "main file arena A");
    main_fileArenaB = snz_arenaInit(10000000, "main file arena B");

    FILE* f = fopen("hotel room sort data_v1.csv", "r");
    SNZ_ASSERT(f, "opening file failed.");

    main_readUntil(f, '\n', scratch); // skip first line bc there are garbage bits + it's not useful

    SNZ_ARENA_ARR_BEGIN(&main_fileArenaB, Person);
    while (!feof(f)) {
        Person* p = SNZ_ARENA_PUSH(&main_fileArenaB, Person);
        p->fileLine = main_readCSVLine(f, &main_fileArenaA);
        SNZ_ASSERTF(p->fileLine.count == 3, "Person file line had %d elts.", p->fileLine.count);
        p->name = p->fileLine.elems[0];
    }
    people = SNZ_ARENA_ARR_END(&main_fileArenaB, Person);

    fclose(f);

    const char* genderStrs[2] = { "Male", "Female" };
    HMM_Vec4 genderColors[2] = {
        HMM_V4(0.8, 0.9, 0.95, 1),
        HMM_V4(0.95, 0.9, 0.8, 1),
    };
    for (int i = 0; i < people.count; i++) {
        Person* p = &people.elems[i];
        for (int i = 0; i < 2; i++) {
            int minLen = SNZ_MIN(strlen(genderStrs[i]), (uint64_t)p->name.count);
            if (strncmp(genderStrs[i], p->fileLine.elems[1].elems, minLen) == 0) {
                p->genderColor = genderColors[i];
                break;
            }
        }
        SNZ_ASSERT(p->genderColor.A != 0, "person didn't find a gender color");
    }
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
    snzu_boxScope() {
        snzu_boxNew("left side");
        snzu_boxFillParent();
        snzu_boxSizePctParent(0.5, SNZU_AX_X);
        snzu_boxScope() {
            snzu_boxNew("scroller");
            snzu_boxSetSizeMarginFromParent(10);
            snzu_boxSetColor(HMM_V4(0.95, 0.95, 0.95, 1));
            snzu_boxScope() {
                float sizeOfPeopleCol = 0;
                for (int i = 0; i < people.count; i++) {
                    Person* p = &people.elems[i];
                    HMM_Vec2 s = snzr_strSize(&main_font, p->name.elems, p->name.count, main_font.renderedSize);
                    sizeOfPeopleCol = SNZ_MAX(s.X, sizeOfPeopleCol);
                }
                float textPadding = 5;
                float boxHeight = main_font.renderedSize + 2 * textPadding;

                snzu_boxNew("margin");
                snzu_boxSetSizeMarginFromParent(5);
                snzu_boxScope() {
                    for (int i = 0; i < people.count; i++) {
                        snzu_boxNew(snz_arenaFormatStr(scratch, "%d", i));
                        Person* p = &people.elems[i];
                        snzu_boxSetColor(p->genderColor);
                        snzu_boxSetCornerRadius(10);
                        snzu_boxFillParent();
                        snzu_boxSetSizeFromStartAx(SNZU_AX_Y, boxHeight);
                        snzu_boxScope() {
                            snzu_boxNew("name");
                            snzu_boxSetDisplayStrLen(&main_font, HMM_V4(0, 0, 0, 1), p->name.elems, p->name.count);
                            snzu_boxSetSizeFitText(textPadding);
                            snzu_boxAlignInParent(SNZU_AX_Y, SNZU_ALIGN_CENTER);
                            snzu_boxAlignInParent(SNZU_AX_X, SNZU_ALIGN_LEFT);
                        }
                    }
                } // end margin
                snzu_boxOrderChildrenInRowRecurse(5, SNZU_AX_Y);
                snzu_boxSetSizeFitChildren();
            } // end scroller
            snzuc_scrollArea();
        } // end left side
    }

    HMM_Mat4 vp = HMM_Orthographic_RH_NO(0, screenSize.X, screenSize.Y, 0, 0, 10000);
    snzu_frameDrawAndGenInteractions(inputs, vp);
}

int main() {
    snz_main("Sorting hat", "res/sort_hat_logo.bmp", main_init, main_loop);
}