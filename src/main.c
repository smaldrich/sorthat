#include "stdio.h"
#include "stdbool.h"
#include "stdint.h"
#include "snooze.h"
#include "nfd/include/nfd.h"
#include "stb/stb_image.h"

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

    for (int i = s->count - 1; final.count > 0; i--) {
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

typedef struct {
    CharSlice name;
    Person* person; // may be null to indicate failure
} PersonWant;

SNZ_SLICE(PersonWant);

struct Person {
    CharSliceSlice fileLine;

    CharSlice name;
    HMM_Vec4 genderColor;
    PersonWantSlice wantsFromFile;
    PersonPtrSlice validWants;
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

const char* main_loadedPath = NULL;
PersonSlice main_people = { 0 };
Room* main_firstRoom = NULL;
snz_Arena main_fileArenaA = { 0 };
snz_Arena main_fileArenaB = { 0 };

snzr_Texture main_xButton = { 0 };

snzu_Instance main_inst = { 0 };
snzr_Font main_font = { 0 };
snz_Arena main_fontArena = { 0 };

const char* _main_messageBoxMessageSignal = NULL;
bool _main_messageBoxShouldBeError = false;

void main_startMessageBox(const char* msg, bool isError) {
    SNZ_ASSERT(_main_messageBoxMessageSignal == NULL, "non-null message already this frame :(");
    _main_messageBoxMessageSignal = msg;
    _main_messageBoxShouldBeError = isError;
}

#define COL_BACKGROUND HMM_V4(32.0/255, 27.0/255, 27.0/255, 1.0)
#define COL_TEXT HMM_V4(1, 1, 1, 1)
#define COL_HOVERED HMM_V4(1, 1, 1, 0.2)
#define COL_PANEL_A HMM_V4(33.0/255, 38.0/255, 40.0/255, 1.0)
#define COL_PANEL_B HMM_V4(40.0/255, 37.0/255, 33.0/255, 1.0)
#define COL_PANEL_ERROR HMM_V4(99.0/255, 48.0/255, 46.0/255, 1.0)
#define COL_ERROR_TEXT HMM_V4(255.0/255, 141.0/255, 141.0/255, 1.0)
#define TEXT_PADDING 7
#define BORDER_THICKNESS 1

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

void main_clear() {
    snz_arenaClear(&main_fileArenaA);
    snz_arenaClear(&main_fileArenaB);
    main_people = (PersonSlice){ 0 };
    main_firstRoom = NULL;
    main_loadedPath = NULL;
}

// FIXME: this leaks memory, explicitly store room data in one of the file arenas so it gets cleaned up
void main_autogroup(snz_Arena* scratch) {
    // strong pairs
    SNZ_ARENA_ARR_BEGIN(scratch, PersonPair);
    for (int i = 0; i < main_people.count; i++) {
        Person* p = &main_people.elems[i];
        for (int j = i; j < main_people.count; j++) {
            Person* other = &main_people.elems[j];

            bool found = false;
            for (int k = 0; k < p->wantsFromFile.count; k++) {
                if (p->validWants.elems[k] == other) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                continue;
            }

            found = false;
            for (int k = 0; k < other->wantsFromFile.count; k++) {
                if (other->validWants.elems[k] == p) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                continue;
            }

            *SNZ_ARENA_PUSH(scratch, PersonPair) = (PersonPair){
                .a = p,
                .b = other,
            };
        }
    }
    PersonPairSlice strongPairs = SNZ_ARENA_ARR_END(scratch, PersonPair);
    main_firstRoom = NULL;

    // actual room gen
    PersonPtrSlice peopleRemaining = {
        .count = main_people.count,
        .elems = SNZ_ARENA_PUSH_ARR(scratch, main_people.count, Person*),
    };
    for (int i = 0; i < peopleRemaining.count; i++) {
        peopleRemaining.elems[i] = &main_people.elems[i];
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
                SNZ_ARENA_ARR_END_NAMED(&main_fileArenaA, Person*, PersonPtrSlice);
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
} // end autogroup

// return indicates success, 1 good, 0 bad
bool _main_importWithErrors(FILE* f, const char* pathForErrorMessage, snz_Arena* scratch) {
    main_readUntil(f, '\n', scratch); // skip first line bc there are garbage bits + it's not useful

    SNZ_ARENA_ARR_BEGIN(&main_fileArenaB, Person);
    int lineNum = 0;
    while (!feof(f)) {
        lineNum++;
        CharSliceSlice line = main_readCSVLine(f, &main_fileArenaA);
        if (line.count != 3) {
            main_startMessageBox(snz_arenaFormatStr(scratch, "Can't figure out '%s'.\nInvalid formatting on line %d.", pathForErrorMessage, lineNum), true);
            SNZ_ARENA_ARR_END(&main_fileArenaB, Person);
            return false;
        }
        Person* p = SNZ_ARENA_PUSH(&main_fileArenaB, Person);
        p->fileLine = line;
        p->name = p->fileLine.elems[0];
        main_charSliceTrim(&p->name);
    }
    main_people = SNZ_ARENA_ARR_END(&main_fileArenaB, Person);

    if (main_people.count == 0) {
        main_startMessageBox("No people in the file.", true);
        return false;
    }

    return true;
}

// out error str set when any error occurs, will be a user facing string. Str may be formatted into scratch.
void main_import(snz_Arena* scratch) {
    main_clear();
    nfdchar_t* path = NULL;
    {
        nfdresult_t result = NFD_OpenDialog(NULL, NULL, &path);
        if (result != NFD_OKAY) {
            return;
        }
        char* newPath = snz_arenaCopyStr(&main_fileArenaA, path);
        free(path);
        path = newPath;
    }
    FILE* f = fopen(path, "r");
    if (!f) {
        main_startMessageBox(snz_arenaFormatStr(scratch, "Opening file '%s' failed.", path), true);
        return;
    }
    bool importSuccess = _main_importWithErrors(f, path, scratch);
    fclose(f);

    if (!importSuccess) {
        return;
    }

    main_loadedPath = path;

    { // coloring by gender
        const char* genderStrs[2] = { "Male", "Female" };
        HMM_Vec4 genderColors[2] = {
            COL_PANEL_A,
            COL_PANEL_B,
        };
        for (int i = 0; i < main_people.count; i++) {
            Person* p = &main_people.elems[i];
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
        for (int i = 0; i < main_people.count; i++) {
            Person* p = &main_people.elems[i];
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

            SNZ_ARENA_ARR_BEGIN(&main_fileArenaA, PersonWant);
            for (int i = 0; i < wantedNames.count; i++) {
                *SNZ_ARENA_PUSH(&main_fileArenaA, PersonWant) = (PersonWant){
                    .name = wantedNames.elems[i],
                    .person = NULL,
                };
            }
            p->wantsFromFile = SNZ_ARENA_ARR_END(&main_fileArenaA, PersonWant);

            for (int otherIdx = 0; otherIdx < main_people.count; otherIdx++) {
                Person* other = &main_people.elems[otherIdx];
                for (int wantedIdx = 0; wantedIdx < p->wantsFromFile.count; wantedIdx++) {
                    PersonWant* wanted = &p->wantsFromFile.elems[wantedIdx];
                    if (main_charSliceEqual(wanted->name, other->name)) {
                        wanted->person = other;
                    }
                }
            }

            SNZ_ARENA_ARR_BEGIN(&main_fileArenaA, Person*);
            for (int i = 0; i < p->wantsFromFile.count; i++) {
                PersonWant* w = &p->wantsFromFile.elems[i];
                if (w->person) {
                    *SNZ_ARENA_PUSH(&main_fileArenaA, Person*) = w->person;
                }
            }
            p->validWants = SNZ_ARENA_ARR_END_NAMED(&main_fileArenaA, Person*, PersonPtrSlice);

        } // end validating wants
    }

    // counting wants
    for (int i = 0; i < main_people.count; i++) {
        Person* p = &main_people.elems[i];
        for (int j = 0; j < p->wantsFromFile.count; j++) {
            PersonWant* w = &p->wantsFromFile.elems[j];
            if (!w->person) {
                continue;
            }
            w->person->wantsCount++;
        }
    }

    // adjacents
    for (int i = 0; i < main_people.count; i++) {
        Person* p = &main_people.elems[i];

        SNZ_ARENA_ARR_BEGIN(&main_fileArenaA, Person*);
        void* beginning = main_fileArenaA.end;
        for (int j = 0; j < p->wantsFromFile.count; j++) {
            int64_t count = (void**)main_fileArenaA.end - (void**)beginning; // FIXME: kill me
            main_pushToArenaIfNotInCollection(beginning, count, p->validWants.elems[j], &main_fileArenaA);
        }
        for (int j = i; j < main_people.count; j++) {
            Person* other = &main_people.elems[j];
            bool inAdj = false;
            for (int k = 0; k < other->wantsFromFile.count; k++) {
                if (other->validWants.elems[k] == p) {
                    inAdj = true;
                }
            }
            if (!inAdj) {
                continue;
            }
            int64_t count = (void**)main_fileArenaA.end - (void**)beginning; // FIXME: kill me
            main_pushToArenaIfNotInCollection(beginning, count, &main_people.elems[j], &main_fileArenaA);
        }
        p->adjacents = SNZ_ARENA_ARR_END_NAMED(&main_fileArenaA, Person*, PersonPtrSlice);

        // printf("\n\nadjs for %.*s:\n", (int)p->name.count, p->name.elems);
        // for (int j = 0; j < p->adjacents.count; j++) {
        //     Person* adj = p->adjacents.elems[j];
        //     printf("%.*s, ", (int)adj->name.count, adj->name.elems);
        // }
    }

    main_autogroup(scratch);
    main_startMessageBox(snz_arenaFormatStr(scratch, "Imported file from '%s'.", main_loadedPath), false);
}

void main_export(snz_Arena* scratch) {
    if (!main_firstRoom) {
        main_startMessageBox("Can't export, there aren't any rooms yet", true);
        return;
    }

    nfdchar_t* outPath = NULL;
    nfdresult_t result = NFD_SaveDialog(NULL, NULL, &outPath);
    if (result != NFD_OKAY) {
        return;
    } else if (strcmp(outPath, main_loadedPath) == 0) {
        main_startMessageBox("You probably shouldn't overwrite the file with your people data in it.", true);
        free(outPath);
        return;
    }

    FILE* f = fopen(outPath, "w");
    if (!f) {
        main_startMessageBox(snz_arenaFormatStr(scratch, "Opening file '%s' failed.", outPath), true);
    }

    for (Room* room = main_firstRoom; room; room = room->next) {
        for (int i = 0; i < room->people.count; i++) {
            Person* p = room->people.elems[i];
            fprintf(f, "%.*s,", (int)p->name.count, p->name.elems);
        }
        fprintf(f, "\n");
    }
    fclose(f);

    main_startMessageBox(snz_arenaFormatStr(scratch, "Saved rooms to '%s'.", outPath), false);
    free(outPath);
}

void main_init(snz_Arena* scratch, SDL_Window* window) {
    assert(scratch || !scratch);
    assert(window || !window);

    main_inst = snzu_instanceInit();
    main_fontArena = snz_arenaInit(10000000, "main font arena");
    main_font = snzr_fontInit(&main_fontArena, scratch, "res/AzeretMono-Regular.ttf", 16);

    main_fileArenaA = snz_arenaInit(10000000, "main file arena A");
    main_fileArenaB = snz_arenaInit(10000000, "main file arena B");

    int w, h, bpp;
    stbi_set_flip_vertically_on_load(1);
    uint8_t* data = stbi_load("res/x_button.png", &w, &h, &bpp, 4);
    SNZ_ASSERT(data, "x button texture load failed.");
    main_xButton = snzr_textureInitRBGA(w, h, data);
}

bool main_button(const char* label) {
    snzu_boxNew(label);
    snzu_boxSetDisplayStr(&main_font, COL_TEXT, label);
    snzu_boxSetCornerRadius(10);
    snzu_boxSetBorder(1, COL_TEXT);

    snzu_Interaction* const inter = SNZU_USE_MEM(snzu_Interaction, "inter");
    snzu_boxSetInteractionOutput(inter, SNZU_IF_HOVER | SNZU_IF_MOUSE_BUTTONS);

    float* const hoverAnim = SNZU_USE_MEM(float, "hoverAnim");
    snzu_easeExp(hoverAnim, inter->hovered, 15);

    float* const pressAnim = SNZU_USE_MEM(float, "pressAnim");
    if (inter->dragged) {
        *pressAnim = 1;
    }
    snzu_easeExp(pressAnim, false, 10);
    snzu_boxSetSizeFitText(TEXT_PADDING + 2 * *pressAnim);

    snzu_boxSetColor(HMM_LerpV4(HMM_V4(0, 0, 0, 0), *hoverAnim, COL_HOVERED));
    return inter->mouseActions[SNZU_MB_LEFT] == SNZU_ACT_UP;
}

// if message non-null will start a message box and fade it out over time.
// sets the message ptr to null
// the string may be freed immediately after calling this
void main_messageBoxBuild(bool error, bool explicitClose, const char** message) {
    snzu_boxNew("messageBox");
    float* fadeAnim = SNZU_USE_MEM(float, "fadeAnim");

    if (*message) {
        *fadeAnim = 2.5;
    }

    if (*fadeAnim > 0.001) {
        int count = *message ? strlen(*message) : 0;
        char* const messageCopy = SNZU_USE_ARRAY(char, count, "msg"); // FIXME: segfault potential if an error gets overwritten with a longer string because usearr doesn't cope w changing sizes
        if (*message) {
            strcpy(messageCopy, *message);
        }

        snzu_boxSizePctParent(.3, SNZU_AX_X);
        snzu_boxSizePctParent(.3, SNZU_AX_Y);
        snzu_boxAlignInParent(SNZU_AX_X, SNZU_ALIGN_CENTER);
        snzu_boxAlignInParent(SNZU_AX_Y, SNZU_ALIGN_CENTER);
        snzu_boxSetCornerRadius(10);


        if (explicitClose) {
            *fadeAnim = 1;
        } else {
            snzu_easeLinearUnbounded(fadeAnim, 0, 1);
        }

        if (explicitClose) {
            snzu_boxScope() {
                snzu_boxNew("x");
                snzu_boxSetSizeMarginFromParent(10);
                snzu_boxSetSizeFromStartAx(SNZU_AX_Y, 25);
                snzu_boxSetSizeFromEndAx(SNZU_AX_X, 25);
                snzu_boxSetTexture(main_xButton);

                snzu_Interaction* inter = SNZU_USE_MEM(snzu_Interaction, "inter");
                snzu_boxSetInteractionOutput(inter, SNZU_IF_HOVER | SNZU_IF_MOUSE_BUTTONS);
                float* const xHover = SNZU_USE_MEM(float, "highlight");
                snzu_easeExp(xHover, inter->hovered, 20);
                snzu_boxHighlightByAnim(xHover, HMM_V4(0.5, 0.5, 0.5, 1.0), 0.4);

                if (inter->mouseActions[SNZU_MB_LEFT] == SNZU_ACT_DOWN) {
                    *fadeAnim = 0;
                } else if (inter->keyCode == SDLK_ESCAPE) {
                    *fadeAnim = 0;
                }
            }
        } // end lil x button

        float lerpT = SNZ_MIN(1, *fadeAnim);
        if (lerpT < 0) {
            lerpT = 0;
        }

        HMM_Vec4 targetBorderCol = error ? COL_ERROR_TEXT : COL_TEXT;
        snzu_boxSetBorder(BORDER_THICKNESS, HMM_Lerp(HMM_V4(0, 0, 0, 0), lerpT, targetBorderCol));
        snzu_boxSetColor(HMM_Lerp(HMM_V4(0, 0, 0, 0), lerpT, COL_BACKGROUND));
        snzu_boxSetDisplayStr(&main_font, HMM_Lerp(HMM_V4(0, 0, 0, 0), lerpT, COL_TEXT), messageCopy);
    } // end not faded check

    *message = NULL;
}

void main_loop(float dt, snz_Arena* scratch, snzu_Input inputs, HMM_Vec2 screenSize) {
    snzu_instanceSelect(&main_inst);
    snzu_frameStart(scratch, screenSize, dt);

    snzu_boxNew("main box");
    snzu_boxFillParent();
    snzu_boxSetColor(COL_BACKGROUND);
    snzu_boxScope() {
        snzu_boxNew("left buttons");
        snzu_boxFillParent();
        snzu_boxSetSizeFromStartAx(SNZU_AX_X, 130);
        snzu_boxScope() {
            snzu_boxNew("margin");
            snzu_boxSetSizeMarginFromParent(10);
            snzu_boxScope() {
                if (main_button("clear")) {
                    main_clear();
                }
                if (main_button("import")) {
                    main_import(scratch);
                }
                if (main_button("autogroup")) {
                    main_autogroup(scratch);
                }
                if (main_button("export")) {
                    main_export(scratch);
                }
            }
            snzu_boxOrderChildrenInRowRecurse(5, SNZU_AX_Y);

            snzu_boxNew("border");
            snzu_boxSetColor(COL_TEXT);
            snzu_boxFillParent();
            snzu_boxSetSizeFromEndAx(SNZU_AX_X, BORDER_THICKNESS);
        }

        snzu_boxNew("main area");
        snzu_boxFillParent();
        snzu_boxSetSizeFromEndAx(SNZU_AX_X, snzu_boxGetSize().X - 130);
        snzu_boxScope() {
            snzu_boxNew("left side");
            snzu_boxFillParent();
            snzu_boxSizePctParent(0.5, SNZU_AX_X);
            snzu_boxScope() {
                snzu_boxNew("scroller");
                snzu_boxSetSizeMarginFromParent(10);
                snzu_boxScope() {
                    float sizeOfPeopleCol = 0;
                    for (int i = 0; i < main_people.count; i++) {
                        Person* p = &main_people.elems[i];
                        HMM_Vec2 s = snzr_strSize(&main_font, p->name.elems, p->name.count, main_font.renderedSize);
                        sizeOfPeopleCol = SNZ_MAX(s.X, sizeOfPeopleCol);
                    }
                    float boxHeight = main_font.renderedSize + 2 * TEXT_PADDING;

                    snzu_boxNew("margin");
                    snzu_boxSetSizeMarginFromParent(5);
                    snzu_boxScope() {
                        for (int i = 0; i < main_people.count; i++) {
                            snzu_boxNew(snz_arenaFormatStr(scratch, "%d", i));
                            Person* p = &main_people.elems[i];
                            snzu_boxSetColor(p->genderColor);
                            snzu_boxSetCornerRadius(10);
                            snzu_boxFillParent();
                            snzu_boxSetSizeFromStartAx(SNZU_AX_Y, boxHeight);
                            snzu_boxSetBorder(BORDER_THICKNESS, HMM_LerpV4(p->genderColor, p->hoverAnim, COL_TEXT));
                            snzu_boxClipChildren(true);
                            snzu_boxScope() {
                                main_buildPerson(p, COL_TEXT, scratch);
                                snzu_boxAlignInParent(SNZU_AX_Y, SNZU_ALIGN_CENTER);
                                snzu_boxAlignInParent(SNZU_AX_X, SNZU_ALIGN_LEFT);

                                snzu_boxNew("others");
                                snzu_boxFillParent();
                                snzu_boxSetStartFromParentAx(sizeOfPeopleCol, SNZU_AX_X);
                                snzu_boxScope() {
                                    for (int i = 0; i < p->wantsFromFile.count; i++) {
                                        PersonWant w = p->wantsFromFile.elems[i];
                                        if (w.person) {
                                            main_buildPerson(w.person, COL_TEXT, scratch);
                                        } else {
                                            snzu_boxNew(snz_arenaFormatStr(scratch, "%s", w.name));
                                            snzu_boxSetDisplayStrLen(&main_font, COL_ERROR_TEXT, w.name.elems, w.name.count);
                                            snzu_boxSetSizeFitText(TEXT_PADDING);
                                        }
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
                                    for (int j = 0; j < p->wantsFromFile.count; j++) {
                                        Person* wanted = p->validWants.elems[j];
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
            } // end right side

            snzu_boxNew("center bar");
            snzu_boxSetColor(COL_TEXT);
            snzu_boxFillParent();
            snzu_boxSetSizeFromStartAx(SNZU_AX_X, BORDER_THICKNESS);
            snzu_boxAlignInParent(SNZU_AX_X, SNZU_ALIGN_CENTER);
        } // end container for main ui

        for (int i = 0; i < main_people.count; i++) {
            Person* p = &main_people.elems[i];
            snzu_easeExp(&p->hoverAnim, p->hovered, 23);
            p->hovered = false;
        }

        main_messageBoxBuild(_main_messageBoxShouldBeError, _main_messageBoxShouldBeError, &_main_messageBoxMessageSignal);
    } // end main parent

    HMM_Mat4 vp = HMM_Orthographic_RH_NO(0, screenSize.X, screenSize.Y, 0, 0, 10000);
    snzu_frameDrawAndGenInteractions(inputs, vp);
}

int main() {
    snz_main("Sorting hat", "res/sort_hat_logo.bmp", main_init, main_loop);
}