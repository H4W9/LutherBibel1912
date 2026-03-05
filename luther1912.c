// Luther Bibel 1912 Viewer for Flipper Zero
// SD card: /ext/apps_data/luther1912/<Section>/<Book>/<Chapter>/verseN.txt

#define APP_VERSION "1.4"
#define APP_NAME    "FZ Bible"

#include "font/font.h"
#include "luther1912.h"
#include "keyboard/keyboard.h"
#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <input/input.h>
#include <storage/storage.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Font table (file-local)
static const uint8_t FONT_CHARS[FONT_COUNT]  = { 22, 30, 24, 20, 13 };
static const uint8_t FONT_LINE_H[FONT_COUNT] = { 10,  8, 10, 12, 16 };
static const char* const FONT_LABELS[FONT_COUNT] = {
    "Default (built-in)",
    "Tiny    (4x6)",
    "Small   (5x8)",
    "Medium  (6x10)",
    "Large   (9x15)",
};

// Canonical book order (folder names on SD card, underscores for spaces)
// 79 books: AT(22), Propheten(17), Apokryphen(13), NT(27)

// folder      = German SD folder name (primary; Luther 1912 layout)
// folder_en   = English SD folder name (NULL if same as German or no EN alias)
//               Matches the Python script's BOOK_MAP_EN values exactly.
// display_label = Proper German name with umlauts for on-screen display only.
//                 NULL means derive the label from the folder name as before
//                 (underscores → spaces). Used only by canon_book_label() /
//                 current_book_label(); never used for filesystem paths.
typedef struct {
    const char* folder;
    const char* folder_en;
    const char* display_label;
    uint8_t     section;
} BookDef;

#define SEC_AT  0
#define SEC_PR  1
#define SEC_AP  2
#define SEC_NT  3

static const BookDef CANON_BOOKS[] = {
    // { folder, folder_en, display_label, section }
    // display_label is NULL when the folder name (underscores->spaces) is already correct
    // Altes Testament / Old Testament
    { "1_Mose",           "Genesis",           "1 Mose",                  SEC_AT },
    { "2_Mose",           "Exodus",            "2 Mose",                  SEC_AT },
    { "3_Mose",           "Leviticus",         "3 Mose",                  SEC_AT },
    { "4_Mose",           "Numbers",           "4 Mose",                  SEC_AT },
    { "5_Mose",           "Deuteronomy",       "5 Mose",                  SEC_AT },
    { "Josua",            "Joshua",            NULL,                      SEC_AT },
    { "Richter",          "Judges",            NULL,                      SEC_AT },
    { "Ruth",             NULL,                NULL,                      SEC_AT },
    { "1_Samuel",         NULL,                "1 Samuel",                SEC_AT },
    { "2_Samuel",         NULL,                "2 Samuel",                SEC_AT },
    { "1_Konige",         "1_Kings",           "1 K\xc3\xb6" "nige",       SEC_AT },
    { "2_Konige",         "2_Kings",           "2 K\xc3\xb6" "nige",       SEC_AT },
    { "1_Chronik",        "1_Chronicles",      "1 Chronik",               SEC_AT },
    { "2_Chronik",        "2_Chronicles",      "2 Chronik",               SEC_AT },
    { "Esra",             "Ezra",              NULL,                      SEC_AT },
    { "Nehemia",          "Nehemiah",          NULL,                      SEC_AT },
    { "Esther",           NULL,                NULL,                      SEC_AT },
    { "Hiob",             "Job",               NULL,                      SEC_AT },
    { "Psalm",            "Psalms",            NULL,                      SEC_AT },
    { "Spruche",          "Proverbs",          "Spr\xc3\xbc" "che",        SEC_AT },
    { "Prediger",         "Ecclesiastes",      NULL,                      SEC_AT },
    { "Hohelied",         "Song_of_Solomon",   NULL,                      SEC_AT },
    // Propheten / Prophets
    { "Jesaja",           "Isaiah",            NULL,                      SEC_PR },
    { "Jeremia",          "Jeremiah",          NULL,                      SEC_PR },
    { "Klagelieder",      "Lamentations",      NULL,                      SEC_PR },
    { "Hesekiel",         "Ezekiel",           NULL,                      SEC_PR },
    { "Daniel",           NULL,                NULL,                      SEC_PR },
    { "Hosea",            NULL,                NULL,                      SEC_PR },
    { "Joel",             NULL,                NULL,                      SEC_PR },
    { "Amos",             NULL,                NULL,                      SEC_PR },
    { "Obadja",           "Obadiah",           NULL,                      SEC_PR },
    { "Jona",             "Jonah",             NULL,                      SEC_PR },
    { "Micha",            "Micah",             NULL,                      SEC_PR },
    { "Nahum",            NULL,                NULL,                      SEC_PR },
    { "Habakuk",          "Habakkuk",          NULL,                      SEC_PR },
    { "Zephanja",         "Zephaniah",         NULL,                      SEC_PR },
    { "Haggai",           NULL,                NULL,                      SEC_PR },
    { "Sacharja",         "Zechariah",         NULL,                      SEC_PR },
    { "Maleachi",         "Malachi",           NULL,                      SEC_PR },
    // Neues Testament / New Testament
    { "Matthaus",         "Matthew",           "Matth\xc3\xa4" "us",       SEC_NT },
    { "Markus",           "Mark",              NULL,                      SEC_NT },
    { "Lukas",            "Luke",              NULL,                      SEC_NT },
    { "Johannes",         "John",              NULL,                      SEC_NT },
    { "Apostelgeschichte","Acts",              NULL,                      SEC_NT },
    { "Romer",            "Romans",            "R\xc3\xb6" "mer",          SEC_NT },
    { "1_Korinther",      "1_Corinthians",     "1 Korinther",             SEC_NT },
    { "2_Korinther",      "2_Corinthians",     "2 Korinther",             SEC_NT },
    { "Galater",          "Galatians",         NULL,                      SEC_NT },
    { "Epheser",          "Ephesians",         NULL,                      SEC_NT },
    { "Philipper",        "Philippians",       NULL,                      SEC_NT },
    { "Kolosser",         "Colossians",        NULL,                      SEC_NT },
    { "1_Thessalonicher", "1_Thessalonians",   "1 Thessalonicher",        SEC_NT },
    { "2_Thessalonicher", "2_Thessalonians",   "2 Thessalonicher",        SEC_NT },
    { "1_Timotheus",      "1_Timothy",         "1 Timotheus",             SEC_NT },
    { "2_Timotheus",      "2_Timothy",         "2 Timotheus",             SEC_NT },
    { "Titus",            NULL,                NULL,                      SEC_NT },
    { "Philemon",         NULL,                NULL,                      SEC_NT },
    { "Hebraer",          "Hebrews",           "Hebr\xc3\xa4" "er",        SEC_NT },
    { "Jakobus",          "James",             NULL,                      SEC_NT },
    { "1_Petrus",         "1_Peter",           "1 Petrus",                SEC_NT },
    { "2_Petrus",         "2_Peter",           "2 Petrus",                SEC_NT },
    { "1_Johannes",       "1_John",            "1 Johannes",              SEC_NT },
    { "2_Johannes",       "2_John",            "2 Johannes",              SEC_NT },
    { "3_Johannes",       "3_John",            "3 Johannes",              SEC_NT },
    { "Judas",            "Jude",              NULL,                      SEC_NT },
    { "Offenbarung",      "Revelation",        NULL,                      SEC_NT },
    // Apokryphen / Apocrypha
    { "Judith",           NULL,                NULL,                      SEC_AP },
    { "Weisheit",         "Wisdom_of_Solomon", NULL,                      SEC_AP },
    { "Tobias",           "Tobit",             NULL,                      SEC_AP },
    { "Sirach",           NULL,                NULL,                      SEC_AP },
    { "Baruch",           NULL,                NULL,                      SEC_AP },
    { "1_Makkabaer",      "1_Maccabees",       "1 Makkab\xc3\xa4" "er",   SEC_AP },
    { "2_Makkabaer",      "2_Maccabees",       "2 Makkab\xc3\xa4" "er",   SEC_AP },
    { "3_Makkabaer",      NULL,                "3 Makkab\xc3\xa4" "er",   SEC_AP },
    { "Zusatze_Esther",   "Additions_to_Esther","Zus\xc3\xa4" "tze Esther",SEC_AP },
    { "Zusatze_Daniel",   "Additions_to_Daniel","Zus\xc3\xa4" "tze Daniel",SEC_AP },
    { "1_Esdras",         NULL,                "1 Esdras",                SEC_AP },
    { "2_Esdras",         NULL,                "2 Esdras",                SEC_AP },
    { "Gebet_Manasse",    "Prayer_of_Manasseh","Gebet Manasse",           SEC_AP },
};
#define CANON_BOOK_COUNT  ((uint8_t)(sizeof(CANON_BOOKS)/sizeof(CANON_BOOKS[0])))

// German section folder names (primary layout – Luther 1912, etc.)
static const char* const SECTION_ORDER[4] = {
    "Altes_Testament",
    "Propheten",
    "Apokryphen",
    "Neues_Testament",
};
// English section folder names (generated by the Python script with --lang en)
static const char* const SECTION_ORDER_EN[4] = {
    "Old_Testament",
    "Prophets",
    "Apocrypha",
    "New_Testament",
};
static const char* const SECTION_LABELS[4] = {
    "Altes Testament",
    "Propheten",
    "Apokryphen",
    "Neues Testament",
};
static const char* const SECTION_LABELS_EN[4] = {
    "Old Testament",
    "Prophets",
    "Apocrypha",
    "New Testament",
};

// Per-translation flag: true if the active translation uses English folder names.
// Detected once in rebuild_section_list() by probing which name exists on disk.
// Stored in App so every path builder can query it cheaply.
//
// Helper: return the actual section folder name to use for section index s.
static const char* section_folder(App* app, uint8_t s) {
    return app->section_lang_en ? SECTION_ORDER_EN[s] : SECTION_ORDER[s];
}

// Helper: return the actual book folder name to use for a canonical book index.
// Tries the German name first; if that is absent (folder_en != NULL and the
// caller is on an EN-layout card) it falls back to the English name.
// NOTE: path probing is done by the caller; this just selects the name.
static const char* book_folder_name(App* app, uint8_t canon_idx) {
    if(app->section_lang_en && CANON_BOOKS[canon_idx].folder_en != NULL)
        return CANON_BOOKS[canon_idx].folder_en;
    return CANON_BOOKS[canon_idx].folder;
}

// Dark mode color helpers
static void set_bg(Canvas* canvas, App* app) {
    if(app->dark_mode) canvas_set_color(canvas, ColorBlack);
    else               canvas_set_color(canvas, ColorWhite);
}
void set_fg(Canvas* canvas, App* app) {
    if(app->dark_mode) canvas_set_color(canvas, ColorWhite);
    else               canvas_set_color(canvas, ColorBlack);
}

// Font helpers

static void apply_font(Canvas* canvas, FontChoice f) {
    switch(f) {
    case FontSmall:  canvas_set_font_custom(canvas, FONT_SIZE_SMALL);  break;
    case FontMedium: canvas_set_font_custom(canvas, FONT_SIZE_MEDIUM); break;
    case FontLarge:  canvas_set_font_custom(canvas, FONT_SIZE_LARGE);  break;
    case FontXLarge: canvas_set_font_custom(canvas, FONT_SIZE_XLARGE); break;
    default:         canvas_set_font(canvas, FontSecondary);           break;
    }
}

static uint8_t font_visible_lines(FontChoice f) {
    return (uint8_t)((SCREEN_H - READ_BODY_Y) / FONT_LINE_H[f]);
}

// Settings persistence

static void settings_save(App* app) {
    File* f = storage_file_alloc(app->storage);
    if(!storage_file_open(f, SETTINGS_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_free(f);
        return;
    }
    char buf[128];
    int len = snprintf(buf, sizeof(buf),
        "font=%d\ndark=%d\nsec=%d\nbook=%d\nchap=%d\nverse=%d\ntrans=%d\n",
        (int)app->font_choice,
        (int)app->dark_mode,
        (int)app->section_idx,
        (int)app->book_idx,
        (int)app->chapter_idx,
        (int)app->verse_idx,
        (int)app->translation_idx);
    if(len > 0) storage_file_write(f, buf, (uint16_t)len);
    storage_file_close(f);
    storage_file_free(f);
}

static void settings_load(App* app) {
    // Defaults
    app->font_choice  = FontSecondaryBuiltin;
    app->dark_mode    = false;
    app->section_idx  = 0;
    app->book_idx     = 0;
    app->chapter_idx  = 0;
    app->verse_idx    = 0;

    File* f = storage_file_alloc(app->storage);
    if(!storage_file_open(f, SETTINGS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(f);
        return;
    }
    char buf[128];
    uint16_t rd = storage_file_read(f, buf, sizeof(buf) - 1);
    buf[rd] = '\0';
    storage_file_close(f);
    storage_file_free(f);

    char* p;
    if((p = strstr(buf, "font="))  != NULL) { int v=atoi(p+5); if(v>=0&&v<FONT_COUNT) app->font_choice=(FontChoice)v; }
    if((p = strstr(buf, "dark="))  != NULL) { app->dark_mode = (atoi(p+5) != 0); }
    if((p = strstr(buf, "sec="))   != NULL) { int v=atoi(p+4); if(v>=0&&v<4) app->section_idx=(uint8_t)v; }
    if((p = strstr(buf, "book="))  != NULL) { int v=atoi(p+5); if(v>=0&&v<MAX_BOOKS) app->book_idx=(uint8_t)v; }
    if((p = strstr(buf, "chap="))  != NULL) { int v=atoi(p+5); if(v>=0&&v<151) app->chapter_idx=(uint8_t)v; }
    if((p = strstr(buf, "verse=")) != NULL) { int v=atoi(p+6); if(v>=0&&v<=176) app->verse_idx=(uint8_t)v; }
    if((p = strstr(buf, "trans=")) != NULL) { int v=atoi(p+6); if(v>=0&&v<MAX_TRANSLATIONS) app->translation_idx=(uint8_t)v; }
}

// Bookmarks persistence

static void bookmarks_save(App* app) {
    File* f = storage_file_alloc(app->storage);
    if(!storage_file_open(f, BOOKMARKS_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_free(f);
        return;
    }
    for(uint8_t i = 0; i < app->bm_count; i++) {
        char line[40];
        int len = snprintf(line, sizeof(line), "%d %d %d %d %d\n",
            (int)app->bookmarks[i].sec,
            (int)app->bookmarks[i].canon_book,
            (int)app->bookmarks[i].chapter,
            (int)app->bookmarks[i].verse,
            (int)app->bookmarks[i].group);
        if(len > 0) storage_file_write(f, line, (uint16_t)len);
    }
    storage_file_close(f);
    storage_file_free(f);
}

static void bookmarks_load(App* app) {
    app->bm_count  = 0;
    app->bm_sel    = 0;
    app->bm_scroll = 0;
    File* f = storage_file_alloc(app->storage);
    if(!storage_file_open(f, BOOKMARKS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(f);
        return;
    }
    // 250 bookmarks * ~40 bytes each = ~10 KB; read in one shot
    char* buf = malloc(10240);
    if(!buf) { storage_file_free(f); return; }
    uint16_t rd = storage_file_read(f, buf, 10239);
    buf[rd] = '\0';
    storage_file_close(f);
    storage_file_free(f);

    char* line = buf;
    while(*line && app->bm_count < MAX_BOOKMARKS) {
        int sec, cb, ch, vv, grp;
        int fields = sscanf(line, "%d %d %d %d %d", &sec, &cb, &ch, &vv, &grp);
        if(fields >= 4) {
            if(sec >= 0 && sec < 4 && cb >= 0 && cb < CANON_BOOK_COUNT &&
               ch >= 0 && vv >= 1) {
                app->bookmarks[app->bm_count].sec        = (uint8_t)sec;
                app->bookmarks[app->bm_count].canon_book = (uint8_t)cb;
                app->bookmarks[app->bm_count].chapter    = (uint8_t)ch;
                app->bookmarks[app->bm_count].verse      = (uint8_t)vv;
                // group field: old files without it default to BM_GROUP_NONE
                if(fields >= 5 && grp >= 0 && grp < MAX_BM_GROUPS)
                    app->bookmarks[app->bm_count].group = (uint8_t)grp;
                else
                    app->bookmarks[app->bm_count].group = BM_GROUP_NONE;
                app->bm_count++;
            }
        }
        while(*line && *line != '\n') line++;
        if(*line == '\n') line++;
    }
    free(buf);
}

// Groups persistence

void bm_groups_save(App* app) {
    File* f = storage_file_alloc(app->storage);
    if(!storage_file_open(f, GROUPS_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_free(f);
        return;
    }
    for(uint8_t i = 0; i < app->bm_group_count; i++) {
        char line[BM_GROUP_NAME_LEN + 2];
        int len = snprintf(line, sizeof(line), "%s\n", app->bm_groups[i]);
        if(len > 0) storage_file_write(f, line, (uint16_t)len);
    }
    storage_file_close(f);
    storage_file_free(f);
}

static void bm_groups_load(App* app) {
    app->bm_group_count = 0;
    File* f = storage_file_alloc(app->storage);
    if(!storage_file_open(f, GROUPS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(f);
        return;
    }
    char buf[MAX_BM_GROUPS * (BM_GROUP_NAME_LEN + 2)];
    uint16_t rd = storage_file_read(f, buf, sizeof(buf) - 1);
    buf[rd] = '\0';
    storage_file_close(f);
    storage_file_free(f);

    char* line = buf;
    while(*line && app->bm_group_count < MAX_BM_GROUPS) {
        char* end = line;
        while(*end && *end != '\n') end++;
        size_t len = (size_t)(end - line);
        if(len > 0 && len < BM_GROUP_NAME_LEN) {
            memcpy(app->bm_groups[app->bm_group_count], line, len);
            app->bm_groups[app->bm_group_count][len] = '\0';
            app->bm_group_count++;
        }
        line = (*end == '\n') ? end + 1 : end;
    }
}

// Returns the index of the current single verse in bookmarks[], or -1 if absent
static int bookmark_find(App* app) {
    if(app->verse_idx == 0 || app->book_count == 0) return -1;
    uint8_t cb = app->book_list[app->book_idx];
    for(int i = 0; i < (int)app->bm_count; i++) {
        if(app->bookmarks[i].sec        == app->section_idx &&
           app->bookmarks[i].canon_book == cb &&
           app->bookmarks[i].chapter    == app->chapter_idx &&
           app->bookmarks[i].verse      == app->verse_idx)
            return i;
    }
    return -1;
}

static bool bookmark_is_set(App* app) {
    return bookmark_find(app) >= 0;
}

static void bookmark_remove(App* app) {
    int idx = bookmark_find(app);
    if(idx < 0) return;
    for(uint8_t i = (uint8_t)idx; i + 1 < app->bm_count; i++)
        app->bookmarks[i] = app->bookmarks[i + 1];
    app->bm_count--;
    bookmarks_save(app);
}

// Add bookmark with specified group (BM_GROUP_NONE = Default / no heading)
static void bookmark_add_with_group(App* app, uint8_t group) {
    if(app->verse_idx == 0 || app->book_count == 0) return;
    if(app->bm_count >= MAX_BOOKMARKS) return;
    uint8_t i = app->bm_count;
    app->bookmarks[i].sec        = app->section_idx;
    app->bookmarks[i].canon_book = app->book_list[app->book_idx];
    app->bookmarks[i].chapter    = app->chapter_idx;
    app->bookmarks[i].verse      = app->verse_idx;
    app->bookmarks[i].group      = group;
    app->bm_count++;
    bookmarks_save(app);
}

// Long-OK in reading view: if already bookmarked, remove it.
// If not bookmarked, open the group-picker overlay.
static void bookmark_toggle(App* app) {
    if(app->verse_idx == 0 || app->book_count == 0) return;
    if(bookmark_find(app) >= 0) {
        bookmark_remove(app);
    } else {
        // Open group picker
        app->bm_pick_sel    = 0;
        app->bm_pick_scroll = 0;
        app->view = ViewBmGroupPick;
    }
}

// ============================================================
// UI font helper
// ============================================================

// Use instead of canvas_set_font(canvas, FontSecondary) whenever the string
// being drawn might contain umlauts. If it does, falls back to the custom font
// defined by UMLAUT_FALLBACK_FONT (set in font.h — change it there to resize).
// If the string is clean ASCII, uses the standard built-in FontSecondary.
void set_ui_font(Canvas* canvas, const char* str) {
    if(str_has_umlaut(str)) {
        canvas_set_font_custom(canvas, UMLAUT_FALLBACK_FONT);
    } else {
        canvas_set_font(canvas, FontSecondary);
    }
}

// ============================================================
// Book / section display labels
// ============================================================

// Convert a canonical book index to a display label for the UI.
// Prefers display_label (with proper umlauts) when set; otherwise falls back
// to deriving the label from the filesystem folder name (underscores → spaces).
static void canon_book_label(App* app, uint8_t canon_idx, char* buf, size_t len) {
    // For English translations, derive the label from the English folder name
    // (underscores → spaces) so book names match the translation's language.
    if(app->section_lang_en && CANON_BOOKS[canon_idx].folder_en != NULL) {
        const char* src = CANON_BOOKS[canon_idx].folder_en;
        size_t i;
        for(i = 0; i < len - 1 && src[i]; i++)
            buf[i] = (src[i] == '_') ? ' ' : src[i];
        buf[i] = '\0';
        return;
    }
    if(CANON_BOOKS[canon_idx].display_label != NULL) {
        // Use the pre-composed display name (may contain umlauts)
        strncpy(buf, CANON_BOOKS[canon_idx].display_label, len - 1);
        buf[len - 1] = '\0';
    } else {
        // Fall back: derive from the appropriate folder name
        const char* src = book_folder_name(app, canon_idx);
        size_t i;
        for(i = 0; i < len - 1 && src[i]; i++)
            buf[i] = (src[i] == '_') ? ' ' : src[i];
        buf[i] = '\0';
    }
}



// ============================================================
// Keywords  (suggestions in search input)
// ============================================================

// Built-in fallback list – covers common Luther Bible vocabulary.
// User can override by placing keywords_de.txt (German) or keywords_en.txt
// (English) in DATA_DIR on the SD card. The correct file is chosen based on
// the active translation's folder language (section_lang_en).
static const char* const BUILTIN_KEYWORDS[] = {
    // People & patriarchs
    "Abraham","Adam","Apostel","Daniel","David","Elia","Elija","Esau",
    "Eva","Hagar","Hiob","Isaak","Jakob","Jesus","Johannes","Josef",
    "Josua","Judas","Kain","Lukas","Maria","Markus",
    "Matth\xc3\xa4" "us","Mose",
    "Nimrod","Noah","Paulus","Petrus","Salomo","Samuel","Sara","Simeon",
    // Titles & roles
    "Christus","Herr","Heiland","K\xc3\xb6" "nig","Priester","Prophet","Richter",
    "J\xc3\xbc" "nger","Hohepriester",
    "Schriftgelehrter","Pharis\xc3\xa4" "er",
    "Knecht","Hirte","Levit","Nazarener",
    // God & faith
    "Glaube","Gnade","Gott","Gottesdienst","Heilig","Hoffnung","Liebe",
    "Barmherzigkeit","Erbarmen","Erl\xc3\xb6" "sung","Ewigkeit","Freiheit",
    "Gerechtigkeit","Herrlichkeit","Heiligkeit","Lobpreis","Treue",
    "Vergebung","Vorsehung","Wahrheit","Weisheit","Wunder","Zeichen",
    // Worship & practice
    "Altar","Amen","Gebet","Gebot","Gesetz","Gottesdienst","Lobpreis",
    "Opfer","Sabbat","Segen","Taufe","Tempel","Zehnten","Beschneidung",
    "Beichte","Bu\xc3\x9f" "e","Dankopfer","Fastentag",
    "Gel\xc3\xbc" "bde","Opferlamm",
    // Salvation & sin
    "Errettung","S\xc3\xbc" "nde","Schuld","Reue","Umkehr","Bu\xc3\x9f" "e",
    "Gericht","Heil","Verdammnis",
    "Vers\xc3\xb6" "hnung","Strafe",
    // Creation & nature
    "Erde","Feuer","Himmel","Licht","Meer","Nacht","Tag","Wasser",
    "Wind","Berg","Fluss","Garten","Land","Stern","Sonne","Mond",
    "Wolke","Regen","W\xc3\xbc" "ste","Tal","Welle","Feld","Baum",
    // Objects & places
    "Arche","Brot","Grab","Kreuz","Lamm","Schwert","Stein","Stab",
    "Bundeslade","Jerusalem","Israel","Zion","Bethlehem",
    "Galil\xc3\xa4" "a","Jordan","Kapernaum","Nazareth",
    "\xc3\x84" "gypten","Babylon","Sinai","Golgatha",
    // Body & life
    "Auge","Blut","Bruder","Hand","Herz","Leben","Leib","Mutter",
    "Name","Seele","Sohn","Tod","Vater","Tochter","Schwester","Geist",
    "Atem","Fleisch","Kraft","Stimme",
    // Key concepts
    "Buch","Engel","Evangelium","Friede","Gemeinde","Gleichnis","Lob",
    "Psalm","Reich","Trost","Volk","Wort","Zeugnis","Auferstehung",
    "Bescheidenheit","Demut","Einheit","Gehorsam","Geduld",
    "G\xc3\xbc" "te","Harfe","Hosanna","Hunger","Kraft","Langmut",
    "N\xc3\xa4" "chstenliebe","Retter","Teufel","Traum","Vision",
    // Common German search words
    "nicht","und","aber","auch","denn","wie","wenn","dass","alle",
    "alles","viele","gro\xc3\x9f","gut","neu","alt","heilig","ewig","wahr",
    "klein","stark","schwach","arm","reich","gerecht",
    "b\xc3\xb6" "se","fromm","weise","geduldig","treu","bereit","lebendig",
    "Anfang","Ende","Zeit","Jahr","Morgen","Abend",
    "Weg","Welt","Feind","Kampf","Ruf","Ehe","Frieden","Leid","Freude",
};

#define BUILTIN_KW_COUNT ((uint16_t)(sizeof(BUILTIN_KEYWORDS)/sizeof(BUILTIN_KEYWORDS[0])))

// Built-in English keyword list – used automatically when the active translation
// uses English folder names (section_lang_en == true) and no keywords_en.txt is
// present on the SD card.
static const char* const BUILTIN_KEYWORDS_EN[] = {
    // People & patriarchs
    "Abraham","Adam","Apostle","Daniel","David","Elijah","Elisha","Esau",
    "Eve","Hagar","Job","Isaac","Jacob","Jesus","John","Joseph",
    "Joshua","Judas","Cain","Luke","Mary","Mark","Matthew","Moses",
    "Nimrod","Noah","Paul","Peter","Solomon","Samuel","Sarah","Simeon",
    // Titles & roles
    "Christ","Lord","Savior","King","Priest","Prophet","Judge",
    "Disciple","HighPriest","Scribe","Pharisee","Servant","Shepherd",
    "Levite","Nazarene","Apostle","Elder","Deacon",
    // God & faith
    "Faith","Grace","God","Worship","Holy","Hope","Love",
    "Mercy","Compassion","Redemption","Eternity","Freedom",
    "Righteousness","Glory","Holiness","Praise","Faithfulness",
    "Forgiveness","Providence","Truth","Wisdom","Miracle","Sign",
    // Worship & practice
    "Altar","Amen","Prayer","Commandment","Law","Worship","Praise",
    "Offering","Sabbath","Blessing","Baptism","Temple","Tithe",
    "Circumcision","Confession","Repentance","Thanksgiving","Fasting",
    "Vow","Sacrifice","Covenant",
    // Salvation & sin
    "Salvation","Sin","Guilt","Remorse","Repentance","Judgment",
    "Deliverance","Condemnation","Atonement","Punishment","Wrath",
    "Reconciliation","Justification","Sanctification",
    // Creation & nature
    "Earth","Fire","Heaven","Light","Sea","Night","Day","Water",
    "Wind","Mountain","River","Garden","Land","Star","Sun","Moon",
    "Cloud","Rain","Desert","Valley","Wave","Field","Tree","Wilderness",
    // Objects & places
    "Ark","Bread","Tomb","Cross","Lamb","Sword","Stone","Staff",
    "ArkOfCovenant","Jerusalem","Israel","Zion","Bethlehem",
    "Galilee","Jordan","Capernaum","Nazareth",
    "Egypt","Babylon","Sinai","Golgotha","Canaan","Judea","Samaria",
    // Body & life
    "Eye","Blood","Brother","Hand","Heart","Life","Body","Mother",
    "Name","Soul","Son","Death","Father","Daughter","Sister","Spirit",
    "Breath","Flesh","Strength","Voice","Bone","Face",
    // Key concepts
    "Book","Angel","Gospel","Peace","Church","Parable","Praise",
    "Psalm","Kingdom","Comfort","People","Word","Testimony","Resurrection",
    "Humility","Unity","Obedience","Patience","Goodness","Harp",
    "Hosanna","Hunger","Power","Longsuffering","Love","Savior",
    "Devil","Dream","Vision","Prophecy","Covenant","Promise",
    // Common English search words
    "not","and","but","also","for","how","when","that","all",
    "everything","many","great","good","new","old","holy","eternal","true",
    "small","strong","weak","poor","rich","righteous",
    "evil","devout","wise","patient","faithful","ready","living",
    "beginning","end","time","year","morning","evening",
    "way","world","enemy","battle","call","marriage","peace","sorrow","joy",
};

#define BUILTIN_KW_EN_COUNT ((uint16_t)(sizeof(BUILTIN_KEYWORDS_EN)/sizeof(BUILTIN_KEYWORDS_EN[0])))

// Static storage for keyword table -- kept out of App to avoid bloating the
// heap allocation and to ensure it lives in BSS (zero-initialised).
static char s_kw_words[MAX_KEYWORDS][KEYWORD_WORD_LEN];
static uint16_t s_kw_count = 0;

void keywords_load(App* app) {
    s_kw_count         = 0;
    app->kw_count      = 0;
    app->suggest_count = 0;
    app->suggest_sel   = 0;

    const char* kw_path = app->section_lang_en ? KEYWORDS_PATH_EN : KEYWORDS_PATH_DE;
    File* f = storage_file_alloc(app->storage);
    bool from_file = storage_file_open(f, kw_path, FSAM_READ, FSOM_OPEN_EXISTING);
    if(from_file) {
        // Read line-by-line with a small stack buffer to avoid stack overflow.
        char line[KEYWORD_WORD_LEN + 2];
        uint8_t lpos = 0;
        char ch = 0;
        while(s_kw_count < MAX_KEYWORDS) {
            uint16_t rd = storage_file_read(f, &ch, 1);
            if(rd == 0) {
                // EOF: flush any pending word
                if(lpos > 0) {
                    while(lpos > 0 && line[lpos-1] == ' ') lpos--;
                    if(lpos > 0) {
                        line[lpos] = '\0';
                        memcpy(s_kw_words[s_kw_count], line, lpos + 1);
                        s_kw_count++;
                    }
                }
                break;
            }
            if(ch == '\n' || ch == '\r') {
                // End of line: trim and store
                while(lpos > 0 && line[lpos-1] == ' ') lpos--;
                if(lpos > 0) {
                    line[lpos] = '\0';
                    memcpy(s_kw_words[s_kw_count], line, lpos + 1);
                    s_kw_count++;
                }
                lpos = 0;
            } else {
                if(lpos < KEYWORD_WORD_LEN - 1)
                    line[lpos++] = ch;
                // else: word too long, keep reading until newline discards it
            }
        }
        storage_file_close(f);
        storage_file_free(f);
    } else {
        storage_file_free(f);
        // No language-specific keywords file on SD card (keywords_de.txt /
        // keywords_en.txt): pick the built-in list that matches the active
        // translation's language (English folder layout → English words,
        // German folder layout → German words).
        const char* const* src;
        uint16_t           src_count;
        if(app->section_lang_en) {
            src       = BUILTIN_KEYWORDS_EN;
            src_count = BUILTIN_KW_EN_COUNT;
        } else {
            src       = BUILTIN_KEYWORDS;
            src_count = BUILTIN_KW_COUNT;
        }
        for(uint16_t i = 0; i < src_count && i < MAX_KEYWORDS; i++) {
            strncpy(s_kw_words[i], src[i], KEYWORD_WORD_LEN - 1);
            s_kw_words[i][KEYWORD_WORD_LEN - 1] = '\0';
        }
        s_kw_count = (src_count < MAX_KEYWORDS) ? src_count : MAX_KEYWORDS;
    }
    app->kw_count = s_kw_count;
}

// Case-insensitive ASCII prefix match
static bool kw_prefix_match(const char* word, const char* prefix, uint8_t plen) {
    for(uint8_t i = 0; i < plen; i++) {
        char a = word[i], b = prefix[i];
        if(!a) return false;
        if(a >= 'A' && a <= 'Z') a = (char)(a + 32);
        if(b >= 'A' && b <= 'Z') b = (char)(b + 32);
        if(a != b) return false;
    }
    return true;
}

// Rebuild suggestion list from current buffer (prefix = last word after space)
void suggestions_update(App* app) {
    app->suggest_count = 0;
    if(app->search_len == 0) return;

    const char* prefix = app->search_buf;
    for(int i = (int)app->search_len - 1; i >= 0; i--) {
        if(app->search_buf[i] == ' ') { prefix = app->search_buf + i + 1; break; }
    }
    uint8_t plen = (uint8_t)strlen(prefix);
    if(plen == 0) return;

    for(uint16_t i = 0; i < s_kw_count && app->suggest_count < SUGGEST_MAX; i++) {
        if(kw_prefix_match(s_kw_words[i], prefix, plen)) {
            strncpy(app->suggest[app->suggest_count],
                    s_kw_words[i], KEYWORD_WORD_LEN - 1);
            app->suggest[app->suggest_count][KEYWORD_WORD_LEN - 1] = '\0';
            app->suggest_count++;
        }
    }
    if(app->suggest_sel >= app->suggest_count)
        app->suggest_sel = (app->suggest_count > 0) ? app->suggest_count - 1 : 0;
}

// Replace last word in buffer with the selected suggestion
void suggestion_fill(App* app) {
    if(app->suggest_count == 0) return;
    const char* word = app->suggest[app->suggest_sel];

    int last_space = -1;
    for(int i = (int)app->search_len - 1; i >= 0; i--) {
        if(app->search_buf[i] == ' ') { last_space = i; break; }
    }
    uint8_t prefix_end = (uint8_t)(last_space + 1);

    size_t wlen = strlen(word);
    if(prefix_end + wlen >= MAX_SEARCH_LEN) return;

    memcpy(app->search_buf + prefix_end, word, wlen);
    app->search_len = (uint8_t)(prefix_end + wlen);
    app->search_buf[app->search_len] = '\0';

    app->suggest_count = 0;
    app->suggest_sel   = 0;
    app->cursor_pos    = app->search_len;  // cursor moves to end of filled word
}

// Returns the root path for the currently selected translation's content.
static void translation_data_dir(App* app, char* buf, size_t len) {
    if(app->translation_count == 0) {
        snprintf(buf, len, "%s", DATA_DIR);
    } else {
        snprintf(buf, len, "%s/%s",
                 DATA_DIR,
                 app->translations[app->translation_idx]);
    }
}

// Scan DATA_DIR for subdirectories that contain at least one recognised
// section folder. Each such subdir is treated as a translation.
// If none are found the app falls back to the old flat layout.
void translations_scan(App* app) {
    app->translation_count = 0;
    app->translation_idx   = 0;

    File* dir = storage_file_alloc(app->storage);
    if(!storage_dir_open(dir, DATA_DIR)) {
        storage_file_free(dir);
        return;
    }

    FileInfo fi;
    char     fname[NAME_LEN];
    while(storage_dir_read(dir, &fi, fname, sizeof(fname))) {
        if(!file_info_is_dir(&fi)) continue;
        if(fname[0] == '.') continue;

        // Check whether this sub-directory contains at least one section folder
        // (German layout OR English layout from the Python script)
        bool is_translation = false;
        for(uint8_t s = 0; s < 4 && !is_translation; s++) {
            char probe[200];
            // Try German name
            snprintf(probe, sizeof(probe), "%s/%s/%s",
                     DATA_DIR, fname, SECTION_ORDER[s]);
            File* pf = storage_file_alloc(app->storage);
            if(storage_dir_open(pf, probe)) {
                is_translation = true;
                storage_dir_close(pf);
            }
            storage_file_free(pf);
            if(is_translation) break;
            // Try English name
            snprintf(probe, sizeof(probe), "%s/%s/%s",
                     DATA_DIR, fname, SECTION_ORDER_EN[s]);
            pf = storage_file_alloc(app->storage);
            if(storage_dir_open(pf, probe)) {
                is_translation = true;
                storage_dir_close(pf);
            }
            storage_file_free(pf);
        }

        if(is_translation && app->translation_count < MAX_TRANSLATIONS) {
            strncpy(app->translations[app->translation_count],
                    fname, TRANSLATION_NAME_LEN - 1);
            app->translations[app->translation_count][TRANSLATION_NAME_LEN - 1] = '\0';
            app->translation_count++;
        }
    }
    storage_dir_close(dir);
    storage_file_free(dir);
}

// Scan which of the 4 canonical sections actually exist on SD for the current
// translation. Populates avail_sections[] and avail_section_count.
// Also clamps section_idx to a valid available section.
void rebuild_section_list(App* app) {
    app->avail_section_count = 0;
    app->section_lang_en = false;
    char tdir[120];
    translation_data_dir(app, tdir, sizeof(tdir));

    // Detect language: probe for a German section first, then English.
    // Whichever is found first wins for the whole translation.
    bool lang_detected = false;
    for(uint8_t s = 0; s < 4 && !lang_detected; s++) {
        char probe[200];
        // Try German
        snprintf(probe, sizeof(probe), "%s/%s", tdir, SECTION_ORDER[s]);
        File* f = storage_file_alloc(app->storage);
        if(storage_dir_open(f, probe)) {
            storage_dir_close(f);
            app->section_lang_en = false;
            lang_detected = true;
        }
        storage_file_free(f);
        if(lang_detected) break;
        // Try English
        snprintf(probe, sizeof(probe), "%s/%s", tdir, SECTION_ORDER_EN[s]);
        f = storage_file_alloc(app->storage);
        if(storage_dir_open(f, probe)) {
            storage_dir_close(f);
            app->section_lang_en = true;
            lang_detected = true;
        }
        storage_file_free(f);
    }

    // Now scan all four canonical sections using the detected language
    for(uint8_t s = 0; s < 4; s++) {
        char probe[200];
        snprintf(probe, sizeof(probe), "%s/%s", tdir, section_folder(app, s));
        File* f = storage_file_alloc(app->storage);
        bool exists = storage_dir_open(f, probe);
        if(exists) {
            storage_dir_close(f);
            app->avail_sections[app->avail_section_count++] = s;
        }
        storage_file_free(f);
    }

    // If nothing found fall back to showing all sections (handles flat layout)
    if(app->avail_section_count == 0) {
        for(uint8_t s = 0; s < 4; s++) app->avail_sections[s] = s;
        app->avail_section_count = 4;
    }

    // Clamp section_idx to a valid available entry
    bool found = false;
    for(uint8_t i = 0; i < app->avail_section_count; i++) {
        if(app->avail_sections[i] == app->section_idx) { found = true; break; }
    }
    if(!found) app->section_idx = app->avail_sections[0];
}

void rebuild_book_list(App* app) {
    uint8_t sec = app->section_idx;
    app->book_count = 0;
    for(uint8_t i = 0; i < CANON_BOOK_COUNT; i++) {
        if(CANON_BOOKS[i].section == sec && app->book_count < MAX_BOOKS) {
            app->book_list[app->book_count++] = i;
        }
    }
    if(app->book_idx >= app->book_count && app->book_count > 0)
        app->book_idx = 0;
}

static const char* current_book_folder(App* app) {
    if(app->book_count == 0) return "";
    return book_folder_name(app, app->book_list[app->book_idx]);
}

static void current_book_label(App* app, char* buf, size_t len) {
    if(app->book_count == 0) { buf[0] = '\0'; return; }
    uint8_t canon_idx = app->book_list[app->book_idx];
    canon_book_label(app, canon_idx, buf, len);
}

// Directory helpers

static uint8_t count_chapter_dirs(Storage* storage, const char* path) {
    File* dir = storage_file_alloc(storage);
    if(!storage_dir_open(dir, path)) { storage_file_free(dir); return 0; }
    FileInfo fi; char fname[NAME_LEN]; uint8_t count = 0;
    while(storage_dir_read(dir, &fi, fname, sizeof(fname)))
        if(file_info_is_dir(&fi) && fname[0] >= '1' && fname[0] <= '9') count++;
    storage_dir_close(dir); storage_file_free(dir);
    return count;
}

static uint8_t count_verse_files(Storage* storage, const char* path) {
    File* dir = storage_file_alloc(storage);
    if(!storage_dir_open(dir, path)) { storage_file_free(dir); return 0; }
    FileInfo fi; char fname[NAME_LEN]; uint8_t count = 0;
    while(storage_dir_read(dir, &fi, fname, sizeof(fname))) {
        if(!file_info_is_dir(&fi)) {
            size_t l = strlen(fname);
            if(l > 4 && strcmp(fname + l - 4, ".txt") == 0) count++;
        }
    }
    storage_dir_close(dir); storage_file_free(dir);
    return count;
}

static void build_chapter_path(App* app, char* buf, size_t buf_len) {
    char tdir[120];
    translation_data_dir(app, tdir, sizeof(tdir));
    snprintf(buf, buf_len, "%s/%s/%s/%d",
             tdir,
             section_folder(app, app->section_idx),
             current_book_folder(app),
             app->chapter_idx + 1);
}

// Data loading

void refresh_chapter_count(App* app) {
    char tdir[120];
    translation_data_dir(app, tdir, sizeof(tdir));
    char path[200];
    snprintf(path, sizeof(path), "%s/%s/%s",
             tdir,
             section_folder(app, app->section_idx),
             current_book_folder(app));
    app->chapter_count = count_chapter_dirs(app->storage, path);
    if(app->chapter_idx >= app->chapter_count && app->chapter_count > 0)
        app->chapter_idx = 0;
}

void refresh_verse_count(App* app) {
    char path[220];
    build_chapter_path(app, path, sizeof(path));
    app->verse_count = count_verse_files(app->storage, path);
    if(app->verse_idx > app->verse_count) app->verse_idx = 0;
}

static bool read_verse_file(App* app, uint8_t vnum, char* dst, size_t dst_len) {
    char path[240]; char cpath[220];
    build_chapter_path(app, cpath, sizeof(cpath));
    snprintf(path, sizeof(path), "%s/verse%d.txt", cpath, vnum);
    File* f = storage_file_alloc(app->storage);
    bool ok = false;
    if(storage_file_open(f, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint16_t rd = storage_file_read(f, dst, (uint16_t)(dst_len - 1));
        dst[rd] = '\0';
        while(rd > 0 && (dst[rd-1] == '\n' || dst[rd-1] == '\r')) dst[--rd] = '\0';
        ok = true;
        storage_file_close(f);
    }
    storage_file_free(f);
    return ok;
}

// Case-insensitive substring match (ASCII only, matches Luther German)
static bool icontains_ascii(const char* hay, const char* needle) {
    if(!hay || !needle || !needle[0]) return false;
    size_t nlen = strlen(needle);
    size_t hlen = strlen(hay);
    if(nlen > hlen) return false;
    for(size_t i = 0; i <= hlen - nlen; i++) {
        bool ok = true;
        for(size_t j = 0; j < nlen; j++) {
            char a = hay[i+j], b = needle[j];
            if(a >= 'A' && a <= 'Z') a = (char)(a + 32);
            if(b >= 'A' && b <= 'Z') b = (char)(b + 32);
            if(a != b) { ok = false; break; }
        }
        if(ok) return true;
    }
    return false;
}

// Search -- iterates every verse file in the current book,
// across all chapters, one file at a time. No index file needed.
// Results stored in app->hits[].
void do_search(App* app) {
    app->hit_count = 0;
    app->hit_sel   = 0;
    app->hit_scroll = 0;

    if(!app->search_len || app->book_count == 0) return;

    // Get book path: tdir/section/book/
    char tdir[120];
    translation_data_dir(app, tdir, sizeof(tdir));
    char book_path[200];
    snprintf(book_path, sizeof(book_path), "%s/%s/%s",
             tdir,
             section_folder(app, app->section_idx),
             current_book_folder(app));

    uint8_t chap_count = count_chapter_dirs(app->storage, book_path);
    if(chap_count == 0) return;

    char verse_buf[VERSE_BUF_LEN];

    for(uint8_t chap = 1; chap <= chap_count && app->hit_count < MAX_SEARCH_HITS; chap++) {
        char chap_path[220];
        snprintf(chap_path, sizeof(chap_path), "%s/%d", book_path, (int)chap);

        uint8_t vcount = count_verse_files(app->storage, chap_path);

        for(uint8_t v = 1; v <= vcount && app->hit_count < MAX_SEARCH_HITS; v++) {
            char vpath[240];
            snprintf(vpath, sizeof(vpath), "%s/verse%d.txt", chap_path, (int)v);

            File* f = storage_file_alloc(app->storage);
            bool opened = storage_file_open(f, vpath, FSAM_READ, FSOM_OPEN_EXISTING);
            if(opened) {
                uint16_t rd = storage_file_read(f, verse_buf, sizeof(verse_buf) - 1);
                verse_buf[rd] = '\0';
                storage_file_close(f);
            }
            storage_file_free(f);

            if(!opened) continue;
            if(!icontains_ascii(verse_buf, app->search_buf)) continue;

            // Record hit
            uint8_t hi = app->hit_count;
            app->hits[hi].sec     = app->section_idx;
            app->hits[hi].book    = app->book_idx;
            app->hits[hi].chapter = (uint8_t)(chap - 1); // 0-based
            app->hits[hi].verse   = v;

            // Build display reference label
            char blabel[21];
            current_book_label(app, blabel, sizeof(blabel));
            // Truncate book label to fit on screen (max 12 chars)
            blabel[12] = '\0';
            snprintf(app->hits[hi].ref, sizeof(app->hits[hi].ref),
                     "%s %d:%d", blabel, (int)chap, (int)v);

            app->hit_count++;
        }
    }
}

// Load a window of up to VERSE_PAGE_SIZE verses starting at Bible verse `start`
// (1-based). Fills chapter.text[] and sets verse_page_start.
static void load_verse_page(App* app, uint8_t start) {
    memset(&app->chapter, 0, sizeof(app->chapter));
    if(start < 1) start = 1;
    if(start > app->verse_count) start = app->verse_count;
    app->verse_page_start = start;

    uint8_t end = start + VERSE_PAGE_SIZE - 1;
    if(end > app->verse_count) end = app->verse_count;

    for(uint8_t v = start; v <= end; v++) {
        read_verse_file(app, v,
                        app->chapter.text[app->chapter.count],
                        VERSE_BUF_LEN);
        app->chapter.count++;
    }
}

// Wrapper: initial load (All mode starts at verse 1, single mode ignores paging)
static void load_chapter(App* app) {
    if(app->verse_idx == 0) {
        load_verse_page(app, 1);
    } else {
        memset(&app->chapter, 0, sizeof(app->chapter));
        app->verse_page_start = app->verse_idx;
        read_verse_file(app, app->verse_idx, app->chapter.text[0], VERSE_BUF_LEN);
        app->chapter.count = 1;
    }
}

// Word wrap

static void wrap_chapter(App* app) {
    memset(&app->wrap, 0, sizeof(app->wrap));
    uint8_t cols = FONT_CHARS[app->font_choice];
    if(cols > 2) cols = (uint8_t)(cols - 1);
    uint16_t line = 0;

    for(uint16_t v = 0; v < app->chapter.count && line < WRAP_MAX_LINES; v++) {
        char full[VERSE_BUF_LEN + 10];
        if(app->verse_idx == 0) {
            snprintf(full, sizeof(full), "%d: %s",
                     (int)(app->verse_page_start + v),
                     app->chapter.text[v]);
        } else {
            snprintf(full, sizeof(full), "%s", app->chapter.text[v]);
        }

        size_t src_len = strlen(full), pos = 0;
        bool first_line_of_verse = true;

        while(pos < src_len && line < WRAP_MAX_LINES) {
            size_t rem = src_len - pos;
            uint8_t effective_cols = first_line_of_verse ? cols :
                                     (cols > 1 ? (uint8_t)(cols - 1) : cols);
            size_t take = (rem < effective_cols) ? rem : effective_cols;
            if(pos + take < src_len && full[pos + take] != ' ') {
                size_t t2 = take;
                while(t2 > 0 && full[pos + t2] != ' ') t2--;
                if(t2 > 0) take = t2;
            }

            // Clamp copy to fit in the line buffer, but advance pos by
            // exactly `take` so no characters are silently dropped.
            if(first_line_of_verse) {
                if(app->verse_idx != 0) {
                    // Single-verse view: indent first line with one space
                    app->wrap.lines[line][0] = ' ';
                    size_t copy = (take < 37u) ? take : 37u;
                    memcpy(app->wrap.lines[line] + 1, full + pos, copy);
                    app->wrap.lines[line][1 + copy] = '\0';
                } else {
                    size_t copy = (take < 38u) ? take : 38u;
                    memcpy(app->wrap.lines[line], full + pos, copy);
                    app->wrap.lines[line][copy] = '\0';
                }
            } else {
                if (app->verse_idx != 0) {
                    // Single-verse view: no indent on continuation lines
                    size_t copy = (take < 38u) ? take : 38u;
                    memcpy(app->wrap.lines[line], full + pos, copy);
                    app->wrap.lines[line][copy] = '\0';
                }
                else {
                    // All mode: indent continuation lines under verse number
                    app->wrap.lines[line][0] = ' ';
                    size_t copy = (take < 37u) ? take : 37u;
                    memcpy(app->wrap.lines[line] + 1, full + pos, copy);
                    app->wrap.lines[line][1 + copy] = '\0';
                }
            }

            pos += take;
            if(pos < src_len && full[pos] == ' ') pos++;
            line++;
            first_line_of_verse = false;
        }
    }

    app->wrap.count  = line;
    app->wrap.scroll = 0;
}

// Shared draw primitives

// Full-width dark header bar with bold white title (always dark
// for visibility regardless of dark_mode body setting)
void draw_hdr(Canvas* canvas, const char* title) {
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, SCREEN_W, HDR_H);
    canvas_set_color(canvas, ColorWhite);
    if(str_has_umlaut(title)) {
        canvas_set_font_custom(canvas, UMLAUT_FALLBACK_FONT_HDR);
    } else {
        canvas_set_font(canvas, FontPrimary);
    }
    canvas_draw_str_aligned(canvas, SCREEN_W / 2, 1,
                            AlignCenter, AlignTop, title);
    canvas_set_color(canvas, ColorBlack);
}

// Proportional scrollbar
void draw_scrollbar(Canvas* canvas, App* app,
                            uint16_t pos, uint16_t total, uint8_t vis) {
    if(total <= vis) return;
    uint8_t track_h = (uint8_t)(SCREEN_H - HDR_H - 2);
    uint8_t bar_h   = (uint8_t)((uint32_t)track_h * vis / total);
    if(bar_h < 3) bar_h = 3;
    uint8_t bar_y = (uint8_t)(HDR_H + 1 +
        (uint32_t)(track_h - bar_h) * pos / (total - vis));
    set_fg(canvas, app);
    canvas_draw_line(canvas, SB_X + 1, HDR_H + 1, SB_X + 1, SCREEN_H - 1);
    canvas_draw_box(canvas, SB_X, bar_y, SB_W, bar_h);
}

// Scene: Menu

static void draw_menu(Canvas* canvas, App* app) {
    // Background
    if(app->dark_mode) {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, 0, 0, SCREEN_W, SCREEN_H);
    }
    draw_hdr(canvas, APP_NAME " v" APP_VERSION);

    set_fg(canvas, app);
    canvas_set_font(canvas, FontSecondary);

    char book_label[21];
    current_book_label(app, book_label, sizeof(book_label));

    // Keep scroll in range so cursor is always visible
    if((uint8_t)app->sel_row < app->menu_scroll)
        app->menu_scroll = (uint8_t)app->sel_row;
    if((uint8_t)app->sel_row >= app->menu_scroll + MENU_VIS)
        app->menu_scroll = (uint8_t)app->sel_row - MENU_VIS + 1;

    for(uint8_t vi = 0; vi < MENU_VIS; vi++) {
        uint8_t r = app->menu_scroll + vi;
        if(r >= MENU_ROWS) break;

        int y = MENU_BODY_Y + vi * MENU_ROW_H;
        bool sel = ((uint8_t)app->sel_row == r);

        if(sel) {
            set_fg(canvas, app);
            canvas_draw_box(canvas, 0, y, SCREEN_W - SB_W - 2, MENU_ROW_H);
            set_bg(canvas, app);
        } else {
            set_fg(canvas, app);
        }

        // Divider above Search row when it's visible and not selected
        if(r == RowSearch && !sel) {
            set_fg(canvas, app);
            canvas_draw_line(canvas, 0, y - 1, SCREEN_W - SB_W - 3, y - 1);
        }

        // Divider above Settings row when it's visible and not selected
        if(r == RowSettings && !sel) {
            set_fg(canvas, app);
            canvas_draw_line(canvas, 0, y - 1, SCREEN_W - SB_W - 3, y - 1);
        }

        switch(r) {
        case RowSection:
        case RowBook:
        case RowChapter:
        case RowVerse: {
            static const char* const picker_labels[4] = {
                "Section", "Book", "Chapter", "Verse"
            };
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str(canvas, 2, y + 8, picker_labels[r]);

            char val[64];
            if(r == RowSection) {
                const char* const* sec_labels =
                    app->section_lang_en ? SECTION_LABELS_EN : SECTION_LABELS;
                snprintf(val, sizeof(val), "<%s>",
                         sec_labels[app->section_idx]);
            } else if(r == RowBook) {
                if(app->book_count == 0)
                    snprintf(val, sizeof(val), "<?>");
                else
                    snprintf(val, sizeof(val), "<%s>", book_label);
            } else if(r == RowChapter) {
                if(app->chapter_count == 0)
                    snprintf(val, sizeof(val), "<?>");
                else
                    snprintf(val, sizeof(val), "< %d >",
                             (int)(app->chapter_idx + 1));
            } else {
                if(app->verse_idx == 0)
                    snprintf(val, sizeof(val), "<All>");
                else
                    snprintf(val, sizeof(val), "< %d >",
                             (int)app->verse_idx);
            }
            set_ui_font(canvas, val);
            canvas_draw_str_aligned(canvas, SCREEN_W - SB_W - 3, y + 8,
                                    AlignRight, AlignBottom, val);
            break;
        }
        case RowSearch:
            canvas_draw_str(canvas, 2, y + 8, "Search");
            canvas_draw_str_aligned(canvas, SCREEN_W - SB_W - 3, y + 8,
                                    AlignRight, AlignBottom, ">");
            break;
        case RowBookmarks: {
            canvas_draw_str(canvas, 2, y + 8, "Bookmarks");
            char bval[8];
            if(app->bm_count == 0)
                snprintf(bval, sizeof(bval), ">");
            else
                snprintf(bval, sizeof(bval), "%d >", (int)app->bm_count);
            canvas_draw_str_aligned(canvas, SCREEN_W - SB_W - 3, y + 8,
                                    AlignRight, AlignBottom, bval);
            break;
        }
        case RowSettings:
            canvas_draw_str(canvas, 2, y + 8, "Settings");
            canvas_draw_str_aligned(canvas, SCREEN_W - SB_W - 3, y + 8,
                                    AlignRight, AlignBottom, ">");
            break;
        case RowAbout:
            canvas_draw_str(canvas, 2, y + 8, "About");
            canvas_draw_str_aligned(canvas, SCREEN_W - SB_W - 3, y + 8,
                                    AlignRight, AlignBottom, ">");
            break;
        default: break;
        }

        set_fg(canvas, app);
    }

    // Scrollbar for menu
    draw_scrollbar(canvas, app, app->menu_scroll, MENU_ROWS, MENU_VIS);
}

// Scene: Reading view

static void draw_reading(Canvas* canvas, App* app) {
    if(app->dark_mode) {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, 0, 0, SCREEN_W, SCREEN_H);
    }

    // Clamp book label to 20 chars so hdr[64] never truncates.
    // Worst case: "12345678901234567890 150:80" = 27 chars.
    char book_label[21];
    current_book_label(app, book_label, sizeof(book_label));

    char hdr[64];
    bool bm_star = false;
    if(app->verse_idx == 0)
        snprintf(hdr, sizeof(hdr), "%s %d",
                 book_label, (int)(app->chapter_idx + 1));
    else {
        bm_star = bookmark_is_set(app);
        snprintf(hdr, sizeof(hdr), "%s %d:%d",
                 book_label, (int)(app->chapter_idx + 1), (int)app->verse_idx);
    }

    draw_hdr(canvas, hdr);

    // Draw bold bookmark star in the header using FontPrimary (same as header font)
    if(bm_star) {
        canvas_set_color(canvas, ColorWhite);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, SCREEN_W - 2, 1,
                                AlignRight, AlignTop, "*");
        canvas_set_color(canvas, ColorBlack);
    }

    // Page turn hints: show "<" on the left and/or ">" on the right of the
    // header when in All mode and there are previous/next verse pages.
    if(app->verse_idx == 0) {
        bool has_prev = (app->verse_page_start > 1);
        bool has_next = (app->verse_page_start + app->chapter.count - 1) < app->verse_count;
        canvas_set_color(canvas, ColorWhite);
        canvas_set_font(canvas, FontSecondary);
        if(has_prev)
            canvas_draw_str(canvas, 1, HDR_H - 2, "<");
        if(has_next)
            canvas_draw_str_aligned(canvas, SCREEN_W - 2, HDR_H - 2,
                                    AlignRight, AlignBottom, ">");
        canvas_set_color(canvas, ColorBlack);
    }

    set_fg(canvas, app);
    apply_font(canvas, app->font_choice);
    uint8_t lh  = FONT_LINE_H[app->font_choice];
    uint8_t vis = font_visible_lines(app->font_choice);

    for(uint8_t i = 0; i < vis &&
            (app->wrap.scroll + i) < app->wrap.count; i++) {
        canvas_draw_str(canvas, 2,
                        READ_BODY_Y + i * lh + lh - 1,
                        app->wrap.lines[app->wrap.scroll + i]);
    }

    draw_scrollbar(canvas, app, app->wrap.scroll, app->wrap.count, vis);
}

// Scene: Settings

static void draw_settings(Canvas* canvas, App* app) {
    if(app->dark_mode) {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, 0, 0, SCREEN_W, SCREEN_H);
    }

    draw_hdr(canvas, "Settings");

    canvas_set_font(canvas, FontSecondary);

    const uint8_t ITEM_H = 11;
    const uint8_t BODY_Y = HDR_H + 2;
    const uint8_t VIS    = 4;

    if(app->settings_font_open) {
        // -- Expanded font picker --
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, 0, HDR_H, SCREEN_W, 9);
        canvas_set_color(canvas, ColorWhite);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, SCREEN_W / 2, HDR_H + 8,
                                AlignCenter, AlignBottom, "Select Font  OK=Confirm");
        set_fg(canvas, app);

        uint8_t scroll = (app->settings_font_sel >= VIS) ?
                         app->settings_font_sel - VIS + 1 : 0;

        const uint8_t FY0 = HDR_H + 11;
        for(uint8_t i = 0; i < VIS && (scroll + i) < FONT_COUNT; i++) {
            uint8_t si     = scroll + i;
            uint8_t y      = FY0 + i * ITEM_H;
            bool    cursor = (app->settings_font_sel == si);
            bool    active = ((uint8_t)app->font_choice == si);

            if(cursor) {
                set_fg(canvas, app);
                canvas_draw_box(canvas, 2, y - 1, SCREEN_W - 4, ITEM_H);
                set_bg(canvas, app);
            } else {
                set_fg(canvas, app);
            }
            canvas_draw_str(canvas, 5,  y + 8, active ? ">" : " ");
            canvas_draw_str(canvas, 13, y + 8, FONT_LABELS[si]);
            set_fg(canvas, app);
        }
        draw_scrollbar(canvas, app, scroll, FONT_COUNT, VIS);
        return;
    }

    if(app->settings_trans_open) {
        // -- Expanded translation picker --
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, 0, HDR_H, SCREEN_W, 9);
        canvas_set_color(canvas, ColorWhite);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, SCREEN_W / 2, HDR_H + 8,
                                AlignCenter, AlignBottom, "Select Trans  OK=Confirm");
        set_fg(canvas, app);

        uint8_t scroll = (app->settings_trans_sel >= VIS) ?
                         app->settings_trans_sel - VIS + 1 : 0;

        const uint8_t TY0 = HDR_H + 11;
        for(uint8_t i = 0; i < VIS && (scroll + i) < app->translation_count; i++) {
            uint8_t si     = scroll + i;
            uint8_t y      = TY0 + i * ITEM_H;
            bool    cursor = (app->settings_trans_sel == si);
            bool    active = (app->translation_idx == si);

            if(cursor) {
                set_fg(canvas, app);
                canvas_draw_box(canvas, 2, y - 1, SCREEN_W - 4, ITEM_H);
                set_bg(canvas, app);
            } else {
                set_fg(canvas, app);
            }
            canvas_draw_str(canvas, 5,  y + 8, active ? ">" : " ");
            canvas_draw_str(canvas, 13, y + 8, app->translations[si]);
            set_fg(canvas, app);
        }
        draw_scrollbar(canvas, app, scroll, app->translation_count, VIS);
        return;
    }

    // -- Collapsed view --

    // Row 0: Translation (only when multiple are present)
    if(app->translation_count > 1) {
        uint8_t y   = BODY_Y;
        bool    sel = (app->settings_sel == SettingsRowTranslation);
        if(sel) {
            set_fg(canvas, app);
            canvas_draw_box(canvas, 0, y, SCREEN_W, ITEM_H);
            set_bg(canvas, app);
        } else {
            set_fg(canvas, app);
        }
        canvas_draw_str(canvas, 3, y + 8, "Translation");
        char tval[24];
        snprintf(tval, sizeof(tval), "[%.16s]",
                 app->translations[app->translation_idx]);
        canvas_draw_str_aligned(canvas, SCREEN_W - 2, y + 8,
                                AlignRight, AlignBottom, tval);
        set_fg(canvas, app);
    }

    // Row 1 (or 0 if no translation row): Font
    {
        uint8_t row_offset = (app->translation_count > 1) ? 1 : 0;
        uint8_t y   = BODY_Y + ITEM_H * row_offset;
        bool    sel = (app->settings_sel == SettingsRowFont);
        if(sel) {
            set_fg(canvas, app);
            canvas_draw_box(canvas, 0, y, SCREEN_W, ITEM_H);
            set_bg(canvas, app);
        } else {
            set_fg(canvas, app);
        }
        canvas_draw_str(canvas, 3, y + 8, "Font");
        char fval[32];
        snprintf(fval, sizeof(fval), "[%s]", FONT_LABELS[app->font_choice]);
        canvas_draw_str_aligned(canvas, SCREEN_W - 2, y + 8,
                                AlignRight, AlignBottom, fval);
        set_fg(canvas, app);
    }

    // Row 2 (or 1): Dark mode
    {
        uint8_t row_offset = (app->translation_count > 1) ? 2 : 1;
        uint8_t y   = BODY_Y + ITEM_H * row_offset;
        bool    sel = (app->settings_sel == SettingsRowDark);
        if(sel) {
            set_fg(canvas, app);
            canvas_draw_box(canvas, 0, y, SCREEN_W, ITEM_H);
            set_bg(canvas, app);
        } else {
            set_fg(canvas, app);
        }
        canvas_draw_str(canvas, 3, y + 8, "Dark Mode");
        canvas_draw_str_aligned(canvas, SCREEN_W - 2, y + 8,
                                AlignRight, AlignBottom,
                                app->dark_mode ? "[On]" : "[Off]");
        set_fg(canvas, app);
    }

    // Hint row
    set_fg(canvas, app);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, SCREEN_W / 2, SCREEN_H - 2,
                            AlignCenter, AlignBottom,
                            "OK=Save  Long-OK=List");
}

// Scene: About

static const char* const ABOUT_LINES[] = {
    APP_NAME " v" APP_VERSION,
    "---------------------",
    "  Multi-translation",
    "   Bible study app.",
    "---------------------",
    "SD card directory setup:",
    "  /ext/apps_data/",
    "   fz_bible_app/",
    "  <Translation>/",
    "  <Section>/<Book>/",
    "   <Chap>/verseN.txt",
    "  (1 translation: place",
    "   sections directly in",
    "   fz_bible_app/ folder)",
    "---------------------",
    "CONTROLS",
    "- - - - Main Menu - - - -",
    "   Up/Down = move row",
    "   Left/Right = change",
    "   OK = open / read",
    "   Long-OK = Settings",
    "   Back = exit app",
    "- - - - Reading - - - - -",
    "   Up/Down = scroll",
    "   Left = previous",
    "   Right = next",
    "   Long-OK = bookmark",
    "   Back = back to menu",
    "- - - - Settings - - - - ",
    "   Up/Down = move row",
    "   Left/Right = toggle",
    "   Long-OK = show list",
    "   OK = save & close",
    "- - - - Bookmarks - - - -",
    "   Up/Down = move",
    "   OK = jump to verse",
    "   Back = close",
    "   Hold OK on verse",
    "   to add/remove",
    "   Choose heading or",
    "   Default/Add New",
    "- - - - Search - - - - - ",
    "   Select book first",
    "   OK on Search row",
    "   Type + GO! to search",
    "   Long-L/R = cycle hint",
    "   Long-Up = fill hint",
    "   OK = jump to verse",
    "   Back = refine query",
    "   Long-Back = Main Menu",
    "- - - - About - - - - - -",
    "   Up/Down = scroll",
    "   Back/OK = close",
    "---------------------",
    "Last position is saved",
    "automatically on exit.",
    " ",
};
#define ABOUT_LINE_COUNT ((uint8_t)(sizeof(ABOUT_LINES)/sizeof(ABOUT_LINES[0])))

static void draw_about(Canvas* canvas, App* app) {
    if(app->dark_mode) {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, 0, 0, SCREEN_W, SCREEN_H);
    }
    draw_hdr(canvas, "About");

    set_fg(canvas, app);
    canvas_set_font(canvas, FontSecondary);
    const uint8_t lh  = 10;
    const uint8_t vis = (uint8_t)((SCREEN_H - HDR_H - 2) / lh);

    for(uint8_t i = 0; i < vis; i++) {
        uint8_t li = app->about_scroll + i;
        if(li >= ABOUT_LINE_COUNT) break;
        canvas_draw_str(canvas, 2,
                        HDR_H + 2 + i * lh + lh - 1,
                        ABOUT_LINES[li]);
    }
    draw_scrollbar(canvas, app, app->about_scroll, ABOUT_LINE_COUNT, vis);
}

// Scene: Bookmarks

// Scene: Bookmarks
//
// Top level: lists "Default (N)" and each heading "Name (N)".
//            Only rows with at least one bookmark are shown.
// Drill-down: lists the bookmarks inside the selected heading.

// Count bookmarks belonging to a group (BM_GROUP_NONE = Default).
static uint8_t bm_count_in_group(App* app, uint8_t grp) {
    uint8_t n = 0;
    for(uint8_t i = 0; i < app->bm_count; i++)
        if(app->bookmarks[i].group == grp) n++;
    return n;
}

// Top-level row encoding:
//   0x00..0x1F  = group index (named heading)
//   BM_GROUP_NONE (0xFF) when used as a "row type" → grouped heading "Default"
//   0x80..0xFF (but not BM_GROUP_NONE) = bookmark index + 0x80  (ungrouped bm)
//
// We encode rows as follows:
//   rows[i] < 0x80  → named group index
//   rows[i] == BM_GROUP_NONE → the "Default" group row  (kept for compat)
//   rows[i] >= 0x80 && rows[i] != BM_GROUP_NONE → (rows[i] & 0x7F) = bm index,
//                                                   but we use a separate array
//
// Simpler: use a struct-free parallel array approach.
// rows_type[i]: 0 = heading (rows[i] is group idx or BM_GROUP_NONE),
//               1 = ungrouped bookmark (rows[i] is index into app->bookmarks[])
#define BM_TOP_MAX (MAX_BM_GROUPS + 1 + MAX_BOOKMARKS)
typedef struct { uint8_t kind; uint8_t idx; } BmTopRow;
// kind: 0 = heading group (idx = group index, BM_GROUP_NONE = Default heading)
//       1 = ungrouped bookmark (idx = index into app->bookmarks[])

static uint8_t bm_build_top_rows(App* app, BmTopRow rows[BM_TOP_MAX]) {
    uint16_t n = 0;
    // Named headings first (only those that have bookmarks)
    for(uint8_t g = 0; g < app->bm_group_count && n < BM_TOP_MAX; g++)
        if(bm_count_in_group(app, g) > 0) {
            rows[n].kind = 0; rows[n].idx = g; n++;
        }
    // Then ungrouped bookmarks directly (BM_GROUP_NONE)
    for(uint8_t i = 0; i < app->bm_count && n < BM_TOP_MAX; i++)
        if(app->bookmarks[i].group == BM_GROUP_NONE) {
            rows[n].kind = 1; rows[n].idx = i; n++;
        }
    return (uint8_t)n;
}

static void draw_bookmarks(Canvas* canvas, App* app) {
    if(app->dark_mode) {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, 0, 0, SCREEN_W, SCREEN_H);
    }

    const uint8_t LINE_H = 10;
    const uint8_t vis    = (uint8_t)((SCREEN_H - HDR_H - 2) / LINE_H);

    if(!app->bm_in_group) {
        // ── Top level: headings + ungrouped bookmarks ────────────────────────
        char hdr_buf[24];
        snprintf(hdr_buf, sizeof(hdr_buf), "Bookmarks (%d)", (int)app->bm_count);
        draw_hdr(canvas, hdr_buf);

        set_fg(canvas, app);
        canvas_set_font(canvas, FontSecondary);

        if(app->bm_count == 0) {
            canvas_draw_str_aligned(canvas, SCREEN_W / 2, 34,
                                    AlignCenter, AlignCenter, "No bookmarks");
            canvas_draw_str_aligned(canvas, SCREEN_W / 2, 46,
                                    AlignCenter, AlignCenter, "Hold OK on a verse");
            return;
        }

        BmTopRow rows[BM_TOP_MAX];
        uint8_t row_count = bm_build_top_rows(app, rows);

        if(row_count == 0) return;
        if(app->bm_sel >= row_count) app->bm_sel = row_count - 1;

        if(app->bm_sel < app->bm_scroll) app->bm_scroll = app->bm_sel;
        if(app->bm_sel >= app->bm_scroll + vis)
            app->bm_scroll = (uint8_t)(app->bm_sel - vis + 1);

        for(uint8_t i = 0; i < vis && (app->bm_scroll + i) < row_count; i++) {
            uint8_t ri  = app->bm_scroll + i;
            uint8_t y   = HDR_H + 2 + i * LINE_H;
            bool    sel = (ri == app->bm_sel);

            if(sel) {
                set_fg(canvas, app);
                canvas_draw_box(canvas, 0, y - 1, SCREEN_W - SB_W - 2, LINE_H);
                set_bg(canvas, app);
            } else {
                set_fg(canvas, app);
            }

            if(rows[ri].kind == 0) {
                // Heading row
                char label[BM_GROUP_NAME_LEN + 8];
                uint8_t cnt = bm_count_in_group(app, rows[ri].idx);
                snprintf(label, sizeof(label), "%s (%d)",
                         app->bm_groups[rows[ri].idx], (int)cnt);
                set_ui_font(canvas, label);
                canvas_draw_str(canvas, 4, y + 8, label);
                canvas_set_font(canvas, FontSecondary);
                canvas_draw_str_aligned(canvas, SCREEN_W - SB_W - 4, y + 8,
                                        AlignRight, AlignBottom, ">");
            } else {
                // Ungrouped bookmark row
                uint8_t bi = rows[ri].idx;
                char blabel[14];
                canon_book_label(app, app->bookmarks[bi].canon_book,
                                 blabel, sizeof(blabel));
                char ref[32];
                snprintf(ref, sizeof(ref), "%s %d:%d",
                         blabel,
                         (int)(app->bookmarks[bi].chapter + 1),
                         (int)app->bookmarks[bi].verse);
                set_ui_font(canvas, ref);
                canvas_draw_str(canvas, 4, y + 8, ref);
            }
            set_fg(canvas, app);
        }

        draw_scrollbar(canvas, app, app->bm_scroll, row_count, vis);

    } else {
        // ── Drill-down: bookmarks inside one heading ─────────────────────────
        char hdr_buf[32];
        uint8_t grp = app->bm_open_group;
        uint8_t cnt = bm_count_in_group(app, grp);
        snprintf(hdr_buf, sizeof(hdr_buf), "%s (%d)", app->bm_groups[grp], (int)cnt);
        draw_hdr(canvas, hdr_buf);

        set_fg(canvas, app);
        canvas_set_font(canvas, FontSecondary);

        uint8_t idx[MAX_BOOKMARKS];
        uint8_t idx_count = 0;
        for(uint8_t i = 0; i < app->bm_count; i++)
            if(app->bookmarks[i].group == grp && idx_count < MAX_BOOKMARKS)
                idx[idx_count++] = i;

        if(idx_count == 0) {
            canvas_draw_str_aligned(canvas, SCREEN_W / 2, 38,
                                    AlignCenter, AlignCenter, "(empty)");
            return;
        }

        if(app->bm_sel >= idx_count) app->bm_sel = idx_count - 1;

        if(app->bm_sel < app->bm_scroll) app->bm_scroll = app->bm_sel;
        if(app->bm_sel >= app->bm_scroll + vis)
            app->bm_scroll = (uint8_t)(app->bm_sel - vis + 1);

        for(uint8_t i = 0; i < vis && (app->bm_scroll + i) < idx_count; i++) {
            uint8_t ri = app->bm_scroll + i;
            uint8_t bi = idx[ri];
            uint8_t y  = HDR_H + 2 + i * LINE_H;
            bool    sel = (ri == app->bm_sel);

            if(sel) {
                set_fg(canvas, app);
                canvas_draw_box(canvas, 0, y - 1, SCREEN_W - SB_W - 2, LINE_H);
                set_bg(canvas, app);
            } else {
                set_fg(canvas, app);
            }

            char blabel[14];
            canon_book_label(app, app->bookmarks[bi].canon_book, blabel, sizeof(blabel));
            char ref[32];
            snprintf(ref, sizeof(ref), "%s %d:%d",
                     blabel,
                     (int)(app->bookmarks[bi].chapter + 1),
                     (int)app->bookmarks[bi].verse);
            set_ui_font(canvas, ref);
            canvas_draw_str(canvas, 4, y + 8, ref);
            canvas_set_font(canvas, FontSecondary);
            set_fg(canvas, app);
        }

        draw_scrollbar(canvas, app, app->bm_scroll, idx_count, vis);
    }
}



// ============================================================
// Scene: Bookmark group picker overlay (ViewBmGroupPick)
// List rows: "Default", <existing groups>, "Add New"
// ============================================================

// Total rows in picker = 1 (Default) + bm_group_count + 1 (Add New)
static uint8_t bm_pick_row_count(App* app) {
    return (uint8_t)(2 + app->bm_group_count);
}

void draw_bm_group_pick(Canvas* canvas, App* app) {
    // Draw a centered modal box
    const uint8_t BOX_X  = 4;
    const uint8_t BOX_Y  = 4;
    const uint8_t BOX_W  = SCREEN_W - 8;
    const uint8_t BOX_H  = SCREEN_H - 8;
    const uint8_t ITEM_H = 10;
    const uint8_t HDR_HT = 12;
    const uint8_t BODY_Y = BOX_Y + HDR_HT;
    const uint8_t VIS    = (uint8_t)((BOX_H - HDR_HT) / ITEM_H);

    // Shadow / background
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, BOX_X + 2, BOX_Y + 2, BOX_W, BOX_H);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, BOX_X, BOX_Y, BOX_W, BOX_H);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_frame(canvas, BOX_X, BOX_Y, BOX_W, BOX_H);

    // Header
    canvas_draw_box(canvas, BOX_X, BOX_Y, BOX_W, HDR_HT);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, SCREEN_W / 2, BOX_Y + 1,
                            AlignCenter, AlignTop, "Save under:");
    canvas_set_color(canvas, ColorBlack);

    // Clamp scroll
    uint8_t total = bm_pick_row_count(app);
    if(app->bm_pick_sel < app->bm_pick_scroll)
        app->bm_pick_scroll = app->bm_pick_sel;
    if(app->bm_pick_sel >= app->bm_pick_scroll + VIS)
        app->bm_pick_scroll = (uint8_t)(app->bm_pick_sel - VIS + 1);

    canvas_set_font(canvas, FontSecondary);
    for(uint8_t vi = 0; vi < VIS; vi++) {
        uint8_t ri = app->bm_pick_scroll + vi;
        if(ri >= total) break;
        uint8_t y  = BODY_Y + vi * ITEM_H;
        bool    sel = (ri == app->bm_pick_sel);

        if(sel) {
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_box(canvas, BOX_X + 1, y, BOX_W - 2, ITEM_H);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_set_color(canvas, ColorBlack);
        }

        const char* label;
        char lbuf[BM_GROUP_NAME_LEN + 2];
        if(ri == 0) {
            label = "Default";
        } else if(ri <= app->bm_group_count) {
            snprintf(lbuf, sizeof(lbuf), "%s", app->bm_groups[ri - 1]);
            label = lbuf;
        } else {
            label = "+ Add New";
        }
        canvas_draw_str(canvas, BOX_X + 4, y + 8, label);
    }
    // Mini scrollbar inside box
    if(total > VIS) {
        uint8_t track_h = (uint8_t)(BOX_H - HDR_HT - 2);
        uint8_t bar_h   = (uint8_t)((uint32_t)track_h * VIS / total);
        if(bar_h < 2) bar_h = 2;
        uint8_t bar_y = (uint8_t)(BODY_Y + 1 +
            (uint32_t)(track_h - bar_h) * app->bm_pick_scroll / (total - VIS));
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, BOX_X + BOX_W - 3, bar_y, 2, bar_h);
    }
}

void on_bm_group_pick(App* app, InputEvent* ev) {
    if(ev->type != InputTypeShort && ev->type != InputTypeRepeat) return;
    uint8_t total = bm_pick_row_count(app);
    switch(ev->key) {
    case InputKeyUp:
        if(app->bm_pick_sel > 0) app->bm_pick_sel--;
        else app->bm_pick_sel = total - 1;
        break;
    case InputKeyDown:
        if(app->bm_pick_sel < total - 1) app->bm_pick_sel++;
        else app->bm_pick_sel = 0;
        break;
    case InputKeyOk: {
        uint8_t ri = app->bm_pick_sel;
        uint8_t target_group;
        if(ri == 0) {
            target_group = BM_GROUP_NONE;
        } else if(ri <= app->bm_group_count) {
            target_group = (uint8_t)(ri - 1);
        } else {
            // "Add New" -- open keyboard in naming mode
            memset(app->search_buf, 0, sizeof(app->search_buf));
            app->search_len  = 0;
            app->cursor_pos  = 0;
            app->text_scroll = 0;
            app->cursor_blink = 0;
            app->kb_row  = 0;
            app->kb_col  = 0;
            app->kb_page = 0;
            app->kb_caps = false;
            app->kb_long_consumed      = false;
            app->kb_back_long_consumed = false;
            app->suggest_count = 0;
            app->suggest_sel   = 0;
            app->suggest_long_consumed = false;
            app->bm_naming = true;
            // Signal "create new" to bm_group_name_confirm
            app->bm_edit_group = app->bm_group_count;
            app->view = ViewBmGroupNew;
            break;
        }

        if(app->bm_pick_move) {
            // Reassign existing bookmark to the chosen group
            app->bookmarks[app->bm_item_idx].group = target_group;
            bookmarks_save(app);
            app->bm_pick_move = false;
            // Return to bookmark list top level
            app->bm_in_group = false;
            app->bm_sel      = 0;
            app->bm_scroll   = 0;
            app->view = ViewBookmarks;
        } else {
            // Original behaviour: add a new bookmark under the chosen group
            bookmark_add_with_group(app, target_group);
            app->view = ViewReading;
        }
        break;
    }
    case InputKeyBack:
        // Cancel: return to reading without bookmarking (or bookmarks if moving)
        if(app->bm_pick_move) {
            app->bm_pick_move = false;
            app->view = ViewBookmarks;
        } else {
            app->view = ViewReading;
        }
        break;
    default: break;
    }
}

// ============================================================
// Called by keyboard.c GO! when bm_naming == true.
// Saves the typed name as a new group, adds the bookmark, returns to reading.
// ============================================================
void bm_group_name_confirm(App* app) {
    if(app->search_len > 0) {
        uint8_t len = app->search_len;
        if(len >= BM_GROUP_NAME_LEN) len = BM_GROUP_NAME_LEN - 1;

        if(app->bm_edit_group < app->bm_group_count) {
            // Rename existing group
            memcpy(app->bm_groups[app->bm_edit_group], app->search_buf, len);
            app->bm_groups[app->bm_edit_group][len] = '\0';
            bm_groups_save(app);
            // Restore search state
            memset(app->search_buf, 0, sizeof(app->search_buf));
            app->search_len   = 0;
            app->cursor_pos   = 0;
            app->text_scroll  = 0;
            app->suggest_count = 0;
            app->suggest_sel   = 0;
            app->bm_naming = false;
            app->bm_in_group = false;
            app->bm_sel    = 0;
            app->bm_scroll = 0;
            app->view = ViewBookmarks;
            return;
        } else if(app->bm_group_count < MAX_BM_GROUPS) {
            // Create new group and add the pending bookmark
            memcpy(app->bm_groups[app->bm_group_count], app->search_buf, len);
            app->bm_groups[app->bm_group_count][len] = '\0';
            uint8_t new_grp = app->bm_group_count;
            app->bm_group_count++;
            bm_groups_save(app);
            bookmark_add_with_group(app, new_grp);
        }
    }
    // Restore search state
    memset(app->search_buf, 0, sizeof(app->search_buf));
    app->search_len   = 0;
    app->cursor_pos   = 0;
    app->text_scroll  = 0;
    app->suggest_count = 0;
    app->suggest_sel   = 0;
    app->bm_naming = false;
    app->view = ViewReading;
}


// ============================================================
// Scene: Heading edit overlay (ViewBmHeadingEdit)
// Two options: Rename  /  Delete
// ============================================================

void draw_bm_heading_edit(Canvas* canvas, App* app) {
    const uint8_t BOX_X  = 14;
    const uint8_t BOX_Y  = 12;
    const uint8_t BOX_W  = SCREEN_W - 28;
    const uint8_t BOX_H  = 44;
    const uint8_t HDR_HT = 12;
    const uint8_t ITEM_H = 12;
    const uint8_t BODY_Y = BOX_Y + HDR_HT + 2;

    // Shadow
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, BOX_X + 2, BOX_Y + 2, BOX_W, BOX_H);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, BOX_X, BOX_Y, BOX_W, BOX_H);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_frame(canvas, BOX_X, BOX_Y, BOX_W, BOX_H);

    // Header: group name (truncated)
    canvas_draw_box(canvas, BOX_X, BOX_Y, BOX_W, HDR_HT);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    char hdr[BM_GROUP_NAME_LEN + 2];
    snprintf(hdr, sizeof(hdr), "%s", app->bm_groups[app->bm_edit_group]);
    // Truncate to box width
    hdr[16] = '\0';
    canvas_draw_str_aligned(canvas, SCREEN_W / 2, BOX_Y + 1,
                            AlignCenter, AlignTop, hdr);
    canvas_set_color(canvas, ColorBlack);

    // Two option rows: Rename, Delete
    const char* opts[2] = { "Rename", "Delete" };
    for(uint8_t i = 0; i < 2; i++) {
        uint8_t y   = BODY_Y + i * ITEM_H;
        bool    sel = (app->bm_edit_sel == i);
        if(sel) {
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_box(canvas, BOX_X + 2, y, BOX_W - 4, ITEM_H);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_set_color(canvas, ColorBlack);
        }
        canvas_draw_str_aligned(canvas, SCREEN_W / 2, y + 9,
                                AlignCenter, AlignBottom, opts[i]);
    }
}

void on_bm_heading_edit(App* app, InputEvent* ev) {
    // Suppress the Short that follows the Long which opened this overlay
    if(ev->type == InputTypeRelease && ev->key == InputKeyOk) {
        app->bm_long_consumed = false;
        return;
    }
    if(app->bm_long_consumed) return;

    if(ev->type != InputTypeShort && ev->type != InputTypeRepeat) return;

    switch(ev->key) {
    case InputKeyUp:
    case InputKeyDown:
        app->bm_edit_sel ^= 1;  // toggle between 0 and 1
        break;
    case InputKeyOk:
        if(app->bm_edit_sel == 0) {
            // Rename: open keyboard in naming mode
            // Save which group we're renaming in bm_edit_group (already set)
            memset(app->search_buf, 0, sizeof(app->search_buf));
            // Pre-fill with current name
            strncpy(app->search_buf, app->bm_groups[app->bm_edit_group],
                    MAX_SEARCH_LEN);
            app->search_len = (uint8_t)strlen(app->search_buf);
            app->cursor_pos  = app->search_len;
            app->text_scroll = 0;
            app->cursor_blink = 0;
            app->kb_row  = 0;
            app->kb_col  = 0;
            app->kb_page = 0;
            app->kb_caps = false;
            app->kb_long_consumed      = false;
            app->kb_back_long_consumed = false;
            app->suggest_count = 0;
            app->suggest_sel   = 0;
            app->suggest_long_consumed = false;
            // Use bm_naming=true; bm_group_name_confirm checks bm_edit_group
            // to know it should rename rather than create.
            // We set a separate flag via bm_edit_group != BM_GROUP_NONE already.
            app->bm_naming = true;
            app->view = ViewBmGroupNew;
        } else {
            // Delete: remove all bookmarks in this group, then remove group entry
            uint8_t g = app->bm_edit_group;
            // Remove all bookmarks belonging to this group
            uint8_t w = 0;
            for(uint8_t i = 0; i < app->bm_count; i++) {
                if(app->bookmarks[i].group != g) {
                    // Re-number group indices above g
                    if(app->bookmarks[i].group != BM_GROUP_NONE &&
                       app->bookmarks[i].group > g)
                        app->bookmarks[i].group--;
                    app->bookmarks[w++] = app->bookmarks[i];
                }
            }
            app->bm_count = w;
            // Remove group from bm_groups[]
            for(uint8_t i = g; i + 1 < app->bm_group_count; i++)
                memcpy(app->bm_groups[i], app->bm_groups[i + 1],
                       BM_GROUP_NAME_LEN);
            app->bm_group_count--;
            bm_groups_save(app);
            bookmarks_save(app);
            // Return to top-level bookmark list
            app->bm_in_group = false;
            app->bm_sel      = 0;
            app->bm_scroll   = 0;
            app->view = ViewBookmarks;
        }
        break;
    case InputKeyBack:
        // Cancel – return to bookmarks top level
        app->bm_in_group = false;
        app->bm_sel      = 0;
        app->bm_scroll   = 0;
        app->view = ViewBookmarks;
        break;
    default: break;
    }
}

// ============================================================
// Scene: Bookmark item edit overlay (ViewBmItemEdit)
// Two options: Add to Heading  /  Delete
// ============================================================

void draw_bm_item_edit(Canvas* canvas, App* app) {
    const uint8_t BOX_X  = 10;
    const uint8_t BOX_Y  = 12;
    const uint8_t BOX_W  = SCREEN_W - 20;
    const uint8_t BOX_H  = 44;
    const uint8_t HDR_HT = 12;
    const uint8_t ITEM_H = 12;
    const uint8_t BODY_Y = BOX_Y + HDR_HT + 2;

    // Shadow
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, BOX_X + 2, BOX_Y + 2, BOX_W, BOX_H);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, BOX_X, BOX_Y, BOX_W, BOX_H);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_frame(canvas, BOX_X, BOX_Y, BOX_W, BOX_H);

    // Header: bookmark reference
    canvas_draw_box(canvas, BOX_X, BOX_Y, BOX_W, HDR_HT);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    uint8_t bi = app->bm_item_idx;
    char blabel[14];
    canon_book_label(app, app->bookmarks[bi].canon_book, blabel, sizeof(blabel));
    char ref[28];
    snprintf(ref, sizeof(ref), "%s %d:%d",
             blabel,
             (int)(app->bookmarks[bi].chapter + 1),
             (int)app->bookmarks[bi].verse);
    canvas_draw_str_aligned(canvas, SCREEN_W / 2, BOX_Y + 1,
                            AlignCenter, AlignTop, ref);
    canvas_set_color(canvas, ColorBlack);

    // Two option rows
    const char* opts[2] = { "Add to Heading", "Delete" };
    for(uint8_t i = 0; i < 2; i++) {
        uint8_t y   = BODY_Y + i * ITEM_H;
        bool    sel = (app->bm_item_sel == i);
        if(sel) {
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_box(canvas, BOX_X + 2, y, BOX_W - 4, ITEM_H);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_set_color(canvas, ColorBlack);
        }
        canvas_draw_str_aligned(canvas, SCREEN_W / 2, y + 9,
                                AlignCenter, AlignBottom, opts[i]);
    }
}

void on_bm_item_edit(App* app, InputEvent* ev) {
    // Suppress the Short that fires right after the long-press that opened us
    if(ev->type == InputTypeRelease && ev->key == InputKeyOk) {
        app->bm_long_consumed = false;
        return;
    }
    if(app->bm_long_consumed) return;

    if(ev->type != InputTypeShort && ev->type != InputTypeRepeat) return;

    switch(ev->key) {
    case InputKeyUp:
    case InputKeyDown:
        app->bm_item_sel ^= 1;
        break;
    case InputKeyOk:
        if(app->bm_item_sel == 0) {
            // Add to Heading: open the group picker in "move" mode
            app->bm_pick_sel    = 0;
            app->bm_pick_scroll = 0;
            app->bm_pick_move   = true;
            app->view = ViewBmGroupPick;
        } else {
            // Delete this bookmark
            uint8_t bi = app->bm_item_idx;
            for(uint8_t i = bi; i + 1 < app->bm_count; i++)
                app->bookmarks[i] = app->bookmarks[i + 1];
            app->bm_count--;
            bookmarks_save(app);
            // Return to whichever level we came from (in-group or top)
            app->bm_sel    = 0;
            app->bm_scroll = 0;
            if(app->bm_in_group && bm_count_in_group(app, app->bm_open_group) == 0)
                app->bm_in_group = false;
            app->view = ViewBookmarks;
        }
        break;
    case InputKeyBack:
        app->view = ViewBookmarks;
        break;
    default: break;
    }
}

static void draw_loading(Canvas* canvas, App* app) {
    if(app->dark_mode) {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, 0, 0, SCREEN_W, SCREEN_H);
    }
    draw_hdr(canvas, APP_NAME " v" APP_VERSION);
    set_fg(canvas, app);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, SCREEN_W / 2, SCREEN_H / 2 + 4,
                            AlignCenter, AlignCenter, "Loading...");
}

static void draw_error(Canvas* canvas, App* app) {
    draw_hdr(canvas, "Error");
    set_fg(canvas, app);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, SCREEN_W / 2, SCREEN_H / 2,
                            AlignCenter, AlignCenter, app->error_msg);
}

// Main draw callback

static void draw_cb(Canvas* canvas, void* ctx) {
    App* app = (App*)ctx;
    canvas_clear(canvas);
    switch(app->view) {
    case ViewMenu:     draw_menu(canvas, app);     break;
    case ViewReading:  draw_reading(canvas, app);  break;
    case ViewSettings: draw_settings(canvas, app); break;
    case ViewAbout:    draw_about(canvas, app);    break;
    case ViewBookmarks:     draw_bookmarks(canvas, app);     break;
    case ViewBmGroupPick:   draw_bm_group_pick(canvas, app); break;
    case ViewBmGroupNew:    draw_search_input(canvas, app);  break;  // real keyboard
    case ViewBmHeadingEdit: draw_bm_heading_edit(canvas, app); break;
    case ViewBmItemEdit:    draw_bm_item_edit(canvas, app);   break;
    case ViewSearch:        draw_search_input(canvas, app);   break;
    case ViewSearchResults: draw_search_results(canvas, app); break;
    case ViewLoading:  draw_loading(canvas, app);  break;
    case ViewError:    draw_error(canvas, app);    break;
    }
}

// Input callback

static void input_cb(InputEvent* ev, void* ctx) {
    App* app = (App*)ctx;
    furi_message_queue_put(app->queue, ev, 0);
}

// Input: Menu

static void on_menu(App* app, InputEvent* ev) {
    // Long-press OK -> Settings
    if(ev->type == InputTypeLong && ev->key == InputKeyOk) {
        app->settings_sel        = (app->translation_count > 1) ?
                                   SettingsRowTranslation : SettingsRowFont;
        app->settings_font_sel   = (uint8_t)app->font_choice;
        app->settings_font_open  = false;
        app->settings_trans_open = false;
        app->settings_long_consumed = false;
        app->menu_long_consumed  = true;
        app->view = ViewSettings;
        return;
    }
    if(ev->type == InputTypeRelease && ev->key == InputKeyOk) {
        app->menu_long_consumed = false;
        return;
    }
    if(app->menu_long_consumed) return;

    // Long-press Left/Right on Chapter or Verse row: engage fast scroll (step 5)
    if(ev->type == InputTypeLong &&
       (ev->key == InputKeyLeft || ev->key == InputKeyRight) &&
       (app->sel_row == RowChapter || app->sel_row == RowVerse)) {
        app->menu_fast_scroll = true;
        // Fall through to also move by 1 on this first long event
    }

    // Release clears fast-scroll mode
    if(ev->type == InputTypeRelease &&
       (ev->key == InputKeyLeft || ev->key == InputKeyRight)) {
        app->menu_fast_scroll = false;
        return;
    }

    if(ev->type != InputTypeShort && ev->type != InputTypeRepeat &&
       ev->type != InputTypeLong) return;
    // Long events only continue for Left/Right (handled above); skip all others
    if(ev->type == InputTypeLong &&
       !(ev->key == InputKeyLeft || ev->key == InputKeyRight)) return;

    // Step size: 5 during fast-scroll repeats, 1 otherwise
    uint8_t step = (app->menu_fast_scroll && ev->type == InputTypeRepeat) ? 5 : 1;

    switch(ev->key) {
    case InputKeyUp:
        if(app->sel_row > 0) app->sel_row--;
        else app->sel_row = (MenuRow)(MENU_ROWS - 1);  // wrap to bottom
        break;
    case InputKeyDown:
        if((uint8_t)app->sel_row < MENU_ROWS - 1) app->sel_row++;
        else app->sel_row = (MenuRow)0;  // wrap to top
        break;

    case InputKeyLeft:
        switch(app->sel_row) {
        case RowSection: {
            // Find current position in avail_sections[]
            uint8_t pos = 0;
            for(uint8_t i = 0; i < app->avail_section_count; i++)
                if(app->avail_sections[i] == app->section_idx) { pos = i; break; }
            pos = (pos > 0) ? pos - 1 : app->avail_section_count - 1;
            app->section_idx = app->avail_sections[pos];
            app->book_idx = app->chapter_idx = app->verse_idx = 0;
            rebuild_book_list(app);
            refresh_chapter_count(app);
            refresh_verse_count(app);
            break;
        }
        case RowBook:
            if(app->book_count == 0) break;
            app->book_idx = (app->book_idx > 0) ?
                app->book_idx - 1 : app->book_count - 1;
            app->chapter_idx = app->verse_idx = 0;
            refresh_chapter_count(app);
            refresh_verse_count(app);
            break;
        case RowChapter:
            if(app->chapter_count == 0) break;
            if(app->chapter_idx >= step)
                app->chapter_idx = app->chapter_idx - step;
            else
                app->chapter_idx = app->chapter_count - 1;  // wrap to last
            app->verse_idx = 0;
            refresh_verse_count(app);
            break;
        case RowVerse:
            if(app->verse_idx >= step)
                app->verse_idx = app->verse_idx - step;
            else
                app->verse_idx = app->verse_count;  // wrap to last verse (0=All wraps to last)
            break;
        default: break;
        }
        break;

    case InputKeyRight:
        switch(app->sel_row) {
        case RowSection: {
            uint8_t pos = 0;
            for(uint8_t i = 0; i < app->avail_section_count; i++)
                if(app->avail_sections[i] == app->section_idx) { pos = i; break; }
            pos = (pos < app->avail_section_count - 1) ? pos + 1 : 0;
            app->section_idx = app->avail_sections[pos];
            app->book_idx = app->chapter_idx = app->verse_idx = 0;
            rebuild_book_list(app);
            refresh_chapter_count(app);
            refresh_verse_count(app);
            break;
        }
        case RowBook:
            if(app->book_count == 0) break;
            app->book_idx = (app->book_idx < app->book_count - 1) ?
                app->book_idx + 1 : 0;
            app->chapter_idx = app->verse_idx = 0;
            refresh_chapter_count(app);
            refresh_verse_count(app);
            break;
        case RowChapter:
            if(app->chapter_count == 0) break;
            if(app->chapter_idx + step < app->chapter_count)
                app->chapter_idx = app->chapter_idx + step;
            else
                app->chapter_idx = 0;  // wrap to first
            app->verse_idx = 0;
            refresh_verse_count(app);
            break;
        case RowVerse:
            if(app->verse_idx + step <= app->verse_count)
                app->verse_idx = app->verse_idx + step;
            else
                app->verse_idx = 0;  // wrap to All (0)
            break;
        default: break;
        }
        break;

    case InputKeyOk:
        switch(app->sel_row) {
        case RowSearch:
            // Open search keyboard for current book
            memset(app->search_buf, 0, sizeof(app->search_buf));
            app->search_len = 0;
            app->kb_row  = 0;
            app->kb_col  = 0;
            app->kb_page = 0;
            app->kb_caps = false;
            app->hit_count  = 0;
            app->hit_sel    = 0;
            app->hit_scroll = 0;
            app->suggest_count = 0;
            app->suggest_sel   = 0;
            app->suggest_long_consumed = false;
            app->kb_back_long_consumed = false;
            app->cursor_pos   = 0;
            app->text_scroll  = 0;
            app->cursor_blink = 0;
            app->view = ViewSearch;
            break;
        case RowBookmarks:
            app->bm_sel      = 0;
            app->bm_scroll   = 0;
            app->bm_in_group = false;
            app->view = ViewBookmarks;
            break;
        case RowSettings:
            app->settings_sel        = (app->translation_count > 1) ?
                                       SettingsRowTranslation : SettingsRowFont;
            app->settings_font_sel   = (uint8_t)app->font_choice;
            app->settings_font_open  = false;
            app->settings_trans_open = false;
            app->settings_long_consumed = false;
            app->view = ViewSettings;
            break;
        case RowAbout:
            app->about_scroll = 0;
            app->view = ViewAbout;
            break;
        default:
            // Picker rows -- open reading view
            if(app->book_count == 0 || app->chapter_count == 0) {
                strncpy(app->error_msg, "No data on SD card!",
                        sizeof(app->error_msg) - 1);
                app->error_msg[sizeof(app->error_msg) - 1] = '\0';
                app->view = ViewError;
                break;
            }
            settings_save(app);
            app->prev_view = ViewMenu;
            open_reading(app);
            break;
        }
        break;

    case InputKeyBack:
        app->running = false;
        break;

    default: break;
    }
}

// Load chapter content, wrap text, reset scroll, switch to reading view
void open_reading(App* app) {
    app->view = ViewLoading;
    view_port_update(app->view_port);
    load_chapter(app);
    wrap_chapter(app);
    app->wrap.scroll = 0;
    app->view = ViewReading;
}

// Input: Reading

static void on_reading(App* app, InputEvent* ev) {
    // Long-press OK in single-verse mode: toggle bookmark
    if(ev->type == InputTypeLong && ev->key == InputKeyOk) {
        if(app->verse_idx != 0) bookmark_toggle(app);
        return;
    }
    if(ev->type != InputTypeShort && ev->type != InputTypeRepeat) return;
    uint8_t vis = font_visible_lines(app->font_choice);

    switch(ev->key) {
    case InputKeyUp:
        if(app->wrap.scroll > 0) {
            app->wrap.scroll--;
        } else if(app->verse_idx == 0 && app->verse_page_start > 1) {
            // At top of page and there are earlier verses -- load previous page
            app->view = ViewLoading;
            view_port_update(app->view_port);
            uint8_t new_start = (app->verse_page_start > VERSE_PAGE_SIZE) ?
                                app->verse_page_start - VERSE_PAGE_SIZE : 1;
            load_verse_page(app, new_start);
            wrap_chapter(app);
            // Land at bottom of newly loaded page
            uint8_t vis2 = font_visible_lines(app->font_choice);
            app->wrap.scroll = (app->wrap.count > vis2) ?
                               app->wrap.count - vis2 : 0;
            app->view = ViewReading;
        }
        break;
    case InputKeyDown:
        if(app->wrap.scroll + vis < app->wrap.count) {
            app->wrap.scroll++;
        } else if(app->verse_idx == 0) {
            // At bottom of page -- check if more verses exist
            uint8_t next_start = app->verse_page_start + app->chapter.count;
            if(next_start <= app->verse_count) {
                app->view = ViewLoading;
                view_port_update(app->view_port);
                load_verse_page(app, next_start);
                wrap_chapter(app);
                // wrap_chapter resets scroll to 0 -- land at top of new page
                app->view = ViewReading;
            }
        }
        break;
    case InputKeyLeft:
        if(app->verse_idx == 0) {
            // All mode: page back through verses in this chapter
            if(app->verse_page_start > 1) {
                app->view = ViewLoading;
                view_port_update(app->view_port);
                uint8_t new_start = (app->verse_page_start > VERSE_PAGE_SIZE) ?
                                    app->verse_page_start - VERSE_PAGE_SIZE : 1;
                load_verse_page(app, new_start);
                wrap_chapter(app);
                app->wrap.scroll = 0;
                app->view = ViewReading;
            }
        } else {
            // Single-verse mode: navigate to previous verse/chapter
            if(app->verse_idx > 1) {
                app->verse_idx--;
            } else if(app->verse_idx == 1) {
                app->verse_idx = 0;
            } else {
                if(app->chapter_idx > 0) {
                    app->chapter_idx--;
                } else if(app->book_idx > 0) {
                    app->book_idx--;
                    refresh_chapter_count(app);
                    app->chapter_idx = (app->chapter_count > 0) ?
                        app->chapter_count - 1 : 0;
                }
                refresh_verse_count(app);
                app->verse_idx = 0;
            }
            open_reading(app);
        }
        break;
    case InputKeyRight:
        if(app->verse_idx == 0) {
            // All mode: page forward through verses in this chapter
            uint8_t next_start = app->verse_page_start + app->chapter.count;
            if(next_start <= app->verse_count) {
                app->view = ViewLoading;
                view_port_update(app->view_port);
                load_verse_page(app, next_start);
                wrap_chapter(app);
                app->wrap.scroll = 0;
                app->view = ViewReading;
            }
        } else {
            // Single-verse mode: navigate to next verse/chapter
            if(app->verse_idx < app->verse_count) {
                app->verse_idx++;
            } else {
                if(app->chapter_idx < app->chapter_count - 1)
                    app->chapter_idx++;
                else if(app->book_idx < app->book_count - 1) {
                    app->book_idx++;
                    refresh_chapter_count(app);
                    app->chapter_idx = 0;
                }
                refresh_verse_count(app);
                app->verse_idx = 1;
            }
            open_reading(app);
        }
        break;
    case InputKeyBack:
        app->view = app->prev_view;
        break;
    default: break;
    }
}

// Called after translation_idx has been updated. Preserves section/book/chapter/verse
// where possible; only resets what is no longer valid in the new translation.
static void apply_translation_switch(App* app) {
    rebuild_section_list(app);
    // rebuild_section_list already clamped section_idx if the section is missing.
    // Either way, rebuild the book list for the (possibly unchanged) section.
    rebuild_book_list(app);
    // book_idx, chapter_idx, verse_idx are clamped by refresh_* as usual.
    refresh_chapter_count(app);
    refresh_verse_count(app);
    memset(&app->chapter, 0, sizeof(app->chapter));
    memset(&app->wrap,    0, sizeof(app->wrap));
    // Reload keyword suggestions for the new translation's language.
    // section_lang_en has been set by rebuild_section_list() above, so
    // keywords_load() will now pick the correct file (keywords_de.txt or
    // keywords_en.txt) or built-in list.
    keywords_load(app);
}

// Input: Settings

static void on_settings(App* app, InputEvent* ev) {

    // -- Font list is open --
    if(app->settings_font_open) {
        if(ev->type != InputTypeShort && ev->type != InputTypeRepeat) return;
        switch(ev->key) {
        case InputKeyUp:
            if(app->settings_font_sel > 0) app->settings_font_sel--;
            break;
        case InputKeyDown:
            if(app->settings_font_sel < FONT_COUNT - 1) app->settings_font_sel++;
            break;
        case InputKeyOk:
            app->font_choice = (FontChoice)app->settings_font_sel;
            app->settings_font_open = false;
            app->settings_long_consumed = false;
            if(app->chapter.count > 0) { wrap_chapter(app); app->wrap.scroll = 0; }
            break;
        case InputKeyBack:
            app->settings_font_open = false;
            app->settings_long_consumed = false;
            break;
        default: break;
        }
        return;
    }

    // -- Translation list is open --
    if(app->settings_trans_open) {
        if(ev->type != InputTypeShort && ev->type != InputTypeRepeat) return;
        switch(ev->key) {
        case InputKeyUp:
            if(app->settings_trans_sel > 0) app->settings_trans_sel--;
            break;
        case InputKeyDown:
            if(app->settings_trans_sel < app->translation_count - 1)
                app->settings_trans_sel++;
            break;
        case InputKeyOk:
            if(app->settings_trans_sel != app->translation_idx) {
                app->translation_idx = app->settings_trans_sel;
                apply_translation_switch(app);
            }
            app->settings_trans_open = false;
            app->settings_long_consumed = false;
            break;
        case InputKeyBack:
            app->settings_trans_open = false;
            app->settings_long_consumed = false;
            break;
        default: break;
        }
        return;
    }

    // -- Collapsed settings view --

    // Long-press OK opens the relevant list for Font or Translation rows.
    // Set long_consumed so the following Short/Repeat event is ignored.
    if(ev->type == InputTypeLong && ev->key == InputKeyOk) {
        if(app->settings_sel == SettingsRowFont) {
            app->settings_font_sel  = (uint8_t)app->font_choice;
            app->settings_font_open = true;
            app->settings_long_consumed = true;
            return;
        }
        if(app->settings_sel == SettingsRowTranslation && app->translation_count > 1) {
            app->settings_trans_sel  = app->translation_idx;
            app->settings_trans_open = true;
            app->settings_long_consumed = true;
            return;
        }
    }

    // Release clears the consumed flag
    if(ev->type == InputTypeRelease && ev->key == InputKeyOk) {
        app->settings_long_consumed = false;
        return;
    }

    // Suppress the Short event that fires immediately after the long-press
    if(app->settings_long_consumed) return;

    if(ev->type != InputTypeShort && ev->type != InputTypeRepeat) return;

    uint8_t row_count = (app->translation_count > 1) ? 3 : 2;

    switch(ev->key) {
    case InputKeyUp:
        if((uint8_t)app->settings_sel > 0)
            app->settings_sel = (SettingsRow)(app->settings_sel - 1);
        break;
    case InputKeyDown:
        if((uint8_t)app->settings_sel < row_count - 1)
            app->settings_sel = (SettingsRow)(app->settings_sel + 1);
        break;
    case InputKeyLeft:
    case InputKeyRight:
        if(app->settings_sel == SettingsRowDark) {
            app->dark_mode = !app->dark_mode;
        } else if(app->settings_sel == SettingsRowFont) {
            if(ev->key == InputKeyLeft) {
                app->settings_font_sel = (app->settings_font_sel > 0) ?
                    app->settings_font_sel - 1 : FONT_COUNT - 1;
            } else {
                app->settings_font_sel = (app->settings_font_sel < FONT_COUNT - 1) ?
                    app->settings_font_sel + 1 : 0;
            }
            app->font_choice = (FontChoice)app->settings_font_sel;
            if(app->chapter.count > 0) { wrap_chapter(app); app->wrap.scroll = 0; }
        } else if(app->settings_sel == SettingsRowTranslation &&
                  app->translation_count > 1) {
            if(ev->key == InputKeyLeft) {
                app->translation_idx = (app->translation_idx > 0) ?
                    app->translation_idx - 1 : app->translation_count - 1;
            } else {
                app->translation_idx = (app->translation_idx < app->translation_count - 1) ?
                    app->translation_idx + 1 : 0;
            }
            apply_translation_switch(app);
        }
        break;
    case InputKeyOk:
        settings_save(app);
        app->menu_long_consumed = false;
        app->view = ViewMenu;
        break;
    case InputKeyBack:
        app->menu_long_consumed = false;
        app->view = ViewMenu;
        break;
    default: break;
    }
}

static void on_about(App* app, InputEvent* ev) {
    if(ev->type != InputTypeShort && ev->type != InputTypeRepeat) return;
    const uint8_t lh  = 10;
    const uint8_t vis = (uint8_t)((SCREEN_H - HDR_H - 2) / lh);
    switch(ev->key) {
    case InputKeyUp:
        if(app->about_scroll > 0) app->about_scroll--;
        break;
    case InputKeyDown:
        if(app->about_scroll + vis < ABOUT_LINE_COUNT) app->about_scroll++;
        break;
    case InputKeyBack:
    case InputKeyOk:
        app->about_scroll = 0;
        app->view = ViewMenu;
        break;
    default: break;
    }
}

// Input: Bookmarks

static void on_bookmarks(App* app, InputEvent* ev) {
    const uint8_t LINE_H = 10;
    const uint8_t vis    = (uint8_t)((SCREEN_H - HDR_H - 2) / LINE_H);

    if(!app->bm_in_group) {
        // ── Top level ────────────────────────────────────────────────────────

        BmTopRow rows[BM_TOP_MAX];
        uint8_t row_count = bm_build_top_rows(app, rows);
        if(row_count == 0) { app->view = ViewMenu; return; }
        if(app->bm_sel >= row_count) app->bm_sel = row_count - 1;

        // Long-press OK on a heading row → open heading edit overlay
        // Long-press OK on an ungrouped bookmark → open bookmark item edit overlay
        if(ev->type == InputTypeLong && ev->key == InputKeyOk) {
            if(app->bm_sel < row_count) {
                if(rows[app->bm_sel].kind == 0) {
                    app->bm_edit_group    = rows[app->bm_sel].idx;
                    app->bm_edit_sel      = 0;
                    app->bm_long_consumed = true;
                    app->view = ViewBmHeadingEdit;
                } else {
                    app->bm_item_idx      = rows[app->bm_sel].idx;
                    app->bm_item_sel      = 0;
                    app->bm_long_consumed = true;
                    app->view = ViewBmItemEdit;
                }
            }
            return;
        }
        // Release clears the consumed flag
        if(ev->type == InputTypeRelease && ev->key == InputKeyOk) {
            app->bm_long_consumed = false;
            return;
        }
        // Suppress Short/Repeat that fires right after the long press
        if(app->bm_long_consumed) return;

        if(ev->type != InputTypeShort && ev->type != InputTypeRepeat) return;

        switch(ev->key) {
        case InputKeyUp:
            if(app->bm_sel > 0) app->bm_sel--;
            else app->bm_sel = row_count - 1;
            break;
        case InputKeyDown:
            if(app->bm_sel < row_count - 1) app->bm_sel++;
            else app->bm_sel = 0;
            break;
        case InputKeyOk: {
            if(app->bm_sel >= row_count) break;
            if(rows[app->bm_sel].kind == 0) {
                // Heading → drill into it
                app->bm_open_group = rows[app->bm_sel].idx;
                app->bm_in_group   = true;
                app->bm_sel        = 0;
                app->bm_scroll     = 0;
            } else {
                // Ungrouped bookmark → jump to reading
                uint8_t bi = rows[app->bm_sel].idx;
                uint8_t cb = app->bookmarks[bi].canon_book;
                app->section_idx = app->bookmarks[bi].sec;
                rebuild_book_list(app);
                app->book_idx = 0;
                for(uint8_t k = 0; k < app->book_count; k++) {
                    if(app->book_list[k] == cb) { app->book_idx = k; break; }
                }
                app->chapter_idx = app->bookmarks[bi].chapter;
                app->verse_idx   = app->bookmarks[bi].verse;
                refresh_chapter_count(app);
                refresh_verse_count(app);
                app->prev_view = ViewBookmarks;
                open_reading(app);
            }
            break;
        }
        case InputKeyBack:
            app->view = ViewMenu;
            break;
        default: break;
        }

        // Clamp scroll
        if(app->bm_sel < app->bm_scroll) app->bm_scroll = app->bm_sel;
        if(app->bm_sel >= app->bm_scroll + vis)
            app->bm_scroll = (uint8_t)(app->bm_sel - vis + 1);

    } else {
        // ── Drill-down: bookmarks inside a heading ───────────────────────────

        uint8_t idx[MAX_BOOKMARKS];
        uint8_t idx_count = 0;
        for(uint8_t i = 0; i < app->bm_count; i++)
            if(app->bookmarks[i].group == app->bm_open_group &&
               idx_count < MAX_BOOKMARKS)
                idx[idx_count++] = i;

        if(idx_count == 0) {
            app->bm_in_group = false;
            app->bm_sel = 0;
            app->bm_scroll = 0;
            return;
        }

        // Long-press OK on a bookmark → item edit overlay
        if(ev->type == InputTypeLong && ev->key == InputKeyOk) {
            if(app->bm_sel < idx_count) {
                app->bm_item_idx      = idx[app->bm_sel];
                app->bm_item_sel      = 0;
                app->bm_long_consumed = true;
                app->view = ViewBmItemEdit;
            }
            return;
        }
        if(ev->type == InputTypeRelease && ev->key == InputKeyOk) {
            app->bm_long_consumed = false;
            return;
        }
        if(app->bm_long_consumed) return;

        if(ev->type != InputTypeShort && ev->type != InputTypeRepeat) return;

        switch(ev->key) {
        case InputKeyUp:
            if(app->bm_sel > 0) app->bm_sel--;
            else app->bm_sel = idx_count - 1;
            break;
        case InputKeyDown:
            if(app->bm_sel < idx_count - 1) app->bm_sel++;
            else app->bm_sel = 0;
            break;
        case InputKeyOk: {
            if(app->bm_sel >= idx_count) break;
            uint8_t bi = idx[app->bm_sel];
            uint8_t cb = app->bookmarks[bi].canon_book;
            app->section_idx = app->bookmarks[bi].sec;
            rebuild_book_list(app);
            app->book_idx = 0;
            for(uint8_t k = 0; k < app->book_count; k++) {
                if(app->book_list[k] == cb) { app->book_idx = k; break; }
            }
            app->chapter_idx = app->bookmarks[bi].chapter;
            app->verse_idx   = app->bookmarks[bi].verse;
            refresh_chapter_count(app);
            refresh_verse_count(app);
            app->prev_view = ViewBookmarks;
            open_reading(app);
            break;
        }
        case InputKeyBack:
            app->bm_in_group = false;
            app->bm_sel      = 0;
            app->bm_scroll   = 0;
            break;
        default: break;
        }

        if(app->bm_sel < app->bm_scroll) app->bm_scroll = app->bm_sel;
        if(app->bm_sel >= app->bm_scroll + vis)
            app->bm_scroll = (uint8_t)(app->bm_sel - vis + 1);
    }
}

// Entry point

int32_t luther1912_app(void* p) {
    UNUSED(p);

    App* app = malloc(sizeof(App));
    if(!app) return -1;
    memset(app, 0, sizeof(App));

    app->running   = true;
    app->view      = ViewLoading;
    app->sel_row   = RowSection;
    app->prev_view = ViewMenu;

    app->storage = furi_record_open(RECORD_STORAGE);

    app->queue     = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, draw_cb, app);
    view_port_input_callback_set(app->view_port, input_cb, app);
    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
    view_port_update(app->view_port);

    storage_simply_mkdir(app->storage, DATA_DIR);

    // Scan for available translations before loading settings so the
    // restored translation_idx can be validated against what's actually present
    translations_scan(app);

    // Load saved settings + last position
    settings_load(app);

    // Clamp restored translation_idx in case translations changed on the card
    if(app->translation_idx >= app->translation_count)
        app->translation_idx = 0;

    // Load bookmarks
    bookmarks_load(app);

    // Load bookmark groups (headings)
    bm_groups_load(app);

    // Build available section list for the active translation, then book list.
    // This must happen before keywords_load so section_lang_en is set correctly,
    // allowing the right built-in keyword list (German vs English) to be chosen.
    rebuild_section_list(app);
    rebuild_book_list(app);

    // Load keyword suggestions (uses section_lang_en set above)
    keywords_load(app);

    // Clamp restored indices to what's actually on the SD
    refresh_chapter_count(app);
    refresh_verse_count(app);

    app->view = ViewMenu;
    view_port_update(app->view_port);

    InputEvent ev;
    while(app->running) {
        if(furi_message_queue_get(app->queue, &ev, 100) != FuriStatusOk)
            continue;

        switch(app->view) {
        case ViewMenu:     on_menu(app, &ev);     break;
        case ViewReading:  on_reading(app, &ev);  break;
        case ViewSettings: on_settings(app, &ev); break;
        case ViewSearch:        on_search(app, &ev);         break;
        case ViewBmGroupNew:    on_search(app, &ev);         break;  // real keyboard
        case ViewSearchResults: on_search_results(app, &ev); break;
        case ViewBookmarks:     on_bookmarks(app, &ev);       break;
        case ViewBmGroupPick:   on_bm_group_pick(app, &ev);   break;
        case ViewBmHeadingEdit: on_bm_heading_edit(app, &ev); break;
        case ViewBmItemEdit:    on_bm_item_edit(app, &ev);    break;
        case ViewAbout:    on_about(app, &ev);    break;
        case ViewError:
            if(ev.type == InputTypeShort && ev.key == InputKeyBack)
                app->view = ViewMenu;
            break;
        case ViewLoading: break;
        }

        view_port_update(app->view_port);
    }

    // Save position on clean exit
    settings_save(app);

    gui_remove_view_port(app->gui, app->view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(app->view_port);
    furi_message_queue_free(app->queue);
    furi_record_close(RECORD_STORAGE);
    free(app);
    return 0;
}
