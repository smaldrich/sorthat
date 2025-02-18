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

CharSliceSlice main_strSplit(CharSlice chars, char splitter, snz_Arena* arena) {
    SNZ_ARENA_ARR_BEGIN(arena, CharSlice);
    int elemBegin = 0;
    for (int i = 0; i < chars.count; i++) {
        if (chars.elems[i] == splitter) {
            *SNZ_ARENA_PUSH(arena, CharSlice) = (CharSlice){
                .count = i - elemBegin,
                .elems = &chars.elems[elemBegin],
            };
            elemBegin = i + 1;
        }
    }
    *SNZ_ARENA_PUSH(arena, CharSlice) = (CharSlice){
        .count = chars.count - elemBegin,
        .elems = &chars.elems[elemBegin],
    };
    return SNZ_ARENA_ARR_END(arena, CharSlice);
}

bool main_charSliceEqual(CharSlice a, CharSlice b) {
    if (a.count != b.count) {
        return false;
    }
    return memcmp(a.elems, b.elems, a.count) == 0;
}

void main_charSliceTrim(CharSlice* s) {
    CharSlice final = *s;
    for (int i = 0; i < s->count; i++) {
        if (s->elems[i] == ' ') {
            final.count--;
            final.elems++;
        } else {
            break;
        }
    }

    for (int i = s->count; final.count > 0; i--) {
        if (s->elems[i] == ' ') {
            final.count--;
        } else {
            break;
        }
    }
    *s = final;
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

    bool hovered;
    float hoverAnim;
};

SNZ_SLICE(Person);

typedef struct {
    Person* a;
    Person* b;
} PersonPair;

SNZ_SLICE(PersonPair);

typedef struct Room Room;
struct Room {
    PersonPtrSlice people;
    Room* next;
};

PersonSlice people = { 0 };
Room* main_firstRoom = NULL;
snz_Arena main_fileArenaA = { 0 };
snz_Arena main_fileArenaB = { 0 };

#define COL_BACKGROUND HMM_V4(32.0/255, 27.0/255, 27.0/255, 1.0)
#define COL_TEXT HMM_V4(1, 1, 1, 1)
#define COL_HOVERED HMM_V4(1, 1, 1, 0.2)
#define COL_PANEL_A HMM_V4(33.0/255, 38.0/255, 40.0/255, 1.0)
#define COL_PANEL_B HMM_V4(40.0/255, 37.0/255, 33.0/255, 1.0)
#define COL_PANEL_ERROR HMM_V4(99.0/255, 48.0/255, 46.0/255, 1.0)
#define COL_ERROR_TEXT HMM_V4(255.0/255, 141.0/255, 141.0/255, 1.0)
#define TEXT_PADDING 7

void main_pushToArenaIfNotInCollection(void** collection, int64_t collectionCount, void* new, snz_Arena* arena) {
    for (int64_t i = 0; i < collectionCount; i++) {
        void* ptr = collection[i];
        if (ptr == new) {
            return;
        }
    }
    *SNZ_ARENA_PUSH(arena, void*) = new;
}

bool main_personInPersonSlice(Person* p, PersonPtrSlice slice) {
    for (int i = 0; i < slice.count; i++) {
        if (slice.elems[i] == p) {
            return true;
        }
    }
    return false;
}

void main_buildPerson(Person* p, HMM_Vec4 textColor, snz_Arena* scratch) {
    snzu_boxNew(snz_arenaFormatStr(scratch, "%p WOWZER", p));
    snzu_boxSetDisplayStrLen(&main_font, textColor, p->name.elems, p->name.count);
    snzu_boxSetSizeFitText(TEXT_PADDING);

    snzu_Interaction* const inter = SNZU_USE_MEM(snzu_Interaction, "inter");
    snzu_boxSetInteractionOutput(inter, SNZU_IF_HOVER | SNZU_IF_MOUSE_BUTTONS);
    p->hovered |= inter->hovered;
    snzu_boxSetColor(HMM_LerpV4(HMM_V4(0, 0, 0, 0), p->hoverAnim, COL_HOVERED));
}

void main_init(snz_Arena* scratch, SDL_Window* window) {
    assert(scratch || !scratch);
    assert(window || !window);

    main_inst = snzu_instanceInit();
    main_fontArena = snz_arenaInit(10000000, "main font arena");
    main_font = snzr_fontInit(&main_fontArena, scratch, "res/AzeretMono-Regular.ttf", 16);

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
        main_charSliceTrim(&p->name);
    }
    people = SNZ_ARENA_ARR_END(&main_fileArenaB, Person);

    fclose(f);

    { // coloring by gender
        const char* genderStrs[2] = { "Male", "Female" };
        HMM_Vec4 genderColors[2] = {
            COL_PANEL_A,
            COL_PANEL_B,
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

    { // generating/validating wants
        for (int i = 0; i < people.count; i++) {
            Person* p = &people.elems[i];
            CharSlice names = p->fileLine.elems[2];
            if (names.elems[0] == '"') {
                SNZ_ASSERT(names.count >= 2, "empty names cell");
                SNZ_ASSERT(names.elems[names.count - 1] == '"', "names cell w no end quote");
                names.count -= 2;
                names.elems++;
            }
            CharSliceSlice wantedNames = main_strSplit(names, ',', scratch);
            for (int i = 0; i < wantedNames.count; i++) {
                main_charSliceTrim(&wantedNames.elems[i]);
            }

            SNZ_ARENA_ARR_BEGIN(&main_fileArenaA, Person*);
            for (int otherIdx = 0; otherIdx < people.count; otherIdx++) {
                Person* other = &people.elems[otherIdx];
                for (int wantedIdx = 0; wantedIdx < wantedNames.count; wantedIdx++) {
                    CharSlice wanted = wantedNames.elems[wantedIdx];
                    if (main_charSliceEqual(wanted, other->name)) {
                        // FIXME: gender check?
                        *SNZ_ARENA_PUSH(&main_fileArenaA, Person*) = other;
                    }
                }
            }
            p->wants = SNZ_ARENA_ARR_END_NAMED(&main_fileArenaA, Person*, PersonPtrSlice);
        }
    }

    // counting wants
    for (int i = 0; i < people.count; i++) {
        Person* p = &people.elems[i];
        for (int j = 0; j < p->wants.count; j++) {
            Person* other = p->wants.elems[j];
            other->wantsCount++;
        }
    }

    // strong pairs
    SNZ_ARENA_ARR_BEGIN(&main_fileArenaB, PersonPair);
    for (int i = 0; i < people.count; i++) {
        Person* p = &people.elems[i];
        for (int j = i; j < people.count; j++) {
            Person* other = &people.elems[j];

            bool found = false;
            for (int k = 0; k < p->wants.count; k++) {
                if (p->wants.elems[k] == other) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                continue;
            }

            found = false;
            for (int k = 0; k < other->wants.count; k++) {
                if (other->wants.elems[k] == p) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                continue;
            }

            *SNZ_ARENA_PUSH(&main_fileArenaB, PersonPair) = (PersonPair){
                .a = p,
                .b = other,
            };
        }
    }
    PersonPairSlice strongPairs = SNZ_ARENA_ARR_END(&main_fileArenaB, PersonPair);

    // adjacents
    for (int i = 0; i < people.count; i++) {
        Person* p = &people.elems[i];

        SNZ_ARENA_ARR_BEGIN(&main_fileArenaA, Person*);
        void* beginning = main_fileArenaA.end;
        for (int j = 0; j < p->wants.count; j++) {
            int64_t count = (void**)main_fileArenaA.end - (void**)beginning; // FIXME: kill me
            main_pushToArenaIfNotInCollection(beginning, count, p->wants.elems[j], &main_fileArenaA);
        }
        for (int j = i; j < people.count; j++) {
            Person* other = &people.elems[j];
            bool inAdj = false;
            for (int k = 0; k < other->wants.count; k++) {
                if (other->wants.elems[k] == p) {
                    inAdj = true;
                }
            }
            if (!inAdj) {
                continue;
            }
            int64_t count = (void**)main_fileArenaA.end - (void**)beginning; // FIXME: kill me
            main_pushToArenaIfNotInCollection(beginning, count, &people.elems[j], &main_fileArenaA);
        }
        p->adjacents = SNZ_ARENA_ARR_END_NAMED(&main_fileArenaA, Person*, PersonPtrSlice);

        // printf("\n\nadjs for %.*s:\n", (int)p->name.count, p->name.elems);
        // for (int j = 0; j < p->adjacents.count; j++) {
        //     Person* adj = p->adjacents.elems[j];
        //     printf("%.*s, ", (int)adj->name.count, adj->name.elems);
        // }
    }

    { // room gen!!!
        main_firstRoom = NULL;

        PersonPtrSlice peopleRemaining = {
            .count = people.count,
            .elems = SNZ_ARENA_PUSH_ARR(scratch, people.count, Person*),
        };
        for (int i = 0; i < peopleRemaining.count; i++) {
            peopleRemaining.elems[i] = &people.elems[i];
        }

        while (true) { // FIXME: cutoff?
            Room* room = SNZ_ARENA_PUSH(&main_fileArenaA, Room);
            SNZ_ARENA_ARR_BEGIN(&main_fileArenaA, Person*);
            int countInRoom = 0;

            PersonPair* minBasePair = NULL;
            int minBasePairScore = 0;
            for (int i = 0; i < strongPairs.count; i++) {
                PersonPair* p = &strongPairs.elems[i];
                if (main_personInPersonSlice(p->a, peopleRemaining) && main_personInPersonSlice(p->b, peopleRemaining)) {
                    int score = p->a->wantsCount + p->b->wantsCount;
                    if (!minBasePair || score < minBasePairScore) {
                        minBasePair = p;
                        minBasePairScore = score;
                    }
                }
            }

            if (minBasePair) {
                *SNZ_ARENA_PUSH(&main_fileArenaA, Person*) = minBasePair->a;
                *SNZ_ARENA_PUSH(&main_fileArenaA, Person*) = minBasePair->b;
                countInRoom += 2;
            } else {
                Person* found = NULL;
                for (int i = 0; i < peopleRemaining.count; i++) {
                    Person* p = peopleRemaining.elems[i];
                    if (!p) {
                        continue;
                    }
                    found = p;
                    break;
                }
                if (!found) {
                    break;
                }
                *SNZ_ARENA_PUSH(&main_fileArenaA, Person*) = found;
                countInRoom++;
            }

            // only append the room if we know it has ppl in it
            room->next = main_firstRoom;
            main_firstRoom = room;

            while (countInRoom < 4) {
                SNZ_ARENA_ARR_BEGIN(scratch, Person*);
                SNZ_ASSERT(countInRoom == main_fileArenaA.arrModeElemCount, "ew");
                PersonPtrSlice peopleInRoom = (PersonPtrSlice){
                    .count = countInRoom,
                    .elems = (Person**)main_fileArenaA.end - countInRoom, // FIXME: some of the grossest ptr shit i've ever written, so sorry. Please fix up the arena api to let u access this safely
                };
                for (int i = 0; i < peopleInRoom.count; i++) {
                    Person* p = peopleInRoom.elems[i];
                    for (int j = 0; j < p->adjacents.count; j++) {
                        Person* adj = p->adjacents.elems[j];
                        if (!main_personInPersonSlice(adj, peopleRemaining)) {
                            continue;
                        } else if (main_personInPersonSlice(adj, peopleInRoom)) {
                            continue;
                        }
                        *SNZ_ARENA_PUSH(scratch, Person*) = adj;
                    }
                }
                PersonPtrSlice adjacent = SNZ_ARENA_ARR_END_NAMED(scratch, Person*, PersonPtrSlice);
                if (adjacent.count == 0) {
                    break;
                }
                Person* minAdjacent = NULL;
                for (int i = 0; i < adjacent.count; i++) {
                    Person* adj = adjacent.elems[i];
                    if (!minAdjacent || adj->wantsCount < minAdjacent->wantsCount) {
                        minAdjacent = adj;
                    }
                }
                SNZ_ASSERT(minAdjacent, "null adjacent how");

                *SNZ_ARENA_PUSH(&main_fileArenaA, Person*) = minAdjacent;
                countInRoom++;
            }

            room->people = SNZ_ARENA_ARR_END_NAMED(&main_fileArenaA, Person*, PersonPtrSlice);

            for (int i = 0; i < room->people.count; i++) {
                Person* p = room->people.elems[i];
                bool found = false;
                for (int k = 0; k < peopleRemaining.count; k++) {
                    if (peopleRemaining.elems[k] == p) {
                        peopleRemaining.elems[k] = NULL;
                        found = true;
                        break;
                    }
                }
                SNZ_ASSERT(found, "couldn't remove person from remaining bc they weren't there.");
            } // end removing ppl from remaining arr
        }
    } // end room gen
}

void main_loop(float dt, snz_Arena* scratch, snzu_Input inputs, HMM_Vec2 screenSize) {
    snzu_instanceSelect(&main_inst);
    snzu_frameStart(scratch, screenSize, dt);

    snzu_boxNew("main box");
    snzu_boxFillParent();
    snzu_boxSetColor(COL_BACKGROUND);
    snzu_boxScope() {
        snzu_boxNew("left side");
        snzu_boxFillParent();
        snzu_boxSizePctParent(0.5, SNZU_AX_X);
        snzu_boxScope() {
            snzu_boxNew("scroller");
            snzu_boxSetSizeMarginFromParent(10);
            snzu_boxScope() {
                float sizeOfPeopleCol = 0;
                for (int i = 0; i < people.count; i++) {
                    Person* p = &people.elems[i];
                    HMM_Vec2 s = snzr_strSize(&main_font, p->name.elems, p->name.count, main_font.renderedSize);
                    sizeOfPeopleCol = SNZ_MAX(s.X, sizeOfPeopleCol);
                }
                float boxHeight = main_font.renderedSize + 2 * TEXT_PADDING;

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
                        snzu_boxSetBorder(2, HMM_LerpV4(p->genderColor, p->hoverAnim, COL_TEXT));
                        snzu_boxClipChildren(true);
                        snzu_boxScope() {
                            main_buildPerson(p, COL_TEXT, scratch);
                            snzu_boxAlignInParent(SNZU_AX_Y, SNZU_ALIGN_CENTER);
                            snzu_boxAlignInParent(SNZU_AX_X, SNZU_ALIGN_LEFT);

                            snzu_boxNew("others");
                            snzu_boxFillParent();
                            snzu_boxSetStartFromParentAx(sizeOfPeopleCol, SNZU_AX_X);
                            snzu_boxScope() {
                                for (int i = 0; i < p->wants.count; i++) {
                                    Person* other = p->wants.elems[i];
                                    main_buildPerson(other, COL_TEXT, scratch);
                                }
                            }
                            snzu_boxOrderChildrenInRowRecurseAlignEnd(5, SNZU_AX_X);
                        }
                    }
                } // end margin
                snzu_boxOrderChildrenInRowRecurse(5, SNZU_AX_Y);
                snzu_boxSetSizeFitChildren();
            } // end scroller
            snzuc_scrollArea();
        } // end left side

        snzu_boxNew("right side");
        snzu_boxFillParent();
        snzu_boxSizeFromEndPctParent(0.5, SNZU_AX_X);
        snzu_boxSetEndFromParentEndAx(-100, SNZU_AX_Y);
        snzu_boxScope() {
            snzu_boxNew("scroller");
            snzu_boxSetSizeMarginFromParent(10);
            snzu_boxScope() {
                float boxHeight = main_font.renderedSize + 2 * TEXT_PADDING;
                snzu_boxNew("margin");
                snzu_boxSetSizeMarginFromParent(5);
                snzu_boxScope() {
                    HMM_Vec4 numberColor = COL_TEXT;
                    numberColor.A = 0.5;

                    int roomNumber = 1;
                    float roomNumberColWidth = snzr_strSize(&main_font, "200", 2, main_font.renderedSize).X;

                    for (Room* room = main_firstRoom; room; (room = room->next, roomNumber++)) {
                        const char* errString = NULL;
                        snzu_boxNew(snz_arenaFormatStr(scratch, "%p", room));
                        SNZ_ASSERT(room->people.count > 0, "empty room??");
                        HMM_Vec4 color = room->people.elems[0]->genderColor;
                        if (room->people.count <= 2) {
                            color = COL_PANEL_ERROR;
                            if (room->people.count == 1) {
                                errString = "only 1 person.";
                            } else {
                                errString = snz_arenaFormatStr(scratch, "only %lld people.", room->people.count);
                            }
                        }
                        snzu_boxSetColor(color);
                        snzu_boxSetCornerRadius(10);
                        snzu_boxClipChildren(true);
                        snzu_boxFillParent();
                        snzu_boxSetSizeFromStartAx(SNZU_AX_Y, boxHeight);
                        snzu_boxScope() {
                            snzu_boxNew("number");
                            snzu_boxSetDisplayStr(&main_font, numberColor, snz_arenaFormatStr(scratch, "%d", roomNumber));
                            snzu_boxSetSizeFitText(TEXT_PADDING);
                            snzu_boxSetSizeFromStartAx(SNZU_AX_X, roomNumberColWidth + 2 * TEXT_PADDING);

                            for (int i = 0; i < room->people.count; i++) {
                                Person* p = room->people.elems[i];
                                bool anyMatches = false;
                                for (int j = 0; j < p->wants.count; j++) {
                                    Person* wanted = p->wants.elems[j];
                                    if (main_personInPersonSlice(wanted, room->people)) {
                                        anyMatches = true;
                                        break;
                                    }
                                }
                                if (!anyMatches && !errString) {
                                    errString = snz_arenaFormatStr(scratch, "%.*s doesn't like anyone here.", (int)p->name.count, p->name.elems);
                                }
                                main_buildPerson(p, anyMatches ? COL_TEXT : COL_ERROR_TEXT, scratch);
                            }
                        }
                        snzu_boxOrderChildrenInRowRecurse(5, SNZU_AX_X);

                        snzu_boxScope() {
                            if (errString) {
                                snzu_boxNew("err");
                                snzu_boxSetDisplayStr(&main_font, COL_ERROR_TEXT, errString);
                                snzu_boxSetSizeFitText(TEXT_PADDING);
                                snzu_boxAlignInParent(SNZU_AX_X, SNZU_ALIGN_RIGHT);
                                snzu_boxAlignInParent(SNZU_AX_Y, SNZU_ALIGN_CENTER);
                            }
                        }
                    }
                } // end margin
                snzu_boxOrderChildrenInRowRecurse(5, SNZU_AX_Y);
                snzu_boxSetSizeFitChildren();
            } // end scroller
            snzuc_scrollArea();

            snzu_boxNew("right bar");
            snzu_boxSetColor(COL_TEXT);
            snzu_boxFillParent();
            snzu_boxSetSizeFromEndAx(SNZU_AX_Y, 2);
        } // end right side

        snzu_boxNew("center bar");
        snzu_boxSetColor(COL_TEXT);
        snzu_boxFillParent();
        snzu_boxSetSizeFromStartAx(SNZU_AX_X, 2);
        snzu_boxAlignInParent(SNZU_AX_X, SNZU_ALIGN_CENTER);

        for (int i = 0; i < people.count; i++) {
            Person* p = &people.elems[i];
            snzu_easeExp(&p->hoverAnim, p->hovered, 18);
            p->hovered = false;
        }

    } // end main parent

    HMM_Mat4 vp = HMM_Orthographic_RH_NO(0, screenSize.X, screenSize.Y, 0, 0, 10000);
    snzu_frameDrawAndGenInteractions(inputs, vp);
}

int main() {
    snz_main("Sorting hat", "res/sort_hat_logo.bmp", main_init, main_loop);
}