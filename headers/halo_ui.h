#ifndef HALO_UI_H
#define HALO_UI_H

#include "halo_engine.h"
#include "halo_audio.h"

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>

/* ========================================================================
   Constants  Dark Obsidian & Neon Orange Palette
   ======================================================================== */

#define UI_COLOR_BG         RGB(18, 20, 25)     /* #121419  deep obsidian charcoal */
#define UI_COLOR_PANEL      RGB(24, 27, 34)     /* #181B22  card background */
#define UI_COLOR_PANEL_HDR  RGB(32, 36, 46)     /* #20242E  ribbon header */
#define UI_COLOR_BORDER     RGB(48, 54, 68)     /* #303644  subtle dark frame */
#define UI_COLOR_ACCENT     RGB(255, 140, 25)   /* #FF8C19  radiant neon orange */
#define UI_COLOR_ACCENT_DIM RGB(140, 65, 12)    /* #8C410C  dial track background */
#define UI_COLOR_PAD_BG     RGB(22, 25, 32)     /* #161920  unselected preset pad */
#define UI_COLOR_PAD_ACTIVE RGB(255, 155, 45)   /* #FF9B2D  flash / selected orange */
#define UI_COLOR_TEXT       RGB(240, 244, 250)  /* #F0F4FA primary text */
#define HALO_TITLE_REST_ALPHA 0.85f             /* title letter resting brightness */
#define UI_COLOR_TEXT_DIM   RGB(145, 155, 170)  /* #919BAA  secondary text */
#define UI_COLOR_SCOPE_BG   RGB(10, 12, 16)     /* #0A0C10  pitch-dark scope viewport */
#define UI_COLOR_SCOPE_GRID RGB(24, 28, 36)     /* #181C24  oscilloscope grid */
#define UI_COLOR_KEY_WHITE  RGB(220, 225, 230)  /* white key unlit */
#define UI_COLOR_KEY_BLACK  RGB(40, 44, 52)     /* black key unlit */

#define UI_MARGIN           16
#define UI_MARGIN_DOUBLE    32
#define UI_CARD_GAP         8

/* Oscilloscope: samples pulled from the audio ring per paint + plotted span */
#define HALO_SCOPE_PTS      1024

/* Piano keyboard geometry ------------------------------------------------
   White-key semitone offsets within the visible span, starting at C.
   The white keys are NOT semitone-sequential: index 1 is D (semitone 2),
   not C#. The old code mapped white-key *index* directly to semitones,
   which made every click land on the wrong note. */
#define PIANO_WHITE_KEYS 15
static const int white_key_semis[PIANO_WHITE_KEYS] = {
    0, 2, 4, 5, 7, 9, 11, 12, 14, 16, 17, 19, 21, 23, 24
};

/* FL-style musical typing hints. Lower octave lives on the letter rows
   (Z X C V B N M + S D G H J), upper octave on Q W E R T Y U I with the
   number row (2 3 5 6 7) as its black keys. */
static const char* piano_white_hint(int semi) {
    switch (semi) {
        case 0:  return "Z";  case 2:  return "X";  case 4:  return "C";
        case 5:  return "V";  case 7:  return "B";  case 9:  return "N";
        case 11: return "M";  case 12: return "Q";  case 14: return "W";
        case 16: return "E";  case 17: return "R";  case 19: return "T";
        case 21: return "Y";  case 23: return "U";  case 24: return "I";
        default: return NULL;
    }
}

static const char* piano_black_hint(int semi) {
    switch (semi) {
        case 1:  return "S";  case 3:  return "D";  case 6:  return "G";
        case 8:  return "H";  case 10: return "J";  case 13: return "2";
        case 15: return "3";  case 18: return "5";  case 20: return "6";
        case 22: return "7";
        default: return NULL;
    }
}

/* A black key sits on the boundary AFTER white key i when the white key's
   semitone is C, D, F, G or A (i.e. i % 7 in {0,1,3,4,5}). */
#define PIANO_BLACK_AFTER(i) ((((i) % 7) == 0) || (((i) % 7) == 1) || \
                              (((i) % 7) == 3) || (((i) % 7) == 4) || \
                              (((i) % 7) == 5))

typedef struct {
    int left, right, top, bottom;      /* drawable key field           */
    int white_width, black_width, black_height;
    int base_note;                     /* MIDI note of the first white key */
} PianoGeom;


/* Single global patch  no presets */
extern HaloPatch g_current_patch;

typedef struct {
    char filepath[MAX_PATH];
    float buffer[HALO_MAX_SAMPLES * 2];   /* interleaved stereo L/R */
    int count;                            /* frames (one frame = L+R) */
    HWND notify_hwnd;
} ExportJob;

typedef struct {
    int id;
    const char* label;
    const char* unit;
    double* param_ptr;
    double min_val;
    double max_val;
    int is_int;
    int decimals;      /* digits shown in the value readout */
    int curve;         /* 0 = linear, 1 = log (Hz), 2 = cubic (times) */
    RECT rect;
} KnobCtrl;

/* Knob curve mapping (Task 3.1): frequency knobs feel linear in octaves,
   envelope-time knobs get fine resolution in the first half of the throw. */
static double knob_norm_to_value(const KnobCtrl* k, double norm) {
    if (norm < 0.0) norm = 0.0;
    if (norm > 1.0) norm = 1.0;
    if (k->curve == 1) {                       /* logarithmic */
        double lo = (k->min_val < 1.0) ? 1.0 : k->min_val;
        return lo * pow(k->max_val / lo, norm);
    }
    if (k->curve == 2) {                       /* cubic */
        return k->min_val + norm * norm * norm * (k->max_val - k->min_val);
    }
    return k->min_val + norm * (k->max_val - k->min_val);
}

static double knob_value_to_norm(const KnobCtrl* k, double value) {
    double norm;
    if (k->curve == 1) {                       /* logarithmic */
        double lo = (k->min_val < 1.0) ? 1.0 : k->min_val;
        double v = (value < lo) ? lo : value;
        norm = log(v / lo) / log(k->max_val / lo);
    } else if (k->curve == 2) {                /* cubic */
        double span = k->max_val - k->min_val;
        double t = (span > 1e-9) ? ((value - k->min_val) / span) : 0.0;
        if (t < 0.0) t = 0.0;
        if (t > 1.0) t = 1.0;
        norm = pow(t, 1.0 / 3.0);
    } else {
        norm = (value - k->min_val) / (k->max_val - k->min_val);
    }
    if (norm < 0.0) norm = 0.0;
    if (norm > 1.0) norm = 1.0;
    return norm;
}

static int knob_curve_for_unit(const char* unit) {
    if (unit && strcmp(unit, "Hz") == 0) return 1;   /* log frequency */
    if (unit && strcmp(unit, "s") == 0)  return 2;   /* cubic time */
    return 0;
}

typedef struct {
    /* Parameters are stored directly in g_current_patch  no presets */
    float scope_cache[HALO_SCOPE_PTS];
    int scope_cache_len;

    float title_alpha[4]; /* "halo" letter opacity */
    DWORD note_trigger_time;

    int active_knob_idx;
    int drag_start_y;
    double drag_start_val;

    int master_dragging;
    int master_drag_start_y;
    float master_drag_start_val;

    DWORD btn_flash_time[4]; /* 0=play, 1=export, 2=reset, 3=keybinds */
    int   hovered_btn;

    uint8_t key_down_state[256];

    volatile LONG export_in_progress;
    volatile LONG export_progress;
    DWORD        export_finish_time;
    char         export_filename[MAX_PATH];

    KnobCtrl knobs[32];
    int knob_count;

    /* Piano keyboard state */
    uint8_t keyboard_note_state[128];    /* 1 = pressed, 0 = released */
    int     keyboard_base_note;          /* lowest note on keyboard (e.g., C4 = 60) */
    int     keyboard_key_width;          /* computed each layout */
    RECT    keyboard_rect;               /* overall keyboard area */

    int     mouse_note;                  /* note currently held by the mouse, -1 = none */
    int     kb_octave;                   /* musical typing octave shift, -2..+2 */
    int     kb_notes[256];               /* actual note triggered per virtual key */

    int     preview_active;              /* legacy flag, preview tone disabled */

    /* Preset selector (skinned dropdown) + custom preset text files */
    int     preset_open;                 /* dropdown list visible              */
    int     preset_hover;                /* hovered row in the open dropdown   */
    int     preset_sel;                  /* selected row: 0..7 factory,
                                            8.. factory count => user presets  */
    char    preset_label[64];            /* shown on the closed selector       */
    char    user_preset_names[32][64];   /* names of *.halo.txt files          */
    char    user_preset_paths[32][MAX_PATH];
    int     user_preset_count;

    HWND hwnd_main;
    HFONT font_regular;
    HFONT font_header;
    HFONT font_title;
    HFONT font_small;
} HaloUIState;

static HaloUIState g_ui = {0};
static float g_master_volume = 0.75f;

/* Single source of truth for keyboard geometry: drawing AND hit-testing
   both derive from this, so clicks always land on the drawn key. */
static void piano_get_geometry(const RECT* area, PianoGeom* g) {
    g->left   = area->left + 8;
    g->right  = area->right - 8;
    g->top    = area->top + 4;
    g->bottom = area->bottom - 4;
    if (g->right < g->left + PIANO_WHITE_KEYS * 6) {
        g->right = g->left + PIANO_WHITE_KEYS * 6;
    }
    g->white_width  = (g->right - g->left) / PIANO_WHITE_KEYS;
    if (g->white_width < 6) g->white_width = 6;
    g->black_width  = (int)(g->white_width * 0.58);
    g->black_height = (int)((g->bottom - g->top) * 0.62);
    g->base_note    = 60 + 12 * g_ui.kb_octave;   /* C4 + octave shift */
}

/* Black keys are drawn on top, so they are hit-tested first. */
static int piano_black_note_at(const PianoGeom* g, int x, int y) {
    if (y > g->top + g->black_height) return -1;
    for (int i = 0; i < PIANO_WHITE_KEYS - 1; i++) {
        if (!PIANO_BLACK_AFTER(i)) continue;
        int bx = g->left + (i + 1) * g->white_width - g->black_width / 2;
        if (x >= bx && x < bx + g->black_width) {
            return g->base_note + white_key_semis[i] + 1;
        }
    }
    return -1;
}

/* Map a client point to the MIDI note of the key underneath it. */
static int piano_note_from_point(const RECT* area, int x, int y) {
    PianoGeom g;
    piano_get_geometry(area, &g);

    if (x < g.left || x >= g.right || y < g.top || y > g.bottom) return -1;

    int note = piano_black_note_at(&g, x, y);
    if (note >= 0) return note;

    int idx = (x - g.left) / g.white_width;
    if (idx < 0) idx = 0;
    if (idx > PIANO_WHITE_KEYS - 1) idx = PIANO_WHITE_KEYS - 1;
    return g.base_note + white_key_semis[idx];
}

/* Click depth on the key maps to velocity, like real key travel. */
static float piano_velocity_at(int y) {
    PianoGeom g;
    piano_get_geometry(&g_ui.keyboard_rect, &g);
    float span = (float)(g.bottom - g.top);
    float rel = (span > 1.0f) ? ((float)(y - g.top) / span) : 1.0f;
    if (rel < 0.0f) rel = 0.0f;
    if (rel > 1.0f) rel = 1.0f;
    return 0.65f + 0.35f * rel;
}

/* Forward Declarations */
static void ui_init_knobs(void);
static void ui_update_knob_layout(int w, int h);
static void ui_paint(HWND hwnd, HDC hdc);
static void ui_synthesize_preview(void);
static void draw_piano_keyboard(HDC hdc, RECT area);
static void ui_save_preset_dialog(HWND hwnd);
static void ui_load_preset_dialog(HWND hwnd);

/* Width of the "halo" title in the header font, shared by every consumer
   so the top-bar layout never disagrees between paint and hit-testing. */
static int halo_title_width(HWND hwnd) {
    SIZE sz = {0};
    HDC dc = GetDC(hwnd);
    if (dc) {
        HGDIOBJ old = SelectObject(dc, g_ui.font_title);
        GetTextExtentPoint32A(dc, "halo", 4, &sz);
        SelectObject(dc, old);
        ReleaseDC(hwnd, dc);
    }
    return sz.cx;
}

#define HALO_KEYBINDS_OFFSET 36        /* gap between logo and keybinds btn */
#define HALO_KEYBINDS_W      88

static void get_master_knob_rect(int w, RECT* r) {
    int knob_w = 68;
    r->left = w - 368;
    r->right = r->left + knob_w;
    r->top = 12;
    r->bottom = 38;
}

/* Top-bar geometry shared by paint, hit-testing and the dropdown popup so
   they always agree. Layout: [keybinds] [preset selector v] [save] [load] */
#define PRESET_BTN_W   64
#define PRESET_BTN_GAP 6

#define PRESET_SEL_PAD_X   14   /* horizontal breathing room inside the box  */
#define PRESET_SEL_CARET_W 18   /* right-side clearance for the dropdown caret */
#define PRESET_SEL_W_MIN   148  /* compact fallback before text is measured  */

static int preset_row_count(void);
static void preset_row_label(int row, char* out, size_t sz);
static int s_preset_sel_width = 0;   /* cached snug width; 0 = remeasure */

static void preset_selector_invalidate_width(void) {
    s_preset_sel_width = 0;
}

/* Snug auto-width: the box hugs the longest preset row label instead of a
   fixed 170px. Measured with font_regular and cached; hit-testing,
   painting and the popup all share it so they never disagree. */
static int preset_selector_width(HWND hwnd) {
    if (s_preset_sel_width) return s_preset_sel_width;

    int max_w = 0;
    HDC hdc = GetDC(hwnd);
    if (hdc) {
        HFONT old = (g_ui.font_regular) ? (HFONT)SelectObject(hdc, g_ui.font_regular) : NULL;
        char label[80];
        for (int i = 0; i < preset_row_count(); i++) {
            preset_row_label(i, label, sizeof(label));
            SIZE sz = {0};
            GetTextExtentPoint32A(hdc, label, (int)strlen(label), &sz);
            if (sz.cx > max_w) max_w = sz.cx;
        }
        if (old) SelectObject(hdc, old);
        ReleaseDC(hwnd, hdc);
    }
    s_preset_sel_width = max_w + PRESET_SEL_PAD_X * 2 + PRESET_SEL_CARET_W;
    if (s_preset_sel_width < PRESET_SEL_W_MIN) s_preset_sel_width = PRESET_SEL_W_MIN;
    return s_preset_sel_width;
}

static void keybinds_button_rect(HWND hwnd, RECT* r) {
    r->left   = UI_MARGIN + halo_title_width(hwnd) + 10 + HALO_KEYBINDS_OFFSET;
    r->right  = r->left + HALO_KEYBINDS_W;
    r->top    = 12;
    r->bottom = 38;
}

static void preset_selector_rect(HWND hwnd, RECT* r) {
    keybinds_button_rect(hwnd, r);
    r->left   = r->right + 8;          /* right of keybinds button */
    r->right  = r->left + preset_selector_width(hwnd);
}

static void preset_save_button_rect(HWND hwnd, RECT* r) {
    preset_selector_rect(hwnd, r);
    r->left   = r->right + PRESET_BTN_GAP;
    r->right  = r->left + PRESET_BTN_W;
}

static void preset_load_button_rect(HWND hwnd, RECT* r) {
    preset_save_button_rect(hwnd, r);
    r->left   = r->right + PRESET_BTN_GAP;
    r->right  = r->left + PRESET_BTN_W;
}

static HFONT create_ui_font(int height) {
    HFONT hFont = CreateFontA(
        -height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Inter"
    );
    if (!hFont) {
        hFont = CreateFontA(
            -height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI"
        );
    }
    return hFont;
}

static COLORREF blend_color(COLORREF c_bg, COLORREF c_fg, float alpha) {
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;
    int r = (int)(GetRValue(c_bg) + alpha * (GetRValue(c_fg) - GetRValue(c_bg)));
    int g = (int)(GetGValue(c_bg) + alpha * (GetGValue(c_fg) - GetGValue(c_bg)));
    int b = (int)(GetBValue(c_bg) + alpha * (GetBValue(c_fg) - GetBValue(c_bg)));
    return RGB(r, g, b);
}

static void draw_rounded_rect(HDC hdc, RECT r, COLORREF fill, COLORREF border, int radius) {
    HBRUSH br = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ old_br = SelectObject(hdc, br);
    HGDIOBJ old_pen = SelectObject(hdc, pen);
    RoundRect(hdc, r.left, r.top, r.right, r.bottom, radius, radius);
    SelectObject(hdc, old_br);
    SelectObject(hdc, old_pen);
    DeleteObject(br);
    DeleteObject(pen);
}

/* ========================================================================
   Keybinds Popup Dialog (unchanged)
   ======================================================================== */

static HWND g_keybindsHwnd = NULL;

typedef struct {
    const char* key;
    const char* desc;
} KeybindRow;

static const KeybindRow kHaloKeybinds[] = {
    { "Z X C V B N M ,", "Play lower white keys" },
    { "S D G H J",       "Play lower black keys" },
    { "Q W E R T Y U I", "Play upper white keys" },
    { "2 3 5 6 7",       "Play upper black keys" },
    { "Up / Down",       "Shift octave up / down" },
    { "Space / Enter",   "Audition chord (C4-E4-G4)" },
    { "Ctrl + E",        "Export oneshot to 32-bit WAV" },
    { "K",               "Open keybinds window" },
    { "Click + Drag",    "Adjust parameter" },
    { "Shift + Drag",    "Fine-tune parameter" },
    { "Mouse Wheel",     "Step adjust parameter (hover)" },
    { "Shift + Wheel",   "Fine-tune parameter (hover)" },
    { "Click / Slide",   "Play on-screen piano keys" },
};
#define KEYBIND_COUNT ((int)(sizeof(kHaloKeybinds) / sizeof(kHaloKeybinds[0])))

/* Measure every key/description string with the real font so the popup can
   be sized to fit its content - no more clipped text. Used by both the
   window-creation sizing and the paint routine so they always agree. */
static void keybinds_measure(int* max_key_w, int* max_desc_w, int* row_h) {
    *max_key_w  = 150;
    *max_desc_w = 320;
    *row_h      = 26;

    HDC dc = GetDC(NULL);
    if (!dc) return;

    HFONT font = g_ui.font_regular ? g_ui.font_regular
                                   : (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    HGDIOBJ old = SelectObject(dc, font);

    TEXTMETRICA tm;
    if (GetTextMetricsA(dc, &tm)) {
        *row_h = tm.tmHeight + 10;   /* vertical breathing room per row */
    }

    int mk = 0, md = 0;
    for (int i = 0; i < KEYBIND_COUNT; i++) {
        SIZE sz;
        if (GetTextExtentPoint32A(dc, kHaloKeybinds[i].key,
                                  (int)strlen(kHaloKeybinds[i].key), &sz) && sz.cx > mk) {
            mk = sz.cx;
        }
        if (GetTextExtentPoint32A(dc, kHaloKeybinds[i].desc,
                                  (int)strlen(kHaloKeybinds[i].desc), &sz) && sz.cx > md) {
            md = sz.cx;
        }
    }

    SelectObject(dc, old);
    ReleaseDC(NULL, dc);

    if (mk > 0) *max_key_w = mk;
    if (md > 0) *max_desc_w = md;
}

/* Measure left column keys and right column keys separately so 
   short keys like "K" don't push the right column off-screen */
static LRESULT CALLBACK KeybindsWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_ERASEBKGND: return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            int w = rc.right - rc.left;
            int h = rc.bottom - rc.top;

            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBmp = CreateCompatibleBitmap(hdc, w, h);
            HGDIOBJ oldBmp = SelectObject(memDC, memBmp);

            HBRUSH bg = CreateSolidBrush(UI_COLOR_BG);
            FillRect(memDC, &rc, bg);
            DeleteObject(bg);

            SetBkMode(memDC, TRANSPARENT);
            SelectObject(memDC, g_ui.font_regular ? g_ui.font_regular : GetStockObject(DEFAULT_GUI_FONT));

            int midX = w / 2;
            int startY = 16;
            int rowH = 24;
            int halfCount = (KEYBIND_COUNT + 1) / 2; /* 6 rows */
            int contentBottom = startY + halfCount * rowH;

            /* Center divider line */
            HPEN divPen = CreatePen(PS_SOLID, 1, UI_COLOR_BORDER);
            HGDIOBJ oldPen = SelectObject(memDC, divPen);
            MoveToEx(memDC, midX, startY - 2, NULL);
            LineTo(memDC, midX, contentBottom + 2);
            SelectObject(memDC, oldPen);
            DeleteObject(divPen);

            /* Generous key column so long keys fit without clipping descriptions */
            int key_col_w = 135; 

            for (int i = 0; i < KEYBIND_COUNT; ++i) {
                int isRightCol = (i >= halfCount);
                int row = isRightCol ? (i - halfCount) : i;
                int y = startY + row * rowH;

                RECT keyRc, descRc;
                if (!isRightCol) {
                    keyRc  = (RECT){ 16, y, 16 + key_col_w, y + rowH };
                    descRc = (RECT){ 16 + key_col_w + 14, y, midX - 16, y + rowH };
                } else {
                    keyRc  = (RECT){ midX + 16, y, midX + 16 + key_col_w, y + rowH };
                    descRc = (RECT){ midX + 16 + key_col_w + 14, y, w - 16, y + rowH };
                }

                SetTextColor(memDC, UI_COLOR_ACCENT);
                DrawTextA(memDC, kHaloKeybinds[i].key, -1, &keyRc,
                          DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

                SetTextColor(memDC, UI_COLOR_TEXT_DIM);
                DrawTextA(memDC, kHaloKeybinds[i].desc, -1, &descRc,
                          DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            }

            /* Dedicated bottom hint strip with plenty of breathing room */
            SetTextColor(memDC, RGB(130, 140, 155));
            SelectObject(memDC, g_ui.font_small ? g_ui.font_small : g_ui.font_regular);
            RECT hintRc = { 0, h - 30, w, h - 8 };
            DrawTextA(memDC, "Press [ESC] or [ENTER] to close", -1, &hintRc,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

            BitBlt(hdc, 0, 0, w, h, memDC, 0, 0, SRCCOPY);
            SelectObject(memDC, oldBmp);
            DeleteObject(memBmp);
            DeleteDC(memDC);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE || wParam == VK_RETURN) {
                ShowWindow(hwnd, SW_HIDE);
                return 0;
            }
            break;
        case WM_CLOSE:
            ShowWindow(hwnd, SW_HIDE);
            return 0;
        case WM_DESTROY:
            g_keybindsHwnd = NULL;
            return 0;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

static void open_keybinds_dialog(HWND parentHwnd) {
    /* Exact desired CLIENT dimensions:
       Width: 880px (plenty wide for all descriptions)
       Height: 16px (top) + 6 rows * 24px (144px) + 24px (gap) + 22px (hint) + 12px (pad) = 218px */
    RECT clientRc = { 0, 0, 880, 218 };
    DWORD dwStyle = WS_POPUPWINDOW | WS_CAPTION | WS_VISIBLE;
    DWORD dwExStyle = WS_EX_TOOLWINDOW | WS_EX_TOPMOST;

    /* Converts desired client size to the actual outer window size (accounts for title bar) */
    AdjustWindowRectEx(&clientRc, dwStyle, FALSE, dwExStyle);

    int rw = clientRc.right - clientRc.left;
    int rh = clientRc.bottom - clientRc.top;

    int scrW = GetSystemMetrics(SM_CXSCREEN);
    int scrH = GetSystemMetrics(SM_CYSCREEN);
    int rx = (scrW - rw) / 2;
    int ry = (scrH - rh) / 2;

    if (!g_keybindsHwnd) {
        static int s_kbRegistered = 0;
        if (!s_kbRegistered) {
            WNDCLASSA wc = {0};
            wc.lpfnWndProc = KeybindsWndProc;
            wc.hInstance = GetModuleHandle(NULL);
            wc.lpszClassName = "HaloKeybindsClass";
            wc.hCursor = LoadCursor(NULL, IDC_ARROW);
            RegisterClassA(&wc);
            s_kbRegistered = 1;
        }

        g_keybindsHwnd = CreateWindowExA(
            dwExStyle,
            "HaloKeybindsClass",
            "halo - keybinds & musical typing",
            dwStyle,
            rx, ry, rw, rh,
            parentHwnd, NULL, GetModuleHandle(NULL), NULL
        );
    }

    SetWindowPos(g_keybindsHwnd, HWND_TOPMOST, rx, ry, rw, rh, SWP_SHOWWINDOW);
    SetForegroundWindow(g_keybindsHwnd);
    InvalidateRect(g_keybindsHwnd, NULL, FALSE);
}

/* ========================================================================
   Preset System: factory presets + plain-text custom preset files
   Format: one "key = value" line per patch field, e.g.
       name = My Lead
       fm_depth = 1.40
   Unknown keys are ignored, missing fields keep the current patch's
   value - the file stays human-editable and forward compatible.
   ======================================================================== */

#define HALO_USER_PRESET_MAX 32

/* Field table: name -> field in HaloPatch via the X-macro below. */
#define HALO_PATCH_KEY_LIST(X) \
    X(pitch_semi) X(waveform) X(fm_ratio) X(fm_depth) X(fm_feedback) \
    X(osc_mix) X(detune) X(unison_voices) X(unison_spread) \
    X(partial_count) X(partial_tilt) X(noise_mix) X(noise_cutoff) \
    X(harm_decay) X(inharm) X(filter_cutoff) X(filter_q) X(filter_drive) \
    X(drive) X(filter_type) X(lfo_filt_depth) X(key_track) \
    X(amp_attack) X(amp_decay) X(amp_release) X(filter_env_depth) \
    X(lfo_rate) X(amp_sustain) X(vibrato)

static int preset_lookup_key(const char* key, double** out_value, HaloPatch* p) {
    #define HALO_KEY_MATCH(field) \
        if (strcmp(key, #field) == 0) { *out_value = &p->field; return 1; }
    HALO_PATCH_KEY_LIST(HALO_KEY_MATCH)
    #undef HALO_KEY_MATCH
    return 0;
}

/* The name line is stored separately (label, not a patch parameter). */
static int preset_apply_line(HaloPatch* p, const char* key, const char* val, char* name_out, size_t name_sz) {
    if (_stricmp(key, "name") == 0) {
        if (name_out) snprintf(name_out, name_sz, "%s", val);
        return 1;
    }
    double* field = NULL;
    if (preset_lookup_key(key, &field, p)) {
        *field = atof(val);
        return 1;
    }
    return 0;   /* unknown keys are ignored, not errors */
}

static void preset_save_text(const char* path, const char* name, const HaloPatch* p) {
    FILE* f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "# halo synth preset (plain text, key = value)\n");
    fprintf(f, "name = %s\n", name);
    #define HALO_KEY_WRITE(field) fprintf(f, #field " = %.4f\n", p->field);
    HALO_PATCH_KEY_LIST(HALO_KEY_WRITE)
    #undef HALO_KEY_WRITE
    fclose(f);
}

static void preset_load_text(const char* path, HaloPatch* p, char* name_out, size_t name_sz) {
    if (name_out && name_sz) name_out[0] = '\0';
    FILE* f = fopen(path, "r");
    if (!f) return;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char* hash = strchr(line, '#');
        if (hash) *hash = '\0';
        char* eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char* key = line;
        while (*key == ' ' || *key == '\t') key++;
        char* key_end = key + strlen(key);
        while (key_end > key && (key_end[-1] == ' ' || key_end[-1] == '\t' ||
                                 key_end[-1] == '\r' || key_end[-1] == '\n')) key_end--;
        *key_end = '\0';
        char* val = eq + 1;
        while (*val == ' ' || *val == '\t') val++;
        char* val_end = val + strlen(val);
        while (val_end > val && (val_end[-1] == ' ' || val_end[-1] == '\t' ||
                                 val_end[-1] == '\r' || val_end[-1] == '\n')) val_end--;
        *val_end = '\0';
        if (*key) preset_apply_line(p, key, val, name_out, name_sz);
    }
    fclose(f);
}

/* Scan the exe directory for *.halo.txt custom presets. */
static void preset_scan_user_files(void) {
    g_ui.user_preset_count = 0;
    preset_selector_invalidate_width();
    char dir[MAX_PATH];
    char pattern[MAX_PATH];
    GetModuleFileNameA(NULL, dir, MAX_PATH);
    char* slash = strrchr(dir, '\\');
    if (slash) *slash = '\0'; else strcpy(dir, ".");
    snprintf(pattern, sizeof(pattern), "%s\\*.halo.txt", dir);

    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (g_ui.user_preset_count >= HALO_USER_PRESET_MAX) break;
        char full[MAX_PATH];
        snprintf(full, sizeof(full), "%s\\%s", dir, fd.cFileName);
        int idx = g_ui.user_preset_count;
        snprintf(g_ui.user_preset_paths[idx], MAX_PATH, "%s", full);
        /* derive display name: filename minus .halo.txt */
        char name[64];
        snprintf(name, sizeof(name), "%s", fd.cFileName);
        char* dot = strstr(name, ".halo.txt");
        if (dot) *dot = '\0';
        snprintf(g_ui.user_preset_names[idx], 64, "%s", name);
        g_ui.user_preset_count++;
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}

/* Apply a row selection: 0..7 = factory, 8.. = user files. */
static void preset_apply_selection(int sel) {
    if (sel < 0) return;
    if (sel < HALO_PRESET_COUNT) {
        halo_get_preset(sel, &g_current_patch);
        snprintf(g_ui.preset_label, sizeof(g_ui.preset_label), "%s", HALO_PRESET_NAMES[sel]);
    } else {
        int u = sel - HALO_PRESET_COUNT;
        if (u >= g_ui.user_preset_count) return;
        char name[64] = "";
        preset_load_text(g_ui.user_preset_paths[u], &g_current_patch, name, sizeof(name));
        if (!name[0]) snprintf(name, sizeof(name), "%s", g_ui.user_preset_names[u]);
        snprintf(g_ui.preset_label, sizeof(g_ui.preset_label), "%s", name);
    }
    ui_init_knobs();   /* rebinds + pushes patch to audio + refreshes layout */
}

/* ========================================================================
   Skinned Preset Dropdown (custom-drawn popup list, no stock combobox)
   ======================================================================== */

static HWND g_presetPopupHwnd = NULL;
static HWND g_presetPopupOwner = NULL;

static int preset_row_count(void) {
    return HALO_PRESET_COUNT + g_ui.user_preset_count;
}

static void preset_row_label(int row, char* out, size_t sz) {
    if (row < HALO_PRESET_COUNT) {
        snprintf(out, sz, "%s", HALO_PRESET_NAMES[row]);
    } else {
        int u = row - HALO_PRESET_COUNT;
        if (u < g_ui.user_preset_count) snprintf(out, sz, "%s", g_ui.user_preset_names[u]);
        else snprintf(out, sz, "?");
    }
}

static LRESULT CALLBACK PresetPopupWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_ERASEBKGND: return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            int w = rc.right - rc.left;
            int h = rc.bottom - rc.top;

            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBmp = CreateCompatibleBitmap(hdc, w, h);
            HGDIOBJ oldBmp = SelectObject(memDC, memBmp);

            HBRUSH bg = CreateSolidBrush(RGB(13, 15, 19));
            FillRect(memDC, &rc, bg);
            DeleteObject(bg);

            SetBkMode(memDC, TRANSPARENT);
            SelectObject(memDC, g_ui.font_regular ? g_ui.font_regular : GetStockObject(DEFAULT_GUI_FONT));

            int row_h = 26;
            int rows = preset_row_count();
            for (int i = 0; i < rows; i++) {
                RECT row_r = {2, i * row_h, w - 2, (i + 1) * row_h};
                int sel = (i == g_ui.preset_sel);
                int hov = (i == g_ui.preset_hover) && !sel;
                if (sel) {
                    HBRUSH b = CreateSolidBrush(RGB(42, 46, 58));
                    FillRect(memDC, &row_r, b);
                    DeleteObject(b);
                } else if (hov) {
                    HBRUSH b = CreateSolidBrush(RGB(28, 32, 40));
                    FillRect(memDC, &row_r, b);
                    DeleteObject(b);
                }
                char label[80];
                preset_row_label(i, label, sizeof(label));
                SetTextColor(memDC, sel ? UI_COLOR_ACCENT : UI_COLOR_TEXT);
                RECT txt = {10, row_r.top, w - 10, row_r.bottom};
                DrawTextA(memDC, label, -1, &txt, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            }

            HPEN pen = CreatePen(PS_SOLID, 1, UI_COLOR_BORDER);
            HGDIOBJ oldPen = SelectObject(memDC, pen);
            RECT fr = {0, 0, w - 1, h - 1};
            SelectObject(memDC, GetStockObject(NULL_BRUSH));
            Rectangle(memDC, 0, 0, w, h);
            SelectObject(memDC, oldPen);
            DeleteObject(pen);

            BitBlt(hdc, 0, 0, w, h, memDC, 0, 0, SRCCOPY);
            SelectObject(memDC, oldBmp);
            DeleteObject(memBmp);
            DeleteDC(memDC);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_MOUSEMOVE: {
            int y = GET_Y_LPARAM(lParam);
            int row = y / 26;
            if (row >= 0 && row < preset_row_count() && row != g_ui.preset_hover) {
                g_ui.preset_hover = row;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            TrackMouseEvent(&(TRACKMOUSEEVENT){sizeof(TRACKMOUSEEVENT), TME_LEAVE, hwnd, 0});
            return 0;
        }
        case WM_MOUSELEAVE:
            if (g_ui.preset_hover != -1) {
                g_ui.preset_hover = -1;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        case WM_LBUTTONDOWN: {
            int y = GET_Y_LPARAM(lParam);
            int row = y / 26;
            if (row >= 0 && row < preset_row_count()) {
                g_ui.preset_sel = row;
                preset_apply_selection(row);
            }
            ShowWindow(hwnd, SW_HIDE);
            g_presetPopupHwnd = NULL;
            g_ui.preset_open = 0;
            InvalidateRect(g_presetPopupOwner, NULL, FALSE);
            return 0;
        }
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                ShowWindow(hwnd, SW_HIDE);
                g_presetPopupHwnd = NULL;
                InvalidateRect(g_presetPopupOwner, NULL, FALSE);
                return 0;
            }
            break;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

static void preset_close_popup(void) {
    if (g_presetPopupHwnd) {
        ShowWindow(g_presetPopupHwnd, SW_HIDE);
        g_presetPopupHwnd = NULL;
    }
    g_ui.preset_open = 0;
    g_ui.preset_hover = -1;
}

static void preset_toggle_dropdown(HWND owner) {
    if (g_presetPopupHwnd) {
        preset_close_popup();
        return;
    }
    g_presetPopupOwner = owner;

    static int s_registered = 0;
    if (!s_registered) {
        WNDCLASSA wc = {0};
        wc.lpfnWndProc = PresetPopupWndProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.lpszClassName = "HaloPresetPopupClass";
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        RegisterClassA(&wc);
        s_registered = 1;
    }

    preset_scan_user_files();   /* refresh user list on every open */

    int row_h = 26;
    int rows = preset_row_count();
    int pw = preset_selector_width(owner), ph = rows * row_h + 4;

    /* Drop below the selector button in screen coordinates, aligned with
       the same rect the paint and hit-test code use. */
    RECT sel;
    preset_selector_rect(owner, &sel);
    POINT pt = {sel.left, sel.bottom};
    ClientToScreen(owner, &pt);

    int scrH = GetSystemMetrics(SM_CYSCREEN);
    if (pt.y + ph > scrH - 40) pt.y = pt.y - ph - (sel.bottom - sel.top) - 4;

    /* Focus-free popup: WS_EX_NOACTIVATE + SW_SHOWNA means the popup never
       takes activation from the main window, so no foreground/focus fight
       can fire WM_KILLFOCUS and instantly hide it (the bug where the
       dropdown never appeared). */
    g_presetPopupHwnd = CreateWindowExA(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
        "HaloPresetPopupClass", NULL,
        WS_POPUP,
        pt.x, pt.y, pw, ph,
        owner, NULL, GetModuleHandle(NULL), NULL
    );
    ShowWindow(g_presetPopupHwnd, SW_SHOWNA);
    g_ui.preset_open = 1;
    g_ui.preset_hover = -1;
}

/* ========================================================================
   Async Background WAV Export Worker (unchanged)
   ======================================================================== */

static DWORD WINAPI async_export_worker(LPVOID param) {
    ExportJob* job = (ExportJob*)param;
    if (!job) return 0;

    FILE* f = fopen(job->filepath, "wb");
    if (!f) {
        InterlockedExchange(&g_ui.export_in_progress, 0);
        free(job);
        return 0;
    }

    int32_t sr = HALO_SR;
    int32_t frames = job->count;
    int32_t data_size = frames * 2 * (int32_t)sizeof(float);   /* stereo L/R */
    int32_t chunk_size = 36 + data_size;
    int16_t num_channels = 2;
    int16_t bits = 32;
    int16_t block_align = num_channels * (bits / 8);
    int16_t audio_format = 3; /* WAVE_FORMAT_IEEE_FLOAT */
    int32_t byte_rate = sr * block_align;
    int32_t subchunk_size = 16;

    fwrite("RIFF", 1, 4, f);
    fwrite(&chunk_size, 4, 1, f);
    fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f);
    fwrite(&subchunk_size, 4, 1, f);
    fwrite(&audio_format, 2, 1, f);
    fwrite(&num_channels, 2, 1, f);
    fwrite(&sr, 4, 1, f);
    fwrite(&byte_rate, 4, 1, f);
    fwrite(&block_align, 2, 1, f);
    fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f);
    fwrite(&data_size, 4, 1, f);

    int chunk_samples = 2048;
    int offset = 0;
    while (offset < frames) {
        int to_write = frames - offset;
        if (to_write > chunk_samples) to_write = chunk_samples;
        fwrite(&job->buffer[offset * 2], sizeof(float), to_write * 2, f);
        offset += to_write;
        InterlockedExchange(&g_ui.export_progress, (LONG)((int64_t)offset * 100 / frames));
        Sleep(2);
    }

    fclose(f);
    InterlockedExchange(&g_ui.export_progress, 100);
    InterlockedExchange(&g_ui.export_in_progress, 0);
    g_ui.export_finish_time = GetTickCount();

    if (job->notify_hwnd) {
        InvalidateRect(job->notify_hwnd, NULL, FALSE);
    }

    free(job);
    return 0;
}

/* ========================================================================
   Custom Visuals: Animated Neon Halo Logo
   ======================================================================== */

/* Matches the app icon - a bold orange outer ring
   with a bright inner halo ring on the dark background. The inner ring gets
   a very slight rotating wobble and the whole mark breathes with the
   engine's output level (audio_get_level). Rendered per-pixel on a 4x
   supersampled DIB so the edges stay smooth.

   NOTE on pixel packing: 32-bit DIB sections store bytes as B,G,R,X, so a
   pixel read back as a DWORD holds red in bits 16-23 and blue in bits 0-7.
   The old code had this flipped, which painted the orange rings blue. */

/* renders a single radiant neon-orange halo inclined in 
   3D space with continuous rotational spin, perspective parallax depth, and 
   audio responsiveness (amplitude bloom, breathing, and polyphony excitation). */

static void draw_neon_halo_logo(HDC hdc, int x, int y, int size) {
    int ss = 4;
    int ss_size = size * ss;

    HDC memDC = CreateCompatibleDC(hdc);
    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = ss_size;
    bmi.bmiHeader.biHeight = -ss_size; /* Top-down DIB */
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pBits = NULL;
    HBITMAP hBmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);
    if (!hBmp || !memDC || !pBits) {
        if (memDC) DeleteDC(memDC);
        return;
    }
    HGDIOBJ oldBmp = SelectObject(memDC, hBmp);

    /* Clean obsidian background fill */
    RECT bg_rc = {0, 0, ss_size, ss_size};
    HBRUSH bg_br = CreateSolidBrush(UI_COLOR_BG);
    FillRect(memDC, &bg_rc, bg_br);
    DeleteObject(bg_br);

    /* Audio dynamics */
    float level = 0.0f;
    int voices = 0;
#ifdef HALO_ENGINE_H
    level = audio_get_level();
    voices = audio_get_active_voices();
#endif
    if (level < 0.0f) level = 0.0f;
    if (level > 1.0f) level = 1.0f;

    double t = GetTickCount() * 0.001;

    /* Frame delta smoothing for silky 120 FPS animations */
    static double s_last_t = 0.0;
    double dt = (s_last_t > 0.0) ? (t - s_last_t) : 0.016;
    if (dt < 0.001) dt = 0.001;
    if (dt > 0.1)   dt = 0.1;
    s_last_t = t;

    /* 1. Organic Orbital Center Sway */
    static double s_drift_x = 0.0, s_drift_y = 0.0;
    const double drift_ax = 0.024, drift_ay = 0.018;
    double target_dx = sin(t * 1.1) * drift_ax + cos(t * 2.3) * drift_ax * 0.35;
    double target_dy = cos(t * 0.9) * drift_ay + sin(t * 1.7) * drift_ay * 0.40;
    double smooth = 1.0 - exp(-8.0 * dt);
    s_drift_x += (target_dx - s_drift_x) * smooth;
    s_drift_y += (target_dy - s_drift_y) * smooth;

    double cx = ss_size * 0.5 + s_drift_x * ss_size;
    double cy = ss_size * 0.5 + s_drift_y * ss_size;
    double scale_r = ss_size * 0.46; /* Normalized coordinate unit scale */

    /* 2. 3D Orientation & Parallax Nutation */
    /* Pitch angles: forward tilt so top recedes (+Z) and bottom advances (-Z) */
    double pitch = 1.02 + 0.08 * sin(t * 1.3) - 0.12 * (double)level;
    double yaw   = 0.18 * sin(t * 0.85) + 0.05 * cos(t * 2.1);
    double roll  = 0.10 * cos(t * 1.05);

    /* 3. Spin Velocity Acceleration (boosted by audio output & active voices) */
    static double s_spin_phase = 0.0;
    double spin_speed = 2.4 + 1.6 * (double)level;
    if (voices > 0) spin_speed += 0.8;
    s_spin_phase += spin_speed * dt;

    /* Precalculate spin harmonics for loop efficiency */
    double cos_spin = cos(s_spin_phase);
    double sin_spin = sin(s_spin_phase);

    /* Camera distance for 3D perspective projection */
    const double D = 2.4;

    /* 3D Ring Radius with Audio Breath and Polyphony Excitation */
    double R = 0.62 + 0.038 * (double)level + 0.010 * sin(t * 2.2);
    if (voices > 0) R += 0.012 * sin(t * 9.0);

    /* 3D Orthonormal Basis: Columns of R = R_yaw * R_pitch * R_roll */
    double cp = cos(pitch), sp = sin(pitch);
    double cy_ = cos(yaw),  sy_ = sin(yaw);
    double cr = cos(roll),  sr = sin(roll);

    double Ux = cy_ * cr + sy_ * sp * sr;
    double Uy = cp * sr;
    double Uz = -sy_ * cr + cy_ * sp * sr;

    double Vx = -cy_ * sr + sy_ * sp * cr;
    double Vy = cp * cr;
    double Vz = sy_ * sr + cy_ * sp * cr;

    double Nx = sy_ * cp;
    double Ny = -sp;
    double Nz = cy_ * cp;

    /* Slight 3D vertical centering compensation for perspective foreshortening */
    double y_center_offset = 0.06;

    /* Physical Dimensions of the Single Halo Tube in 3D */
    double r_tube = 0.040 + 0.014 * (double)level; /* Hot solid neon core */
    double r_glow = 0.110 + 0.045 * (double)level; /* Surrounding neon aura */

    /* Baseline resting opacity (~0.76), blooming up to 1.0 on audio transients */
    double base_alpha = 0.76 + 0.24 * (double)level;

    /* Background color components (#121419) */
    const int bg_r = (UI_COLOR_BG) & 0xFF;
    const int bg_g = (UI_COLOR_BG >> 8) & 0xFF;
    const int bg_b = (UI_COLOR_BG >> 16) & 0xFF;

    unsigned int* px = (unsigned int*)pBits;

    /* Fast analytical 3D ray-plane perspective rasterizer */
    for (int py = 0; py < ss_size; py++) {
        /* Cartesian normalized coordinates (y positive upward) */
        double y_s = -((double)py + 0.5 - cy) / scale_r;

        for (int pxx = 0; pxx < ss_size; pxx++) {
            double x_s = ((double)pxx + 0.5 - cx) / scale_r;

            /* Ray-plane intersection: ray from (0,0,-D) through (x_s, y_s, 0) */
            double denom = x_s * Nx + y_s * Ny + D * Nz;
            if (denom <= 0.04) continue;

            double s = (D * Nz + y_center_offset * Ny) / denom;
            if (s <= 0.1 || s > 5.0) continue;

            /* 3D hit point on the tilted ring plane */
            double Qx = s * x_s;
            double Qy = s * y_s - y_center_offset;
            double Qz = D * (s - 1.0);

            /* In-plane 2D coordinates relative to halo center */
            double u = Qx * Ux + Qy * Uy + Qz * Uz;
            double v = Qx * Vx + Qy * Vy + Qz * Vz;

            double r2 = u * u + v * v;
            double r = sqrt(r2 + 1e-12);
            double delta_r = fabs(r - R);

            /* 3D Line-of-sight foreshortening factor */
            double inv_r = 1.0 / r;
            double ur = u * inv_r;
            double vr = v * inv_r;
            double rad_x = ur * Ux + vr * Vx;
            double rad_y = ur * Uy + vr * Vy;
            double rad_z = ur * Uz + vr * Vz;

            double ray_len = sqrt(x_s * x_s + y_s * y_s + D * D);
            double rad_dot_ray = (rad_x * x_s + rad_y * y_s + rad_z * D) / ray_len;
            double f_rad2 = 1.0 - rad_dot_ray * rad_dot_ray;
            double f_rad = (f_rad2 > 0.08) ? sqrt(f_rad2) : 0.28;

            /* Perpendicular cross-sectional distance to the 3D ring wire */
            double d_3d = delta_r * f_rad;
            if (d_3d >= r_glow) continue;

            /* 3D Perspective Parallax Factor:
               Near side (Qz < 0) blooms and thickens; far side (Qz > 0) gently recedes */
            double z_norm = -Qz / (R * 1.15);
            double depth_weight = 0.58 + 0.42 * (z_norm / sqrt(1.0 + z_norm * z_norm));

            /* Hot neon core & radial ambient plasma glow */
            double core = 0.0;
            if (d_3d < r_tube) {
                double c_t = 1.0 - (d_3d / r_tube);
                core = c_t * c_t * (3.0 - 2.0 * c_t); /* Smoothstep inner core */
            }
            double glow = exp(-d_3d / (r_glow * 0.42));

            /* Continuous 3D Orbital Energy Sheen / Specular rotation sweep */
            double cos_psi = ur * cos_spin + vr * sin_spin;
            double cos_2psi = 2.0 * cos_psi * cos_psi - 1.0;
            double spin_sheen = 0.84 + 0.16 * cos_psi + 0.06 * cos_2psi;

            /* Composite final pixel opacity */
            double total_a = (core * 0.94 + glow * 0.58) * spin_sheen * depth_weight * base_alpha;
            if (total_a > 1.0) total_a = 1.0;
            if (total_a <= 0.005) continue;

            /* Luminous Neon Palette:
               Outer Aura: Radiant Orange (#FF7814)
               Core: Warm Amber (#FFA824)
               Specular/Apex: Incandescent Gold-White (#FFE698) */
            double r_col = 255.0;
            double g_col = 120.0 + 48.0 * core;
            double b_col = 20.0  + 20.0 * core;

            /* Near-apex hot specular flare with audio excitation */
            double flare = core * core * (0.35 * depth_weight + 0.50 * (double)level);
            g_col += (230.0 - g_col) * flare;
            b_col += (152.0 - b_col) * flare;

            /* Blend with obsidian background */
            unsigned int* p = &px[py * ss_size + pxx];
            int r8 = (int)(bg_r + (r_col - bg_r) * total_a);
            int g8 = (int)(bg_g + (g_col - bg_g) * total_a);
            int b8 = (int)(bg_b + (b_col - bg_b) * total_a);

            if (r8 > 255) r8 = 255;
            if (g8 > 255) g8 = 255;
            if (b8 > 255) b8 = 255;

            /* 32-bit DIB section pixel format (0xFFRRGGBB) */
            *p = 0xFF000000u | ((unsigned)r8 << 16) | ((unsigned)g8 << 8) | (unsigned)b8;
        }
    }

    SetStretchBltMode(hdc, HALFTONE);
    SetBrushOrgEx(hdc, 0, 0, NULL);
    StretchBlt(hdc, x, y, size, size, memDC, 0, 0, ss_size, ss_size, SRCCOPY);

    SelectObject(memDC, oldBmp);
    DeleteObject(hBmp);
    DeleteDC(memDC);
}

/* ========================================================================
   Knob Drawing (unchanged, uses g_current_patch)
   ======================================================================== */

static void draw_knob(HDC hdc, KnobCtrl* k, int is_active) {
    int kw = k->rect.right - k->rect.left;
    int kh = k->rect.bottom - k->rect.top;
    if (kw <= 0 || kh <= 0) return;

    /* Compact 72px cell: label 0-16, dial 16-58, value 54-72 */
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, UI_COLOR_TEXT);
    SelectObject(hdc, g_ui.font_regular);
    RECT lr = {k->rect.left, k->rect.top, k->rect.right, k->rect.top + 16};
    DrawTextA(hdc, k->label, -1, &lr, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    int cx = (k->rect.left + k->rect.right) / 2;
    int cy = k->rect.top + 38;
    int r = 16;
    int dial_box = 40;

    double norm = knob_value_to_norm(k, *k->param_ptr);

    int ss = 3;
    int ss_dim = dial_box * ss;
    int ss_cx = ss_dim / 2;
    int ss_cy = ss_dim / 2;
    int ss_r = r * ss;

    HDC memDC = CreateCompatibleDC(hdc);
    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = ss_dim;
    bmi.bmiHeader.biHeight = -ss_dim;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pBits = NULL;
    HBITMAP hBmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);
    if (hBmp && memDC) {
        HGDIOBJ oldBmp = SelectObject(memDC, hBmp);

        RECT bg_rc = {0, 0, ss_dim, ss_dim};
        HBRUSH bg_br = CreateSolidBrush(UI_COLOR_PANEL);
        FillRect(memDC, &bg_rc, bg_br);
        DeleteObject(bg_br);

        HBRUSH track_br = CreateSolidBrush(RGB(18, 20, 26));
        HPEN track_pen = CreatePen(PS_SOLID, 1 * ss, UI_COLOR_BORDER);
        HGDIOBJ old_br = SelectObject(memDC, track_br);
        HGDIOBJ old_pen = SelectObject(memDC, track_pen);
        Ellipse(memDC, ss_cx - ss_r, ss_cy - ss_r, ss_cx + ss_r, ss_cy + ss_r);
        SelectObject(memDC, old_br);
        SelectObject(memDC, old_pen);
        DeleteObject(track_br);
        DeleteObject(track_pen);

        double start_ang = 3.92699;   /* 225 deg */
        double sweep_total = 4.71239; /* 270 deg sweep */
        double cur_ang = start_ang - norm * sweep_total;

        COLORREF arc_color = is_active ? RGB(255, 175, 45) : UI_COLOR_ACCENT;
        HPEN arc_pen = CreatePen(PS_SOLID, 3 * ss, arc_color);
        old_pen = SelectObject(memDC, arc_pen);
        for (double a = start_ang; a >= cur_ang; a -= 0.05) {
            int ax = ss_cx + (int)((ss_r - 2 * ss) * cos(a));
            int ay = ss_cy - (int)((ss_r - 2 * ss) * sin(a));
            if (a == start_ang) MoveToEx(memDC, ax, ay, NULL);
            else LineTo(memDC, ax, ay);
        }
        SelectObject(memDC, old_pen);
        DeleteObject(arc_pen);

        COLORREF cap_fill = is_active ? RGB(42, 46, 58) : RGB(30, 34, 44);
        COLORREF cap_border = is_active ? UI_COLOR_ACCENT : UI_COLOR_BORDER;
        HBRUSH cap_br = CreateSolidBrush(cap_fill);
        HPEN cap_pen = CreatePen(PS_SOLID, 1 * ss, cap_border);
        old_br = SelectObject(memDC, cap_br);
        old_pen = SelectObject(memDC, cap_pen);
        int cap_r = ss_r - 5 * ss;
        Ellipse(memDC, ss_cx - cap_r, ss_cy - cap_r, ss_cx + cap_r, ss_cy + cap_r);
        SelectObject(memDC, old_br);
        SelectObject(memDC, old_pen);
        DeleteObject(cap_br);
        DeleteObject(cap_pen);

        HPEN needle_pen = CreatePen(PS_SOLID, 2 * ss, RGB(255, 245, 235));
        old_pen = SelectObject(memDC, needle_pen);
        MoveToEx(memDC, ss_cx, ss_cy, NULL);
        LineTo(memDC, ss_cx + (int)((ss_r - 6 * ss) * cos(cur_ang)),
                      ss_cy - (int)((ss_r - 6 * ss) * sin(cur_ang)));
        SelectObject(memDC, old_pen);
        DeleteObject(needle_pen);

        SetStretchBltMode(hdc, HALFTONE);
        SetBrushOrgEx(hdc, 0, 0, NULL);
        StretchBlt(hdc, cx - dial_box / 2, cy - dial_box / 2, dial_box, dial_box,
                   memDC, 0, 0, ss_dim, ss_dim, SRCCOPY);

        SelectObject(memDC, oldBmp);
        DeleteObject(hBmp);
        DeleteDC(memDC);
    }

    char vbuf[32];
    if (k->is_int) {
        snprintf(vbuf, sizeof(vbuf), "%d %s", (int)(*k->param_ptr + 0.5), k->unit);
    } else if (k->unit && strcmp(k->unit, "%") == 0) {
        /* 0..1 params with a "%" unit read as percentages, not raw fractions */
        snprintf(vbuf, sizeof(vbuf), "%.0f%%", *k->param_ptr * 100.0);
    } else {
        snprintf(vbuf, sizeof(vbuf), "%.*f %s", k->decimals, *k->param_ptr, k->unit);
    }

    SetTextColor(hdc, is_active ? UI_COLOR_ACCENT : UI_COLOR_TEXT);
    SelectObject(hdc, g_ui.font_small ? g_ui.font_small : g_ui.font_regular);
    RECT vr = {k->rect.left, k->rect.top + 54, k->rect.right, k->rect.top + 72};
    DrawTextA(hdc, vbuf, -1, &vr, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

/* ========================================================================
   Master Volume Knob (fixed interactions)
   ======================================================================== */

static void draw_master_volume_knob(HDC hdc, int w) {
    RECT r;
    get_master_knob_rect(w, &r);

    int radius = 10;
    int dial_box = 24;
    int cx = r.left + 14;
    int cy_center = (r.top + r.bottom) / 2;

    int ss = 3;
    int ss_dim = dial_box * ss;
    int ss_cx = ss_dim / 2;
    int ss_cy = ss_dim / 2;
    int ss_r = radius * ss;

    HDC memDC = CreateCompatibleDC(hdc);
    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = ss_dim;
    bmi.bmiHeader.biHeight = -ss_dim;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pBits = NULL;
    HBITMAP hBmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);
    if (hBmp && memDC) {
        HGDIOBJ oldBmp = SelectObject(memDC, hBmp);

        RECT bg_rc = {0, 0, ss_dim, ss_dim};
        HBRUSH bg_br = CreateSolidBrush(UI_COLOR_BG);
        FillRect(memDC, &bg_rc, bg_br);
        DeleteObject(bg_br);

        HBRUSH track_br = CreateSolidBrush(UI_COLOR_PANEL);
        HPEN track_pen = CreatePen(PS_SOLID, 1 * ss, UI_COLOR_BORDER);
        HGDIOBJ old_br = SelectObject(memDC, track_br);
        HGDIOBJ old_pen = SelectObject(memDC, track_pen);
        Ellipse(memDC, ss_cx - ss_r, ss_cy - ss_r, ss_cx + ss_r, ss_cy + ss_r);
        SelectObject(memDC, old_br);
        SelectObject(memDC, old_pen);
        DeleteObject(track_br);
        DeleteObject(track_pen);

        float norm = g_master_volume;
        if (norm < 0.0f) norm = 0.0f;
        if (norm > 1.0f) norm = 1.0f;
        double start_ang = 3.92699;
        double sweep_total = 4.71239;
        double cur_ang = start_ang - norm * sweep_total;

        HPEN arc_pen = CreatePen(PS_SOLID, 2 * ss, UI_COLOR_ACCENT);
        old_pen = SelectObject(memDC, arc_pen);
        for (double a = start_ang; a >= cur_ang; a -= 0.04) {
            int ax = ss_cx + (int)((ss_r - 2 * ss) * cos(a));
            int ay = ss_cy - (int)((ss_r - 2 * ss) * sin(a));
            if (a == start_ang) MoveToEx(memDC, ax, ay, NULL);
            else LineTo(memDC, ax, ay);
        }
        SelectObject(memDC, old_pen);
        DeleteObject(arc_pen);

        HBRUSH cap_br = CreateSolidBrush(RGB(30, 34, 44));
        HPEN cap_pen = CreatePen(PS_SOLID, 1 * ss, UI_COLOR_BORDER);
        old_br = SelectObject(memDC, cap_br);
        old_pen = SelectObject(memDC, cap_pen);
        int cap_r = ss_r - 4 * ss;
        Ellipse(memDC, ss_cx - cap_r, ss_cy - cap_r, ss_cx + cap_r, ss_cy + cap_r);
        SelectObject(memDC, old_br);
        SelectObject(memDC, old_pen);
        DeleteObject(cap_br);
        DeleteObject(cap_pen);

        HPEN needle_pen = CreatePen(PS_SOLID, 2 * ss, RGB(255, 245, 235));
        old_pen = SelectObject(memDC, needle_pen);
        MoveToEx(memDC, ss_cx, ss_cy, NULL);
        LineTo(memDC, ss_cx + (int)((ss_r - 5 * ss) * cos(cur_ang)),
                      ss_cy - (int)((ss_r - 5 * ss) * sin(cur_ang)));
        SelectObject(memDC, old_pen);
        DeleteObject(needle_pen);

        SetStretchBltMode(hdc, HALFTONE);
        SetBrushOrgEx(hdc, 0, 0, NULL);
        StretchBlt(hdc, cx - dial_box / 2, cy_center - dial_box / 2, dial_box, dial_box,
                   memDC, 0, 0, ss_dim, ss_dim, SRCCOPY);

        SelectObject(memDC, oldBmp);
        DeleteObject(hBmp);
        DeleteDC(memDC);
    }

    char vbuf[16];
    snprintf(vbuf, sizeof(vbuf), "%.0f%%", g_master_volume * 100.0f);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, UI_COLOR_TEXT_DIM);
    SelectObject(hdc, g_ui.font_small ? g_ui.font_small : g_ui.font_regular);
    RECT vr = {r.left + 30, r.top, r.right, r.bottom};
    DrawTextA(hdc, vbuf, -1, &vr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

/* ========================================================================
   Oscilloscope (unchanged)
   ======================================================================== */

static void draw_oscilloscope(HDC hdc, RECT r) {
    draw_rounded_rect(hdc, r, UI_COLOR_SCOPE_BG, UI_COLOR_BORDER, 6);

    HPEN grid_pen = CreatePen(PS_SOLID, 1, UI_COLOR_SCOPE_GRID);
    HGDIOBJ old_pen = SelectObject(hdc, grid_pen);

    int scope_header_h = 24;
    int wave_area_top = r.top + scope_header_h;
    int wave_area_bottom = r.bottom - 6;
    int mid_y = (wave_area_top + wave_area_bottom) / 2;

    MoveToEx(hdc, r.left + 8, mid_y, NULL);
    LineTo(hdc, r.right - 8, mid_y);

    int w = r.right - r.left - 16;
    for (int gx = r.left + 8; gx < r.right - 8; gx += w / 6) {
        MoveToEx(hdc, gx, wave_area_top + 4, NULL);
        LineTo(hdc, gx, wave_area_bottom - 2);
    }
    SelectObject(hdc, old_pen);
    DeleteObject(grid_pen);

    audio_get_scope_samples(g_ui.scope_cache, HALO_SCOPE_PTS);

    if (w > 0) {
        HPEN wave_pen = CreatePen(PS_SOLID | PS_JOIN_ROUND | PS_ENDCAP_ROUND, 2, UI_COLOR_ACCENT);
        old_pen = SelectObject(hdc, wave_pen);

        int max_amp = (wave_area_bottom - wave_area_top) / 2 - 4;
        if (max_amp < 6) max_amp = 6;

        /* Rising-edge trigger with hysteresis: arm only after the signal
           dips below a negative threshold, so noisy or multi-crossing
           waveforms can't retrigger and smear the trace. The search window
           stops before the plotted span so the trace length is stable. */
        int trig = 0;
        int armed = 0;
        for (int i = 0; i < HALO_SCOPE_PTS - 720; i++) {
            float s = g_ui.scope_cache[i];
            if (!armed) {
                if (s < -0.015f) armed = 1;
                continue;
            }
            if (s >= 0.0f) { trig = i; break; }
        }

        int n_pts = 720;
        if (trig + n_pts > HALO_SCOPE_PTS) n_pts = HALO_SCOPE_PTS - trig;

        POINT pts[720];
        for (int i = 0; i < n_pts; i++) {
            pts[i].x = r.left + 8 + (int)((int64_t)i * w / (n_pts > 1 ? (n_pts - 1) : 1));
            pts[i].y = mid_y - (int)(g_ui.scope_cache[trig + i] * max_amp);
        }
        Polyline(hdc, pts, n_pts);

        SelectObject(hdc, old_pen);
        DeleteObject(wave_pen);
    }

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, UI_COLOR_TEXT_DIM);
    SelectObject(hdc, g_ui.font_regular);

    char meta[64];
    snprintf(meta, sizeof(meta), "Active Polyphony: %d / %d Voices",
             audio_get_active_voices(), HALO_MAX_VOICES);

    RECT tr = {r.left + 12, r.top, r.right - 12, r.top + scope_header_h};
    DrawTextA(hdc, meta, -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

/* ========================================================================
   Piano Keyboard Drawing (correct note layout + shaded key look)
   ======================================================================== */

static void draw_piano_keyboard(HDC hdc, RECT area) {
    PianoGeom g;
    piano_get_geometry(&area, &g);

    /* Keep hit-testing state in sync with what is on screen. */
    g_ui.keyboard_rect = area;
    g_ui.keyboard_key_width = g.white_width;
    g_ui.keyboard_base_note = g.base_note;

    /* Keyboard body / frame behind the keys */
    RECT body = {area.left + 2, area.top + 2, area.right - 2, area.bottom - 2};
    draw_rounded_rect(hdc, body, RGB(13, 15, 19), UI_COLOR_BORDER, 8);

    SetBkMode(hdc, TRANSPARENT);
    SelectObject(hdc, g_ui.font_small ? g_ui.font_small : g_ui.font_regular);

    /* ---- White keys (C major layout, rounded fronts, depth shading) ---- */
    for (int i = 0; i < PIANO_WHITE_KEYS; i++) {
        int x = g.left + i * g.white_width;
        RECT key_rect = { x, g.top, x + g.white_width - 1, g.bottom };
        int note = g.base_note + white_key_semis[i];
        int pressed = g_ui.keyboard_note_state[note];

        COLORREF fill   = pressed ? UI_COLOR_PAD_ACTIVE : UI_COLOR_KEY_WHITE;
        COLORREF border = pressed ? RGB(255, 190, 90) : RGB(178, 184, 192);

        HBRUSH br = CreateSolidBrush(fill);
        HPEN pen = CreatePen(PS_SOLID, 1, border);
        HGDIOBJ old_br = SelectObject(hdc, br);
        HGDIOBJ old_pen = SelectObject(hdc, pen);
        RoundRect(hdc, key_rect.left, key_rect.top, key_rect.right, key_rect.bottom, 4, 4);
        SelectObject(hdc, old_br);
        SelectObject(hdc, old_pen);
        DeleteObject(br);
        DeleteObject(pen);

        /* Front-edge shading strip (fake 3D key depth) */
        RECT front = { x + 1, g.bottom - 6, x + g.white_width - 2, g.bottom - 1 };
        HBRUSH fbr = CreateSolidBrush(pressed ? RGB(255, 175, 55) : RGB(194, 200, 208));
        FillRect(hdc, &front, fbr);
        DeleteObject(fbr);

        /* Note name on every white key (octave number only on the Cs to
           avoid crowding at 15-key width). */
        {
            static const char* names[12] = { "C", "C#", "D", "D#", "E", "F",
                                             "F#", "G", "G#", "A", "A#", "B" };
            int semi = white_key_semis[i] % 12;
            char nb[6];
            if (semi == 0) snprintf(nb, sizeof(nb), "C%d", (note / 12) - 1);
            else           snprintf(nb, sizeof(nb), "%s", names[semi]);
            SetTextColor(hdc, pressed ? RGB(10, 14, 18) : RGB(139, 148, 158));
            RECT nr = { x, g.bottom - 22, x + g.white_width - 1, g.bottom - 8 };
            DrawTextA(hdc, nb, -1, &nr, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        }

        /* Musical typing hint letter for this key, if it has one */
        {
            const char* hint = piano_white_hint(white_key_semis[i]);
            if (hint) {
                SetTextColor(hdc, pressed ? RGB(10, 14, 18) : RGB(120, 130, 142));
                RECT lr = { x, g.bottom - 38, x + g.white_width - 1, g.bottom - 24 };
                DrawTextA(hdc, hint, -1, &lr, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            }
        }
    }

    /* ---- Black keys (on top, beveled face, QWERTY hints W E T Y U) ---- */
    for (int i = 0; i < PIANO_WHITE_KEYS - 1; i++) {
        if (!PIANO_BLACK_AFTER(i)) continue;

        int bx = g.left + (i + 1) * g.white_width - g.black_width / 2;
        int note = g.base_note + white_key_semis[i] + 1;
        int pressed = g_ui.keyboard_note_state[note];

        COLORREF fill   = pressed ? RGB(255, 155, 45) : RGB(33, 37, 45);
        COLORREF border = pressed ? RGB(255, 205, 120) : RGB(8, 10, 14);

        HBRUSH br = CreateSolidBrush(fill);
        HPEN pen = CreatePen(PS_SOLID, 1, border);
        HGDIOBJ old_br = SelectObject(hdc, br);
        HGDIOBJ old_pen = SelectObject(hdc, pen);
        RoundRect(hdc, bx, g.top, bx + g.black_width, g.top + g.black_height, 3, 3);
        SelectObject(hdc, old_br);
        SelectObject(hdc, old_pen);
        DeleteObject(br);
        DeleteObject(pen);

        /* Bevel highlight on the lower face of the black key */
        RECT bevel = { bx + 2, g.top + g.black_height - 8, bx + g.black_width - 2, g.top + g.black_height - 2 };
        HBRUSH bbr = CreateSolidBrush(pressed ? RGB(255, 195, 100) : RGB(56, 62, 74));
        FillRect(hdc, &bevel, bbr);
        DeleteObject(bbr);

        /* Musical typing hint letter for this black key, if mapped */
        {
            const char* hint = piano_black_hint(white_key_semis[i] + 1);
            if (hint) {
                SetTextColor(hdc, pressed ? RGB(12, 16, 20) : RGB(168, 176, 188));
                RECT lr = { bx, g.top + g.black_height - 24, bx + g.black_width, g.top + g.black_height - 8 };
                DrawTextA(hdc, hint, -1, &lr, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            }
        }
    }
}

/* ========================================================================
   Main Painting
   ======================================================================== */

static void ui_paint(HWND hwnd, HDC hdc) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;

    ui_update_knob_layout(w, h);

    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, w, h);
    HGDIOBJ oldBmp = SelectObject(memDC, memBmp);

    HBRUSH bg_br = CreateSolidBrush(UI_COLOR_BG);
    FillRect(memDC, &rc, bg_br);
    DeleteObject(bg_br);

    SetBkMode(memDC, TRANSPARENT);
    DWORD now = GetTickCount();

    /* Frame dt for the animated title/ring easing (pacer repaints at 120fps). */
    static DWORD s_last_paint_tick = 0;
    double dt = 0.016;
    if (s_last_paint_tick != 0) {
        dt = (double)(now - s_last_paint_tick) / 1000.0;
        if (dt > 0.1) dt = 0.1;
        if (dt < 0.001) dt = 0.001;
    }
    s_last_paint_tick = now;

    /* 1. Header with Luminous Title & Animated Halo Logo */
    SelectObject(memDC, g_ui.font_title);
    const char title_str[] = "halo";
    int cur_x = UI_MARGIN;

    for (int i = 0; i < 4; i++) {
        SIZE ch_sz = {0};
        GetTextExtentPoint32A(memDC, &title_str[i], 1, &ch_sz);

        /* Ease each letter back to its resting brightness (~0.3s glow). */
        float a = g_ui.title_alpha[i];
        if (a <= 0.0f) a = 1.0f;
        a += (HALO_TITLE_REST_ALPHA - a) * (1.0f - expf(-12.0f * (float)dt));
        g_ui.title_alpha[i] = a;
        if (a > 1.0f) a = 1.0f;

        COLORREF letter_col = blend_color(UI_COLOR_BG, UI_COLOR_TEXT, a);
        SetTextColor(memDC, letter_col);

        RECT ch_rc = {cur_x, 12, cur_x + ch_sz.cx, 38};
        DrawTextA(memDC, &title_str[i], 1, &ch_rc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        cur_x += ch_sz.cx;
    }

    SIZE total_sz = {0};
    GetTextExtentPoint32A(memDC, title_str, 4, &total_sz);
    int logo_x = UI_MARGIN + total_sz.cx + 10;
    draw_neon_halo_logo(memDC, logo_x, 12, 26);

    /* 2. Top Action Buttons */
    RECT kb_btn;
    keybinds_button_rect(hwnd, &kb_btn);
    int kb_flash = (now - g_ui.btn_flash_time[3] < 120);
    int kb_hover = (g_ui.hovered_btn == 4);
    COLORREF kb_fill = kb_flash ? UI_COLOR_PAD_ACTIVE : (kb_hover ? RGB(38, 42, 54) : UI_COLOR_PANEL);
    COLORREF kb_bdr  = (kb_flash || kb_hover) ? UI_COLOR_ACCENT : UI_COLOR_BORDER;
    COLORREF kb_txt  = kb_flash ? RGB(10, 14, 18) : (kb_hover ? UI_COLOR_ACCENT : UI_COLOR_TEXT);
    draw_rounded_rect(memDC, kb_btn, kb_fill, kb_bdr, 4);
    SetTextColor(memDC, kb_txt);
    SelectObject(memDC, g_ui.font_regular);
    DrawTextA(memDC, "[keybinds]", -1, &kb_btn, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    /* Preset selector (skinned dropdown): shows current preset name. */
    RECT sel_btn;
    preset_selector_rect(hwnd, &sel_btn);
    int sel_open  = g_ui.preset_open;
    int sel_hover = (g_ui.hovered_btn == 5);
    COLORREF sel_fill = sel_open ? RGB(42, 46, 58)
                      : (sel_hover ? RGB(38, 42, 54) : UI_COLOR_PANEL);
    COLORREF sel_bdr  = (sel_open || sel_hover) ? UI_COLOR_ACCENT : UI_COLOR_BORDER;
    draw_rounded_rect(memDC, sel_btn, sel_fill, sel_bdr, 4);
    SetTextColor(memDC, sel_hover ? UI_COLOR_ACCENT : UI_COLOR_TEXT);
    SelectObject(memDC, g_ui.font_regular);
    char sel_txt[80];
    snprintf(sel_txt, sizeof(sel_txt), "%s", g_ui.preset_label[0] ? g_ui.preset_label : "select preset");
    DrawTextA(memDC, sel_txt, -1, &sel_btn, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    /* Dropdown caret: 4x supersampled DIB for smooth anti-aliased edges. */
    {
        int asize = 10;
        int ss = 4;
        int ss_size = asize * ss;
        double cx = ss_size * 0.5;
        double top_y = ss_size * 0.22;

        BITMAPINFO bmi = {0};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = ss_size;
        bmi.bmiHeader.biHeight = -ss_size;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        void* pBits = NULL;
        HBITMAP hBmp = CreateDIBSection(memDC, &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);
        if (hBmp && pBits) {
            HGDIOBJ oldBmp = SelectObject(memDC, hBmp);
            RECT abg = {0, 0, ss_size, ss_size};
            HBRUSH abg_br = CreateSolidBrush(UI_COLOR_PANEL);
            FillRect(memDC, &abg, abg_br);
            DeleteObject(abg_br);

            /* Distance-field coverage: the caret is a thick V line
               (left tip -> bottom -> right tip), 4x supersampled. */
            double stroke = 2.2 * ss * 0.5;
            double tip_x = ss_size - 1.0;
            double bot_y = ss_size - 1.0 - 2.0 * ss;
            double slope = (bot_y - top_y) / (tip_x - cx);   /* per-pixel dx */

            unsigned int* px = (unsigned int*)pBits;
            COLORREF col = sel_hover ? UI_COLOR_ACCENT : UI_COLOR_TEXT_DIM;
            int cr = col & 0xFF, cg = (col >> 8) & 0xFF, cb = (col >> 16) & 0xFF;

            for (int yy = 0; yy < ss_size; yy++) {
                for (int xx = 0; xx < ss_size; xx++) {
                    double dx = (double)xx + 0.5;
                    double dy = (double)yy + 0.5;
                    /* distance to the two V strokes */
                    double d1 = fabs(dy - (top_y + slope * dx));
                    double d2 = fabs(dy - (top_y + slope * (tip_x - dx)));
                    double d = (d1 < d2) ? d1 : d2;
                    if (dx < 0.5 || dx > tip_x - 0.5) d = 1e9;   /* outside arms */
                    double a = (d < stroke) ? (1.0 - d / stroke) : 0.0;
                    if (a > 0.0) {
                        unsigned int* p = &px[yy * ss_size + xx];
                        unsigned int bg = *p;
                        int br = (bg >> 16) & 0xFF;   /* red   (bits 16-23) */
                        int bgc = (bg >> 8) & 0xFF;   /* green (bits 8-15)  */
                        int bb = bg & 0xFF;           /* blue  (bits 0-7)   */
                        int r8 = (int)(br + (cr - br) * a);
                        int g8 = (int)(bgc + (cg - bgc) * a);
                        int b8 = (int)(bb + (cb - bb) * a);
                        *p = 0xFF000000u | ((unsigned)r8 << 16) | ((unsigned)g8 << 8) | (unsigned)b8;
                    }
                }
            }

            SetStretchBltMode(memDC, COLORONCOLOR);
            StretchBlt(memDC, sel_btn.right - 22, (sel_btn.top + sel_btn.bottom) / 2 - asize / 2,
                       asize, asize, memDC, 0, 0, ss_size, ss_size, SRCCOPY);
            SelectObject(memDC, oldBmp);
            DeleteObject(hBmp);
        }
    }

    /* [save] / [load] preset text files */
    RECT sav_btn, loa_btn;
    preset_save_button_rect(hwnd, &sav_btn);
    preset_load_button_rect(hwnd, &loa_btn);
    int sav_hover = (g_ui.hovered_btn == 6);
    int loa_hover = (g_ui.hovered_btn == 7);
    COLORREF sav_fill = sav_hover ? RGB(38, 42, 54) : UI_COLOR_PANEL;
    COLORREF sav_bdr  = sav_hover ? UI_COLOR_ACCENT : UI_COLOR_BORDER;
    COLORREF sav_txt  = sav_hover ? UI_COLOR_ACCENT : UI_COLOR_TEXT;
    draw_rounded_rect(memDC, sav_btn, sav_fill, sav_bdr, 4);
    SetTextColor(memDC, sav_txt);
    SelectObject(memDC, g_ui.font_regular);
    DrawTextA(memDC, "[save]", -1, &sav_btn, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    COLORREF loa_fill = loa_hover ? RGB(38, 42, 54) : UI_COLOR_PANEL;
    COLORREF loa_bdr  = loa_hover ? UI_COLOR_ACCENT : UI_COLOR_BORDER;
    COLORREF loa_txt  = loa_hover ? UI_COLOR_ACCENT : UI_COLOR_TEXT;
    draw_rounded_rect(memDC, loa_btn, loa_fill, loa_bdr, 4);
    SetTextColor(memDC, loa_txt);
    SelectObject(memDC, g_ui.font_regular);
    DrawTextA(memDC, "[load]", -1, &loa_btn, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    RECT play_btn = {w - 290, 12, w - 204, 38};
    int play_flash = (now - g_ui.btn_flash_time[0] < 120);
    int play_hover = (g_ui.hovered_btn == 1);
    COLORREF play_fill = play_flash ? UI_COLOR_PAD_ACTIVE : (play_hover ? RGB(38, 42, 54) : UI_COLOR_PANEL);
    COLORREF play_bdr  = (play_flash || play_hover) ? UI_COLOR_ACCENT : UI_COLOR_BORDER;
    COLORREF play_txt  = play_flash ? RGB(10, 14, 18) : (play_hover ? UI_COLOR_ACCENT : UI_COLOR_TEXT);
    draw_rounded_rect(memDC, play_btn, play_fill, play_bdr, 4);
    SetTextColor(memDC, play_txt);
    SelectObject(memDC, g_ui.font_regular);
    DrawTextA(memDC, "[play]", -1, &play_btn, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    RECT exp_btn = {w - 196, 12, w - 110, 38};
    int exp_flash = (now - g_ui.btn_flash_time[1] < 120);
    int exp_hover = (g_ui.hovered_btn == 2);
    COLORREF exp_fill = exp_flash ? UI_COLOR_PAD_ACTIVE : (exp_hover ? RGB(38, 42, 54) : UI_COLOR_PANEL);
    COLORREF exp_bdr  = (exp_flash || exp_hover) ? UI_COLOR_ACCENT : UI_COLOR_BORDER;
    COLORREF exp_txt  = exp_flash ? RGB(10, 14, 18) : (exp_hover ? UI_COLOR_ACCENT : UI_COLOR_TEXT);
    draw_rounded_rect(memDC, exp_btn, exp_fill, exp_bdr, 4);
    SetTextColor(memDC, exp_txt);
    SelectObject(memDC, g_ui.font_regular);
    DrawTextA(memDC, "[export]", -1, &exp_btn, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    RECT rst_btn = {w - 102, 12, w - UI_MARGIN, 38};
    int rst_flash = (now - g_ui.btn_flash_time[2] < 120);
    int rst_hover = (g_ui.hovered_btn == 3);
    COLORREF rst_fill = rst_flash ? UI_COLOR_PAD_ACTIVE : (rst_hover ? RGB(38, 42, 54) : UI_COLOR_PANEL);
    COLORREF rst_bdr  = (rst_flash || rst_hover) ? UI_COLOR_ACCENT : UI_COLOR_BORDER;
    COLORREF rst_txt  = rst_flash ? RGB(10, 14, 18) : (rst_hover ? UI_COLOR_ACCENT : UI_COLOR_TEXT);
    draw_rounded_rect(memDC, rst_btn, rst_fill, rst_bdr, 4);
    SetTextColor(memDC, rst_txt);
    SelectObject(memDC, g_ui.font_regular);
    DrawTextA(memDC, "[reset]", -1, &rst_btn, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    draw_master_volume_knob(memDC, w);

    /* 3. Oscilloscope (moved up) */
    RECT scope_r = {UI_MARGIN, 60, w - UI_MARGIN, 140};
    draw_oscilloscope(memDC, scope_r);

    /* 4. Synth Module Panels (moved up) */
    int card_y = 148;
    int bottom_bar_h = 24;
    int keyboard_room = 118; /* tall 2-octave keyboard + status strip */
    int card_h = h - card_y - bottom_bar_h - keyboard_room;
    if (card_h < 224) card_h = 224;
    int card_w = (w - UI_MARGIN_DOUBLE - (4 - 1) * UI_CARD_GAP) / 4;
    const char* module_titles[4] = {
        "oscillator / fm",
        "additive / noise",
        "filter / drive",
        "envelope / mod"
    };

    for (int col = 0; col < 4; col++) {
        RECT cr = {UI_MARGIN + col * (card_w + UI_CARD_GAP), card_y, UI_MARGIN + col * (card_w + UI_CARD_GAP) + card_w, card_y + card_h};
        draw_rounded_rect(memDC, cr, UI_COLOR_PANEL, UI_COLOR_BORDER, 6);

        RECT hr = {cr.left, cr.top, cr.right, cr.top + 22};
        draw_rounded_rect(memDC, hr, UI_COLOR_PANEL_HDR, UI_COLOR_BORDER, 6);
        SetTextColor(memDC, UI_COLOR_ACCENT);
        SelectObject(memDC, g_ui.font_regular);
        DrawTextA(memDC, module_titles[col], -1, &hr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    /* 5. Knobs */
    for (int i = 0; i < g_ui.knob_count; i++) {
        draw_knob(memDC, &g_ui.knobs[i], (i == g_ui.active_knob_idx));
    }

    /* 6. Piano Keyboard at bottom (taller keys for two playable octaves) */
    int key_y = h - 118;
    RECT key_area = {UI_MARGIN, key_y, w - UI_MARGIN, h - 22};
    draw_piano_keyboard(memDC, key_area);

    /* 7. Bottom status strip: octave indicator + export status */
    SelectObject(memDC, g_ui.font_small ? g_ui.font_small : g_ui.font_regular);
    SetTextColor(memDC, UI_COLOR_TEXT_DIM);
    char oct_str[48];
    snprintf(oct_str, sizeof(oct_str), "Octave %+d", g_ui.kb_octave);
    RECT oct_rc = { UI_MARGIN, h - 22, UI_MARGIN + 240, h - 2 };
    DrawTextA(memDC, oct_str, -1, &oct_rc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    DWORD export_elapsed = GetTickCount() - g_ui.export_finish_time;
    if (g_ui.export_in_progress || (g_ui.export_finish_time > 0 && export_elapsed < 3000)) {
        char exp_str[256];
        COLORREF exp_col;
        if (g_ui.export_in_progress) {
            snprintf(exp_str, sizeof(exp_str), "Exporting... %d%%", g_ui.export_progress);
            exp_col = UI_COLOR_ACCENT;
        } else {
            snprintf(exp_str, sizeof(exp_str), "Saved: %s", g_ui.export_filename);
            exp_col = RGB(100, 230, 120);
        }

        SelectObject(memDC, g_ui.font_regular ? g_ui.font_regular : g_ui.font_header);
        SetTextColor(memDC, exp_col);

        RECT exp_rc = {UI_MARGIN, h - 22, w - UI_MARGIN, h - 2};
        DrawTextA(memDC, exp_str, -1, &exp_rc, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
    }

    BitBlt(hdc, 0, 0, w, h, memDC, 0, 0, SRCCOPY);
    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);
}

/* ========================================================================
   Knob & Interaction Setup (modified for single patch)
   ======================================================================== */

static void ui_update_knob_layout(int w, int h) {
    (void)h;
    int card_y = 148;
    int card_w = (w - UI_MARGIN_DOUBLE - (4 - 1) * UI_CARD_GAP) / 4;
    int knob_w = 80;

    /* Knobs per card: 8, 7, 7, 7 (SET_KNOB order is card-major). */
    static const int knobs_per_card[4] = { 8, 7, 7, 7 };
    int row0 = card_y + 24;   /* first knob row, just below the card header */
    int row_pitch = 76;       /* 72px knob cell + 4px gap */

    int kidx = 0;
    for (int col = 0; col < 4; col++) {
        int cx = UI_MARGIN + col * (card_w + UI_CARD_GAP);
        int half_w = card_w / 2;
        int count = knobs_per_card[col];

        for (int idx = 0; idx < count; idx++, kidx++) {
            int row = idx / 2;
            int sub = idx % 2;
            int kx;
            if ((count & 1) && idx == count - 1) {
                /* Odd knob count in the bottom three cards: center the
                   lone last knob instead of leaving it bottom-left. */
                kx = cx + (card_w - knob_w) / 2;
            } else {
                kx = cx + sub * half_w + (half_w - knob_w) / 2;
            }
            int ky = row0 + row * row_pitch;
            g_ui.knobs[kidx].rect = (RECT){kx, ky, kx + knob_w, ky + 72};
        }
    }
}

static void ui_init_knobs(void) {
    HaloPatch* p = &g_current_patch; /* use global patch */
    g_ui.knob_count = 29;

    #define SET_KNOB(idx, lbl, unt, ptr, mn, mx, is_i, dec) do { \
        g_ui.knobs[idx].id = idx; \
        g_ui.knobs[idx].label = lbl; \
        g_ui.knobs[idx].unit = unt; \
        g_ui.knobs[idx].param_ptr = ptr; \
        g_ui.knobs[idx].min_val = mn; \
        g_ui.knobs[idx].max_val = mx; \
        g_ui.knobs[idx].is_int = is_i; \
        g_ui.knobs[idx].decimals = dec; \
        g_ui.knobs[idx].curve = knob_curve_for_unit(unt); \
    } while (0)

    /* Card 1: OSC / FM (8) */
    SET_KNOB(0,  "pitch",       "st",  &p->pitch_semi,      -24.0, 24.0,   1, 0);
    SET_KNOB(1,  "wave",        "",    &p->waveform,        0.0,   3.0,    1, 0);
    SET_KNOB(2,  "fm ratio",    "x",   &p->fm_ratio,        0.5,   8.0,    0, 2);
    SET_KNOB(3,  "fm depth",    "idx", &p->fm_depth,        0.0,   6.0,    0, 2);
    SET_KNOB(4,  "fm feedbk",   "%",   &p->fm_feedback,     0.0,   1.0,    0, 0);
    SET_KNOB(5,  "osc mix",     "%",   &p->osc_mix,         0.0,   1.0,    0, 0);
    SET_KNOB(6,  "unison",      "",    &p->unison_voices,   1.0,   8.0,    1, 0);
    SET_KNOB(7,  "u spread",    "ct",  &p->unison_spread,   0.0,   50.0,   0, 0);

    /* Card 2: ADDITIVE / NOISE (7) */
    SET_KNOB(8,  "detune",      "ct",  &p->detune,          0.0,   50.0,   0, 0);
    SET_KNOB(9,  "partials",    "",    &p->partial_count,   1.0,   12.0,   1, 0);
    SET_KNOB(10, "harm tilt",   "",    &p->partial_tilt,    -2.0,  2.0,    0, 2);
    SET_KNOB(11, "noise mix",   "%",   &p->noise_mix,       0.0,   1.0,    0, 0);
    SET_KNOB(12, "noise col",   "Hz",  &p->noise_cutoff,    100.0, 16000.0,0, 0);
    SET_KNOB(13, "harm decay",  "%",   &p->harm_decay,      0.0,   1.0,    0, 0);
    SET_KNOB(14, "inharm",      "%",   &p->inharm,          0.0,   1.0,    0, 0);

    /* Card 3: FILTER / DRIVE (7) */
    SET_KNOB(15, "cutoff",      "Hz",  &p->filter_cutoff,   50.0,  18000.0,0, 0);
    SET_KNOB(16, "resonance",   "Q",   &p->filter_q,        0.5,   20.0,   0, 1);
    SET_KNOB(17, "f drive",     "x",   &p->filter_drive,    1.0,   8.0,    0, 1);
    SET_KNOB(18, "drive",       "x",   &p->drive,           1.0,   6.0,    0, 1);
    SET_KNOB(19, "filt type",   "",    &p->filter_type,     0.0,   3.0,    1, 0);
    SET_KNOB(20, "lfo filt",    "Hz",  &p->lfo_filt_depth,  0.0,   2000.0, 0, 0);
    SET_KNOB(21, "key track",   "%",   &p->key_track,       0.0,   1.0,    0, 0);

    /* Card 4: ENVELOPE / MOD (7) */
    SET_KNOB(22, "amp attack",  "s",   &p->amp_attack,      0.002, 2.0,    0, 3);
    SET_KNOB(23, "amp decay",   "s",   &p->amp_decay,       0.05,  4.0,    0, 2);
    SET_KNOB(24, "release",     "s",   &p->amp_release,     0.02,  6.0,    0, 2);
    SET_KNOB(25, "sustain",     "%",   &p->amp_sustain,     0.0,   1.0,    0, 0);
    SET_KNOB(26, "env filt",    "%",   &p->filter_env_depth,-1.0,  1.0,    0, 0);
    SET_KNOB(27, "lfo rate",    "Hz",  &p->lfo_rate,        0.1,   20.0,   0, 2);
    SET_KNOB(28, "vibrato",     "ct",  &p->vibrato,         0.0,   100.0,  0, 0);

    #undef SET_KNOB

    audio_set_patch(&g_current_patch);

    if (g_ui.hwnd_main) {
        RECT rc;
        GetClientRect(g_ui.hwnd_main, &rc);
        ui_update_knob_layout(rc.right - rc.left, rc.bottom - rc.top);
    }
}

/* Per-letter title opacity jitter on note triggers; ui_paint eases each
   letter back to its resting alpha with exponential decay. */
static void ui_halo_text_on_key_press(void) {
    for (int i = 0; i < 4; i++) {
        g_ui.title_alpha[i] = 0.40f + ((float)rand() / (float)RAND_MAX) * 0.55f;
    }
}

static void ui_play_audition(void) {
    g_ui.note_trigger_time = GetTickCount();
    ui_halo_text_on_key_press();
    audio_note_on(60, 0.85f);   /* C4 */
    audio_note_on(64, 0.75f);   /* E4 */
    audio_note_on(67, 0.70f);   /* G4 */
    SetTimer(g_ui.hwnd_main, 105, 300, NULL);
}

/* Preview tone: DISABLED. Injecting a synthetic note while the user tweaks
   knobs voice-steals held notes and bounces the scope with transients, so
   knobs are now purely visual+audio-live (block-rate sync) with no tone. */
#define HALO_PREVIEW_NOTE 60

static void ui_preview_tone_start(void) {
    (void)0;   /* intentionally inert */
}

static void ui_preview_tone_tick(void) {
    KillTimer(g_ui.hwnd_main, 106);
}

static void ui_trigger_export(HWND hwnd) {
    if (InterlockedCompareExchange(&g_ui.export_in_progress, 0, 0) != 0) return;

    OPENFILENAMEA ofn;
    char szFile[MAX_PATH];
    char preset_clean[32];
    strncpy(preset_clean, "halo_synth", sizeof(preset_clean) - 1);
    preset_clean[sizeof(preset_clean) - 1] = '\0';
    for (char* cp = preset_clean; *cp; cp++) {
        if (*cp == ' ') *cp = '_';
    }

    snprintf(szFile, sizeof(szFile), "halo_%s_%lu.wav", preset_clean, (unsigned long)time(NULL));

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "Wave Audio (*.wav)\0*.wav\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrDefExt = "wav";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;

    if (GetSaveFileNameA(&ofn)) {
        ExportJob* job = (ExportJob*)malloc(sizeof(ExportJob));
        if (!job) return;

        strncpy(job->filepath, ofn.lpstrFile, MAX_PATH - 1);
        job->filepath[MAX_PATH - 1] = '\0';

        const char* base_name = strrchr(job->filepath, '\\');
        if (!base_name) base_name = strrchr(job->filepath, '/');
        if (base_name) base_name++;
        else base_name = job->filepath;
        strncpy(g_ui.export_filename, base_name, sizeof(g_ui.export_filename) - 1);
        g_ui.export_filename[sizeof(g_ui.export_filename) - 1] = '\0';

        job->count = halo_render_offline(&g_current_patch, 60, 0.85f, 2.5f, job->buffer, HALO_MAX_SAMPLES);
        job->notify_hwnd = hwnd;

        InterlockedExchange(&g_ui.export_in_progress, 1);
        InterlockedExchange(&g_ui.export_progress, 0);
        g_ui.export_finish_time = 0;

        HANDLE hThread = CreateThread(NULL, 0, async_export_worker, job, 0, NULL);
        if (hThread) {
            CloseHandle(hThread);
            SetTimer(hwnd, 103, 30, NULL);
        } else {
            free(job);
            InterlockedExchange(&g_ui.export_in_progress, 0);
        }
    }
}

/* ========================================================================
   Custom Preset Save / Load (plain text files, *.halo.txt)
   ======================================================================== */

static void ui_save_preset_dialog(HWND hwnd) {
    OPENFILENAMEA ofn;
    char szFile[MAX_PATH];

    /* Default the filename to the current preset label. */
    char default_name[64];
    snprintf(default_name, sizeof(default_name), "%s",
             g_ui.preset_label[0] ? g_ui.preset_label : "my_preset");
    for (char* c = default_name; *c; c++) {
        if (*c == ' ' || *c == '/' || *c == '\\' || *c == ':' ||
            *c == '*' || *c == '?' || *c == '"' || *c == '<' || *c == '>' || *c == '|') {
            *c = '_';
        }
    }

    /* Start in the exe's folder so custom presets live with the app. */
    char init_dir[MAX_PATH];
    GetModuleFileNameA(NULL, init_dir, MAX_PATH);
    char* slash = strrchr(init_dir, '\\');
    if (slash) *slash = '\0';

    snprintf(szFile, sizeof(szFile), "%s.halo.txt", default_name);

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrInitialDir = init_dir;
    ofn.lpstrFilter = "Halo Preset (*.halo.txt)\0*.halo.txt\0Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
    ofn.lpstrDefExt = "halo.txt";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;

    if (GetSaveFileNameA(&ofn)) {
        /* Name inside the file: filename without extension, editable later. */
        const char* base = strrchr(szFile, '\\');
        if (!base) base = strrchr(szFile, '/');
        base = base ? base + 1 : szFile;
        char name[64];
        snprintf(name, sizeof(name), "%s", base);
        char* dot = strrchr(name, '.');
        if (dot) *dot = '\0';

        preset_save_text(szFile, name, &g_current_patch);
        snprintf(g_ui.preset_label, sizeof(g_ui.preset_label), "%s", name);
        preset_scan_user_files();
        InvalidateRect(hwnd, NULL, FALSE);
    }
}

static void ui_load_preset_dialog(HWND hwnd) {
    OPENFILENAMEA ofn;
    char szFile[MAX_PATH];
    szFile[0] = '\0';

    char init_dir[MAX_PATH];
    GetModuleFileNameA(NULL, init_dir, MAX_PATH);
    char* slash = strrchr(init_dir, '\\');
    if (slash) *slash = '\0';

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrInitialDir = init_dir;
    ofn.lpstrFilter = "Halo Preset (*.halo.txt)\0*.halo.txt\0Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileNameA(&ofn)) {
        HaloPatch loaded = g_current_patch;   /* missing fields keep current */
        char name[64] = "";
        preset_load_text(szFile, &loaded, name, sizeof(name));

        const char* base = strrchr(szFile, '\\');
        if (!base) base = strrchr(szFile, '/');
        base = base ? base + 1 : szFile;
        if (!name[0]) {
            snprintf(name, sizeof(name), "%s", base);
            char* dot = strrchr(name, '.');
            if (dot) *dot = '\0';
        }

        g_current_patch = loaded;
        snprintf(g_ui.preset_label, sizeof(g_ui.preset_label), "%s", name);
        ui_init_knobs();
        InvalidateRect(hwnd, NULL, FALSE);
    }
}

/* ========================================================================
   Musical Typing (QWERTY Piano) Key Mapper
   ======================================================================== */

/* FL-style two-octave musical typing. Returns the MIDI note of the key at
   octave shift 0 (C4-based), or -1 if the key is not mapped. Lower octave
   sits on the home/bottom letter rows, upper octave on Q-row + number row. */
static int keycode_to_midi_note(WPARAM vk) {
    switch (vk) {
        /* Lower octave whites: Z X C V B N M , */
        case 'Z': return 60;  /* C4 */
        case 'X': return 62;  /* D4 */
        case 'C': return 64;  /* E4 */
        case 'V': return 65;  /* F4 */
        case 'B': return 67;  /* G4 */
        case 'N': return 69;  /* A4 */
        case 'M': return 71;  /* B4 */
        case VK_OEM_COMMA: return 72; /* C5 */

        /* Lower octave blacks: S D G H J */
        case 'S': return 61;  /* C#4 */
        case 'D': return 63;  /* D#4 */
        case 'G': return 66;  /* F#4 */
        case 'H': return 68;  /* G#4 */
        case 'J': return 70;  /* A#4 */

        /* Upper octave whites: Q W E R T Y U I */
        case 'Q': return 72;  /* C5 */
        case 'W': return 74;  /* D5 */
        case 'E': return 76;  /* E5 */
        case 'R': return 77;  /* F5 */
        case 'T': return 79;  /* G5 */
        case 'Y': return 81;  /* A5 */
        case 'U': return 83;  /* B5 */
        case 'I': return 84;  /* C6 */

        /* Upper octave blacks: 2 3 5 6 7 */
        case '2': return 73;  /* C#5 */
        case '3': return 75;  /* D#5 */
        case '5': return 78;  /* F#5 */
        case '6': return 80;  /* G#5 */
        case '7': return 82;  /* A#5 */

        default:  return -1;
    }
}

/* Release every note held by the computer keyboard (used on octave shifts
   so keys held across a shift don't get stuck at the old pitch). */
static void kb_release_all_notes(void) {
    for (int vk = 0; vk < 256; vk++) {
        int note = g_ui.kb_notes[vk];
        if (note >= 0) {
            audio_note_off(note);
            if (note < 128) g_ui.keyboard_note_state[note] = 0;
            g_ui.kb_notes[vk] = -1;
        }
        g_ui.key_down_state[vk] = 0;
    }
    InvalidateRect(g_ui.hwnd_main, &g_ui.keyboard_rect, FALSE);
}

/* ========================================================================
   Main Window Procedure
   ======================================================================== */
#include <mmsystem.h>
#ifdef _MSC_VER
#pragma comment(lib, "winmm.lib")
#endif

/* ========================================================================
   High-Precision 120 FPS QPC Pacer (Halo Engine)
   ======================================================================== */

static volatile LONG g_pacer_running = 0;
static HANDLE        g_pacer_thread  = NULL;

static DWORD WINAPI halo_qpc_pacer_worker(LPVOID param) {
    HWND hwnd = (HWND)param;
    timeBeginPeriod(1);
    
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    const double target_fps = 120.0;
    int64_t ticks_per_frame = (int64_t)((double)freq.QuadPart / target_fps);

    LARGE_INTEGER t_curr;
    QueryPerformanceCounter(&t_curr);
    int64_t next_frame = t_curr.QuadPart + ticks_per_frame;

    while (InterlockedCompareExchange(&g_pacer_running, 1, 1) == 1) {
        /* Check if anything is actually active */
        int is_active = (audio_get_active_voices() > 0) ||
                        (audio_get_level() > 0.001f)    ||
                        (g_ui.active_knob_idx >= 0)     ||
                        (g_ui.master_dragging)          ||
                        (g_ui.mouse_note >= 0)          ||
                        (GetTickCount() - g_ui.note_trigger_time < 1200);

        if (!is_active) {
            /* Quiescent Idle: don't spin, don't invalidate. Sleep for 50ms */
            Sleep(50);
            QueryPerformanceCounter(&t_curr);
            next_frame = t_curr.QuadPart + ticks_per_frame;
            continue;
        }

        /* Active: Run 120 FPS butter-smooth */
        QueryPerformanceCounter(&t_curr);
        if (t_curr.QuadPart >= next_frame) {
            if (IsWindow(hwnd)) {
                InvalidateRect(hwnd, NULL, FALSE);
            }
            next_frame += ticks_per_frame;
            if (next_frame < t_curr.QuadPart) {
                next_frame = t_curr.QuadPart + ticks_per_frame;
            }
        }

        /* Yield / sleep remainder */
        QueryPerformanceCounter(&t_curr);
        int64_t diff = next_frame - t_curr.QuadPart;
        if (diff > (int64_t)(freq.QuadPart * 0.002)) {
            DWORD ms = (DWORD)((diff * 1000) / freq.QuadPart) - 1;
            if (ms > 0) Sleep(ms);
        } else {
            Sleep(0);
        }
    }

    timeEndPeriod(1);
    return 0;
}

static void halo_pacer_start(HWND hwnd) {
    if (InterlockedCompareExchange(&g_pacer_running, 1, 0) == 0) {
        g_pacer_thread = CreateThread(NULL, 0, halo_qpc_pacer_worker, (LPVOID)hwnd, 0, NULL);
    }
}

static void halo_pacer_stop(void) {
    if (InterlockedCompareExchange(&g_pacer_running, 0, 1) == 1) {
        if (g_pacer_thread) {
            WaitForSingleObject(g_pacer_thread, 100);
            CloseHandle(g_pacer_thread);
            g_pacer_thread = NULL;
        }
    }
}

/* ========================================================================
   Main Window Procedure
   ======================================================================== */

static LRESULT CALLBACK HaloWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            srand((unsigned)time(NULL));
            for (int i = 0; i < 4; i++) g_ui.title_alpha[i] = 1.0f;
            g_ui.hwnd_main = hwnd;
            g_ui.active_knob_idx = -1;
            g_ui.mouse_note = -1;
            g_ui.kb_octave = 0;
            for (int i = 0; i < 256; i++) g_ui.kb_notes[i] = -1;

            g_ui.preset_open = 0;
            g_ui.preset_hover = -1;
            g_ui.preset_sel = 0;
            g_ui.user_preset_count = 0;

            /* Initialize default patch (Obsidian Pad) */
            halo_get_preset(0, &g_current_patch);
            snprintf(g_ui.preset_label, sizeof(g_ui.preset_label), "%s", HALO_PRESET_NAMES[0]);
            preset_scan_user_files();
            ui_init_knobs();

            /* Keep the audio master volume in sync with the UI control */
            audio_set_master_volume(g_master_volume);

            g_ui.font_regular = create_ui_font(14);
            g_ui.font_header  = create_ui_font(13);
            g_ui.font_title   = create_ui_font(24);
            g_ui.font_small   = create_ui_font(12);

            /* Start high-precision 120 FPS QPC UI pacer thread */
            halo_pacer_start(hwnd);
            return 0;
        }

        case WM_GETMINMAXINFO: {
            MINMAXINFO* mmi = (MINMAXINFO*)lParam;
            RECT rc = { 0, 0, 960, 640 }; /* Height fits 4 knob rows + piano keyboard */
            AdjustWindowRectEx(&rc, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_SIZEBOX, FALSE, WS_EX_APPWINDOW);
            int fixed_h = rc.bottom - rc.top;
            int min_w   = rc.right - rc.left;
            mmi->ptMinTrackSize.x = min_w;
            mmi->ptMinTrackSize.y = fixed_h;
            mmi->ptMaxTrackSize.y = fixed_h;
            return 0;
        }

        case WM_NCHITTEST: {
            LRESULT hit = DefWindowProcA(hwnd, msg, wParam, lParam);
            if (hit == HTTOP || hit == HTBOTTOM) return HTBORDER;
            if (hit == HTTOPLEFT || hit == HTBOTTOMLEFT) return HTLEFT;
            if (hit == HTTOPRIGHT || hit == HTBOTTOMRIGHT) return HTRIGHT;
            return hit;
        }

        case WM_SIZE: {
            RECT rc;
            GetClientRect(hwnd, &rc);
            ui_update_knob_layout(rc.right - rc.left, rc.bottom - rc.top);
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }

        case WM_ACTIVATE:
            /* Close preset dropdown whenever the window loses focus */
            if (LOWORD(wParam) == WA_INACTIVE) {
                preset_close_popup();
            }
            break;

        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            ui_paint(hwnd, hdc);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_TIMER: {
            if (wParam == 103) {
                if (InterlockedCompareExchange(&g_ui.export_in_progress, 0, 0) == 0) {
                    DWORD done_elapsed = GetTickCount() - g_ui.export_finish_time;
                    if (done_elapsed >= 3000) KillTimer(hwnd, 103);
                }
                InvalidateRect(hwnd, NULL, FALSE);
            }
            if (wParam == 105) {
                KillTimer(hwnd, 105);
                audio_note_off(60);
                audio_note_off(64);
                audio_note_off(67);
            }
            if (wParam == 106) {
                ui_preview_tone_tick();
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }

        case WM_MOUSELEAVE: {
            if (g_ui.hovered_btn != 0) {
                g_ui.hovered_btn = 0;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }

        case WM_LBUTTONDOWN: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            RECT rc;
            GetClientRect(hwnd, &rc);
            int w = rc.right;

            RECT kb_btn;
            keybinds_button_rect(hwnd, &kb_btn);
            if (PtInRect(&kb_btn, (POINT){x, y})) {
                g_ui.btn_flash_time[3] = GetTickCount();
                InvalidateRect(hwnd, NULL, FALSE);
                open_keybinds_dialog(hwnd);
                return 0;
            }

            /* Preset selector dropdown toggle */
            RECT sel_btn;
            preset_selector_rect(hwnd, &sel_btn);
            if (PtInRect(&sel_btn, (POINT){x, y})) {
                preset_toggle_dropdown(hwnd);
                InvalidateRect(hwnd, &sel_btn, FALSE);
                return 0;
            }

            /* [save] / [load] buttons */
            RECT sav_btn, loa_btn;
            preset_save_button_rect(hwnd, &sav_btn);
            preset_load_button_rect(hwnd, &loa_btn);
            if (PtInRect(&sav_btn, (POINT){x, y})) {
                ui_save_preset_dialog(hwnd);
                return 0;
            }
            if (PtInRect(&loa_btn, (POINT){x, y})) {
                ui_load_preset_dialog(hwnd);
                return 0;
            }

            RECT play_btn = {w - 290, 12, w - 204, 38};
            if (PtInRect(&play_btn, (POINT){x, y})) {
                g_ui.btn_flash_time[0] = GetTickCount();
                ui_play_audition();
                return 0;
            }

            RECT exp_btn = {w - 196, 12, w - 110, 38};
            if (PtInRect(&exp_btn, (POINT){x, y})) {
                g_ui.btn_flash_time[1] = GetTickCount();
                InvalidateRect(hwnd, NULL, FALSE);
                ui_trigger_export(hwnd);
                return 0;
            }

            RECT rst_btn = {w - 102, 12, w - UI_MARGIN, 38};
            if (PtInRect(&rst_btn, (POINT){x, y})) {
                g_ui.btn_flash_time[2] = GetTickCount();
                halo_get_preset(0, &g_current_patch);
                snprintf(g_ui.preset_label, sizeof(g_ui.preset_label), "%s", HALO_PRESET_NAMES[0]);
                g_ui.preset_sel = 0;
                ui_init_knobs();
                ui_play_audition();
                return 0;
            }

            RECT master_rect;
            get_master_knob_rect(w, &master_rect);
            if (PtInRect(&master_rect, (POINT){x, y})) {
                g_ui.master_dragging = 1;
                g_ui.master_drag_start_y = y;
                g_ui.master_drag_start_val = g_master_volume;
                SetCapture(hwnd);
                InvalidateRect(hwnd, &master_rect, FALSE);
                return 0;
            }

            /* Piano keyboard hit-testing */
            if (PtInRect(&g_ui.keyboard_rect, (POINT){x, y})) {
                int note = piano_note_from_point(&g_ui.keyboard_rect, x, y);
                if (note >= 0 && note < 128) {
                    float vel = piano_velocity_at(y);
                    g_ui.keyboard_note_state[note] = 1;
                    ui_halo_text_on_key_press();
                    audio_note_on(note, vel);
                    g_ui.mouse_note = note;
                    InvalidateRect(hwnd, &g_ui.keyboard_rect, FALSE);
                }
                SetCapture(hwnd);
                return 0;
            }

            /* Knobs */
            for (int i = 0; i < g_ui.knob_count; i++) {
                if (PtInRect(&g_ui.knobs[i].rect, (POINT){x, y})) {
                    g_ui.active_knob_idx = i;
                    g_ui.drag_start_y = y;
                    g_ui.drag_start_val = *g_ui.knobs[i].param_ptr;
                    SetCapture(hwnd);
                    InvalidateRect(hwnd, &g_ui.knobs[i].rect, FALSE);
                    return 0;
                }
            }
            return 0;
        }

        case WM_MOUSEWHEEL: {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ScreenToClient(hwnd, &pt);
            short zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
            double notches = zDelta / 120.0;

            RECT rc;
            GetClientRect(hwnd, &rc);
            RECT master_rect;
            get_master_knob_rect(rc.right, &master_rect);

            if (PtInRect(&master_rect, pt)) {
                float sens = (GetKeyState(VK_SHIFT) & 0x8000) ? 0.015f : 0.06f;
                float new_val = g_master_volume + (float)(notches * sens);
                if (new_val < 0.0f) new_val = 0.0f;
                if (new_val > 1.0f) new_val = 1.0f;
                g_master_volume = new_val;
                audio_set_master_volume(g_master_volume);
                InvalidateRect(hwnd, &master_rect, FALSE);
                return 0;
            }

            for (int i = 0; i < g_ui.knob_count; i++) {
                if (!PtInRect(&g_ui.knobs[i].rect, pt)) continue;
                KnobCtrl* k = &g_ui.knobs[i];
                double new_val;

                if (k->is_int) {
                    double step = (notches > 0.0) ? 1.0 : -1.0;
                    new_val = floor(*k->param_ptr + 0.5) + step;
                } else {
                    double sens = (GetKeyState(VK_SHIFT) & 0x8000) ? 0.015 : 0.06;
                    new_val = knob_norm_to_value(k, knob_value_to_norm(k, *k->param_ptr) + notches * sens);
                }

                if (new_val < k->min_val) new_val = k->min_val;
                if (new_val > k->max_val) new_val = k->max_val;
                *k->param_ptr = new_val;

                audio_set_patch(&g_current_patch);
                InvalidateRect(hwnd, &k->rect, FALSE);
                return 0;
            }
            break;
        }

        case WM_MOUSEMOVE: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);

            TRACKMOUSEEVENT tme = {sizeof(TRACKMOUSEEVENT), TME_LEAVE, hwnd, 0};
            TrackMouseEvent(&tme);

            RECT rc;
            GetClientRect(hwnd, &rc);
            int w = rc.right;

            RECT kb_btn;
            keybinds_button_rect(hwnd, &kb_btn);
            RECT sel_btn;
            preset_selector_rect(hwnd, &sel_btn);
            RECT sav_btn, loa_btn;
            preset_save_button_rect(hwnd, &sav_btn);
            preset_load_button_rect(hwnd, &loa_btn);

            RECT play_btn = {w - 290, 12, w - 204, 38};
            RECT exp_btn  = {w - 196, 12, w - 110, 38};
            RECT rst_btn  = {w - 102, 12, w - UI_MARGIN, 38};

            int new_hover = 0;
            if (PtInRect(&play_btn, (POINT){x, y})) new_hover = 1;
            else if (PtInRect(&exp_btn, (POINT){x, y})) new_hover = 2;
            else if (PtInRect(&rst_btn, (POINT){x, y})) new_hover = 3;
            else if (PtInRect(&kb_btn, (POINT){x, y})) new_hover = 4;
            else if (PtInRect(&sel_btn, (POINT){x, y})) new_hover = 5;
            else if (PtInRect(&sav_btn, (POINT){x, y})) new_hover = 6;
            else if (PtInRect(&loa_btn, (POINT){x, y})) new_hover = 7;

            if (new_hover != g_ui.hovered_btn) {
                g_ui.hovered_btn = new_hover;
                InvalidateRect(hwnd, NULL, FALSE);
            }

            if ((wParam & MK_LBUTTON) == 0) break;

            if (g_ui.master_dragging) {
                int dy = g_ui.master_drag_start_y - y;
                float sensitivity = 0.004f;
                float new_val = g_ui.master_drag_start_val + dy * sensitivity;
                if (new_val < 0.0f) new_val = 0.0f;
                if (new_val > 1.0f) new_val = 1.0f;
                g_master_volume = new_val;
                audio_set_master_volume(g_master_volume);
                RECT master_rect;
                get_master_knob_rect(w, &master_rect);
                InvalidateRect(hwnd, &master_rect, FALSE);
                return 0;
            }

            if (g_ui.active_knob_idx >= 0) {
                KnobCtrl* k = &g_ui.knobs[g_ui.active_knob_idx];
                int dy = g_ui.drag_start_y - y;

                double sensitivity = (GetKeyState(VK_SHIFT) & 0x8000) ? 0.0015 : 0.006;
                double new_val = knob_norm_to_value(k, knob_value_to_norm(k, g_ui.drag_start_val) + dy * sensitivity);

                if (new_val < k->min_val) new_val = k->min_val;
                if (new_val > k->max_val) new_val = k->max_val;
                if (k->is_int) new_val = floor(new_val + 0.5);

                *k->param_ptr = new_val;
                audio_set_patch(&g_current_patch);
                InvalidateRect(hwnd, &k->rect, FALSE); /* NOT NULL */
                return 0;
            }

            /* Piano keyboard slide / glissando */
            if (g_ui.mouse_note >= 0) {
                int note = piano_note_from_point(&g_ui.keyboard_rect, x, y);
                if (note >= 0 && note != g_ui.mouse_note && note < 128) {
                    audio_note_off(g_ui.mouse_note);
                    g_ui.keyboard_note_state[g_ui.mouse_note] = 0;

                    float vel = piano_velocity_at(y);
                    g_ui.keyboard_note_state[note] = 1;
                    ui_halo_text_on_key_press();
                    audio_note_on(note, vel);
                    g_ui.mouse_note = note;
                    InvalidateRect(hwnd, &g_ui.keyboard_rect, FALSE);
                }
                return 0;
            }
            break;
        }

        case WM_LBUTTONUP: {
            if (g_ui.master_dragging) {
                g_ui.master_dragging = 0;
                ReleaseCapture();
            }
            if (g_ui.active_knob_idx >= 0) {
                g_ui.active_knob_idx = -1;
                ReleaseCapture();
            }
            if (g_ui.mouse_note >= 0) {
                audio_note_off(g_ui.mouse_note);
                if (g_ui.mouse_note < 128) g_ui.keyboard_note_state[g_ui.mouse_note] = 0;
                g_ui.mouse_note = -1;
                ReleaseCapture();
            }
            InvalidateRect(hwnd, &g_ui.keyboard_rect, FALSE);
            return 0;
        }

        case WM_KEYDOWN: {
            if (wParam == VK_ESCAPE) {
                DestroyWindow(hwnd);
                return 0;
            }
            if (wParam == VK_SPACE || wParam == VK_RETURN) {
                if (!(lParam & 0x40000000)) {
                    g_ui.btn_flash_time[0] = GetTickCount();
                    ui_play_audition();
                }
                return 0;
            }
            if (wParam == 'K') {
                g_ui.btn_flash_time[3] = GetTickCount();
                InvalidateRect(hwnd, NULL, FALSE);
                open_keybinds_dialog(hwnd);
                return 0;
            }
            if (wParam == 'E' && (GetKeyState(VK_CONTROL) & 0x8000)) {
                g_ui.btn_flash_time[1] = GetTickCount();
                InvalidateRect(hwnd, NULL, FALSE);
                ui_trigger_export(hwnd);
                return 0;
            }
            /* Octave shift */
            if (wParam == VK_UP || wParam == VK_DOWN) {
                int dir = (wParam == VK_UP) ? 1 : -1;
                int new_oct = g_ui.kb_octave + dir;
                if (new_oct >= -2 && new_oct <= 2) {
                    kb_release_all_notes();
                    g_ui.kb_octave = new_oct;
                    InvalidateRect(hwnd, &g_ui.keyboard_rect, FALSE);
                    InvalidateRect(hwnd, NULL, FALSE);
                }
                return 0;
            }

            /* Musical Typing */
            int note = keycode_to_midi_note(wParam);
            if (note >= 0) {
                note += 12 * g_ui.kb_octave;
                if (note < 0) note = 0;
                if (note > 127) note = 127;
                if (!g_ui.key_down_state[wParam & 0xFF]) {
                    g_ui.key_down_state[wParam & 0xFF] = 1;
                    g_ui.kb_notes[wParam & 0xFF] = note;
                    g_ui.note_trigger_time = GetTickCount();
                    g_ui.keyboard_note_state[note] = 1;
                    ui_halo_text_on_key_press();
                    audio_note_on(note, 0.85f);
                    InvalidateRect(hwnd, &g_ui.keyboard_rect, FALSE);
                }
                return 0;
            }
            break;
        }

        case WM_KEYUP: {
            int note = g_ui.kb_notes[wParam & 0xFF];
            if (note >= 0) {
                g_ui.key_down_state[wParam & 0xFF] = 0;
                g_ui.kb_notes[wParam & 0xFF] = -1;
                if (note < 128) g_ui.keyboard_note_state[note] = 0;
                audio_note_off(note);
                InvalidateRect(hwnd, &g_ui.keyboard_rect, FALSE);
                return 0;
            }
            g_ui.key_down_state[wParam & 0xFF] = 0;
            break;
        }

        case WM_DESTROY: {
            /* Safely stop and join the 120 FPS QPC pacer thread */
            halo_pacer_stop();

            audio_all_notes_off();
            HICON hIconSm = (HICON)SendMessage(hwnd, WM_GETICON, ICON_SMALL, 0);
            HICON hIconLg = (HICON)SendMessage(hwnd, WM_GETICON, ICON_BIG, 0);
            if (hIconSm) DestroyIcon(hIconSm);
            if (hIconLg) DestroyIcon(hIconLg);

            if (g_ui.font_regular) DeleteObject(g_ui.font_regular);
            if (g_ui.font_header)  DeleteObject(g_ui.font_header);
            if (g_ui.font_title)   DeleteObject(g_ui.font_title);
            if (g_ui.font_small)   DeleteObject(g_ui.font_small);
            PostQuitMessage(0);
            return 0;
        }
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

#endif /* HALO_UI_H */
