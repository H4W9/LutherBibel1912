#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <gui/gui.h>
#include <gui/canvas.h>
#include <gui/view_port.h>
#include <input/input.h>
#include <storage/storage.h>

// ============================================================
// Constants
// ============================================================

#define SCREEN_W        128
#define SCREEN_H         64
#define DATA_DIR        "/ext/apps_data/fz_bible_app"
#define SETTINGS_PATH   DATA_DIR "/settings.txt"

#define BOOKMARKS_PATH  DATA_DIR "/bookmarks.txt"
#define MAX_BOOKMARKS    50

#define MAX_SECTIONS      4
#define MAX_BOOKS        80
#define MAX_VERSES       25  // cache holds at most one page (VERSE_PAGE_SIZE)
#define NAME_LEN         48
#define VERSE_BUF_LEN   512

#define HDR_H            12
#define MENU_ROW_H       10
#define MENU_ROWS         8
#define MENU_VIS          5
#define MENU_BODY_Y      (HDR_H + 1)
#define READ_BODY_Y      (HDR_H + 2)
#define VERSE_PAGE_SIZE  25
#define WRAP_MAX_LINES  512

#define SB_W              3
#define SB_X             (SCREEN_W - SB_W - 1)

#define FONT_COUNT        5

#define KB_NROWS          3
#define KB_NCOLS         13
#define MAX_SEARCH_LEN   64
#define MAX_SEARCH_HITS  30

#define KEYWORDS_PATH   DATA_DIR "/keywords.txt"
#define MAX_KEYWORDS    200   // max words loaded from file
#define KEYWORD_WORD_LEN 32   // max chars per keyword
#define SUGGEST_MAX       5   // candidates held at once

#define MAX_TRANSLATIONS   5
#define TRANSLATION_NAME_LEN 32

// ============================================================
// Enums
// ============================================================

typedef enum {
    ViewMenu,
    ViewReading,
    ViewSettings,
    ViewAbout,
    ViewSearch,
    ViewSearchResults,
    ViewBookmarks,
    ViewLoading,
    ViewError,
} AppView;

typedef enum {
    RowSection  = 0,
    RowBook     = 1,
    RowChapter  = 2,
    RowVerse    = 3,
    RowSearch   = 4,
    RowBookmarks= 5,
    RowSettings = 6,
    RowAbout    = 7,
} MenuRow;

typedef enum {
    FontSecondaryBuiltin = 0,
    FontSmall            = 1,
    FontMedium           = 2,
    FontLarge            = 3,
    FontXLarge           = 4,
} FontChoice;

typedef enum {
    SettingsRowTranslation = 0,
    SettingsRowFont        = 1,
    SettingsRowDark        = 2,
    SettingsRowCount       = 3,
} SettingsRow;

// ============================================================
// Structs
// ============================================================

typedef struct {
    char     lines[WRAP_MAX_LINES][40];
    uint16_t count;
    uint16_t scroll;
} WrapState;

typedef struct {
    char     text[MAX_VERSES][VERSE_BUF_LEN];
    uint16_t count;
} ChapterCache;

typedef struct App {
    Gui*              gui;
    ViewPort*         view_port;
    FuriMessageQueue* queue;
    Storage*          storage;

    bool    running;
    AppView view;

    // Menu state
    MenuRow sel_row;
    uint8_t menu_scroll;
    uint8_t section_idx;
    uint8_t book_list[MAX_BOOKS];
    uint8_t book_count;
    uint8_t book_idx;
    uint8_t chapter_count;
    uint8_t chapter_idx;
    uint8_t verse_count;
    uint8_t verse_idx;       // 0 = All
    bool    menu_fast_scroll; // true after 1s hold on Left/Right at Chapter/Verse row

    // Reading
    ChapterCache chapter;
    WrapState    wrap;
    uint8_t      verse_page_start;

    // Settings
    FontChoice  font_choice;
    bool        dark_mode;
    SettingsRow settings_sel;
    uint8_t     settings_font_sel;
    bool        settings_font_open;
    uint8_t     settings_trans_sel;
    bool        settings_trans_open;
    bool        settings_long_consumed;  // suppress repeat/short after long-OK opens a list

    // About
    uint8_t about_scroll;

    // Search keyboard
    char    search_buf[MAX_SEARCH_LEN + 1];
    uint8_t search_len;
    uint8_t kb_row;
    uint8_t kb_col;
    uint8_t kb_page;
    bool    kb_caps;
    bool    kb_long_consumed;  // true while a long-press OK is being held,
                               // suppresses the following repeat/short event
    bool    kb_back_long_consumed; // same for long-press Back in search
    uint8_t cursor_pos;    // insertion point in search_buf (0..search_len)
    uint8_t text_scroll;   // how many chars are scrolled off the left of the input field
    uint8_t cursor_blink;  // incremented each draw frame; blink on bit 3 (~4 frames on/off)

    // Menu long-OK consumed flag (prevents multi-fire into settings)
    bool    menu_long_consumed;

    // Keyword suggestions
    uint16_t kw_count;                                // how many loaded
    char    suggest[SUGGEST_MAX][KEYWORD_WORD_LEN];   // current matches
    uint8_t suggest_count;
    uint8_t suggest_sel;    // which match is displayed / will be filled
    bool    suggest_long_consumed; // suppress repeat after long-L/R on suggestion

    // Search results
    struct {
        uint8_t sec;
        uint8_t book;
        uint8_t chapter;
        uint8_t verse;
        char    ref[32];
    } hits[MAX_SEARCH_HITS];
    uint8_t hit_count;
    uint8_t hit_sel;
    uint8_t hit_scroll;

    char error_msg[64];

    // Bookmarks
    struct {
        uint8_t sec;
        uint8_t canon_book;  // index into CANON_BOOKS[]
        uint8_t chapter;     // 0-based
        uint8_t verse;       // 1-based
    } bookmarks[MAX_BOOKMARKS];
    uint8_t bm_count;
    uint8_t bm_sel;
    uint8_t bm_scroll;

    // Available sections for the current translation (subset of 0..3)
    uint8_t avail_sections[4];
    uint8_t avail_section_count;
    bool    section_lang_en;  // true when active translation uses English folder names

    // Navigation history: view to return to when pressing Back from reading
    AppView prev_view;

    // Translations detected on SD card
    char    translations[MAX_TRANSLATIONS][TRANSLATION_NAME_LEN];
    uint8_t translation_count;
    uint8_t translation_idx;   // index into translations[]
} App;

// ============================================================
// Shared function declarations (defined in luther1912.c,
// called by keyboard.c)
// ============================================================

void draw_hdr(Canvas* canvas, const char* title);
void draw_scrollbar(Canvas* canvas, App* app, uint16_t pos, uint16_t total, uint8_t vis);
void set_fg(Canvas* canvas, App* app);
void set_ui_font(Canvas* canvas, const char* str);
void do_search(App* app);
void open_reading(App* app);
void rebuild_section_list(App* app);
void rebuild_book_list(App* app);
void refresh_chapter_count(App* app);
void refresh_verse_count(App* app);
void keywords_load(App* app);
void suggestions_update(App* app);
void suggestion_fill(App* app);
void translations_scan(App* app);
