// Clear Skies: the launcher. A mod for Savage Wind that adds Cemu's Clarity presets.
//
// Shows a small window to pick the Clarity preset and where savagewind.exe is, saves both to clearskies.ini, then
// starts the game with the Clear Skies layer turned on for that one process (through the Vulkan loader's environment
// variables: nothing is installed system-wide, and the game starts normally without the launcher).
//
//   ClearSkies.exe                    the window
//   ClearSkies.exe --play             start straight away with the saved settings
//   ClearSkies.exe --preset <name>    choose a preset (saved), e.g. --preset filmic --play
//   ClearSkies.exe --game <path>      choose savagewind.exe (saved)
//   ClearSkies.exe ... -- <args>      everything after -- goes to the game (e.g. -- --config)
#define WIN32_LEAN_AND_MEAN
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <uxtheme.h>

#include <stdint.h>
#include <stdio.h>
#include <wchar.h>

#include "cemu_presets.h"

#define LAYER_NAME L"VK_LAYER_CLEARSKIES_savage_wind"
#define APP_TITLE L"Clear Skies"
#define APP_VERSION L"0.1.0"

static wchar_t g_dir[MAX_PATH];  // the launcher's folder (with the layer DLL, its manifest and clearskies.ini)
static wchar_t g_ini[MAX_PATH];
static wchar_t g_game[MAX_PATH];
static wchar_t g_preset[64] = L"serfrost";
static wchar_t g_game_args[4096];

// ------------------------------------------------------------------ settings (clearskies.ini)

static void load_ini(void) {
    wchar_t v[MAX_PATH];
    GetPrivateProfileStringW(L"launcher", L"game", L"", v, MAX_PATH, g_ini);
    if (v[0]) wcscpy(g_game, v);
    GetPrivateProfileStringW(L"clarity", L"preset", L"serfrost", v, MAX_PATH, g_ini);
    wchar_t *c = wcspbrk(v, L";#");
    if (c) *c = 0;
    for (size_t n = wcslen(v); n && (v[n - 1] == L' ' || v[n - 1] == L'\t'); n--) v[n - 1] = 0;
    if (v[0]) wcsncpy(g_preset, v, 63);
}

static void save_ini(void) {
    WritePrivateProfileStringW(L"clarity", L"preset", g_preset, g_ini);
    WritePrivateProfileStringW(L"launcher", L"game", g_game, g_ini);
}

static int file_exists(const wchar_t *p) {
    DWORD a = GetFileAttributesW(p);
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

// savagewind.exe next to the launcher, or one folder up, when none is saved yet
static void guess_game(void) {
    if (g_game[0] && file_exists(g_game)) return;
    wchar_t p[MAX_PATH], full[MAX_PATH];
    swprintf(p, MAX_PATH, L"%lssavagewind.exe", g_dir);
    if (file_exists(p)) { wcscpy(g_game, p); return; }
    swprintf(p, MAX_PATH, L"%ls..\\savagewind.exe", g_dir);
    if (GetFullPathNameW(p, MAX_PATH, full, NULL) && file_exists(full)) wcscpy(g_game, full);
}

// ------------------------------------------------------------------ starting the game

static void prepend_env(const wchar_t *name, const wchar_t *value, const wchar_t *sep) {
    wchar_t old[8192] = L"", buf[8192 + MAX_PATH];
    DWORD n = GetEnvironmentVariableW(name, old, 8192);
    if (n && n < 8192 && wcsstr(old, value)) return;  // (already there)
    if (n && n < 8192) swprintf(buf, sizeof buf / sizeof buf[0], L"%ls%ls%ls", value, sep, old);
    else wcscpy(buf, value);
    SetEnvironmentVariableW(name, buf);
}

static int start_game(HWND owner) {
    if (!file_exists(g_game)) {
        MessageBoxW(owner, L"Choose savagewind.exe first (Browse).", APP_TITLE, MB_ICONWARNING);
        return 0;
    }
    wchar_t layer[MAX_PATH], manifest[MAX_PATH];
    swprintf(layer, MAX_PATH, L"%lsclearskies_layer.dll", g_dir);
    swprintf(manifest, MAX_PATH, L"%lsVkLayer_clearskies.json", g_dir);
    if (!file_exists(layer) || !file_exists(manifest)) {
        MessageBoxW(owner, L"clearskies_layer.dll or VkLayer_clearskies.json is missing from Clear Skies' folder. "
                           L"Keep all of Clear Skies' files together.", APP_TITLE, MB_ICONERROR);
        return 0;
    }
    wchar_t gamedir[MAX_PATH], shaderc[MAX_PATH];
    wcscpy(gamedir, g_game);
    wchar_t *s = wcsrchr(gamedir, L'\\');
    if (s) s[1] = 0;
    swprintf(shaderc, MAX_PATH, L"%lslibshaderc_shared.dll", gamedir);
    if (!file_exists(shaderc) &&
        MessageBoxW(owner, L"libshaderc_shared.dll isn't next to savagewind.exe, and Clear Skies uses it to build its "
                           L"shader. The game will start with its own look.\n\nStart anyway?",
                    APP_TITLE, MB_ICONWARNING | MB_YESNO) != IDYES)
        return 0;

    // Savage Wind opens its own setup screen until the game files are copied and the game code built (extracted\
    // romfs and extracted\swgame.dll in its data folder: the game's folder unless SAVAGEWIND_DATA_ROOT moves it).
    wchar_t game_code[MAX_PATH], romfs[MAX_PATH];
    swprintf(game_code, MAX_PATH, L"%lsextracted\\swgame.dll", gamedir);
    swprintf(romfs, MAX_PATH, L"%lsextracted\\romfs", gamedir);
    DWORD ra = GetFileAttributesW(romfs);
    if (!GetEnvironmentVariableW(L"SAVAGEWIND_DATA_ROOT", NULL, 0) &&
        (!file_exists(game_code) || ra == INVALID_FILE_ATTRIBUTES || !(ra & FILE_ATTRIBUTE_DIRECTORY)) &&
        MessageBoxW(owner, L"This copy of Savage Wind hasn't finished setting up its game files yet, so it will open "
                           L"its own setup screen first. Clarity applies once the game itself is running.\n\n"
                           L"Start anyway?", APP_TITLE, MB_ICONINFORMATION | MB_YESNO) != IDYES)
        return 0;

    // The Vulkan loader finds the layer's manifest in g_dir and turns the layer on, for this game process only.
    wchar_t dir[MAX_PATH];
    wcscpy(dir, g_dir);
    size_t n = wcslen(dir);
    if (n > 3 && dir[n - 1] == L'\\') dir[n - 1] = 0;
    prepend_env(L"VK_ADD_LAYER_PATH", dir, L";");  // (Vulkan loader 1.3.234 and newer)
    prepend_env(L"VK_LAYER_PATH", dir, L";");      // (older loaders)
    prepend_env(L"VK_INSTANCE_LAYERS", LAYER_NAME, L";");
    SetEnvironmentVariableW(L"CLEARSKIES_INI", g_ini);

    wchar_t cmd[8192];
    swprintf(cmd, sizeof cmd / sizeof cmd[0], L"\"%ls\"%ls%ls", g_game, g_game_args[0] ? L" " : L"", g_game_args);
    STARTUPINFOW si = {.cb = sizeof si};
    PROCESS_INFORMATION pi;
    if (!CreateProcessW(g_game, cmd, NULL, NULL, FALSE, 0, NULL, gamedir, &si, &pi)) {
        wchar_t msg[512];
        swprintf(msg, 512, L"Couldn't start %ls (error %lu).", g_game, GetLastError());
        MessageBoxW(owner, msg, APP_TITLE, MB_ICONERROR);
        return 0;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return 1;
}

// ------------------------------------------------------------------ presets

// Combo box row -> preset id: row 0 is Off, then Cemu's presets in Cemu's order.
static const wchar_t *row_id(int row, wchar_t *buf) {
    if (row <= 0) return L"off";
    MultiByteToWideChar(CP_UTF8, 0, k_presets[row - 1].id, -1, buf, 64);
    return buf;
}

static int preset_row(const wchar_t *id) {
    if (!_wcsicmp(id, L"off")) return 0;
    if (!_wcsicmp(id, L"recommended")) id = L"serfrost";
    if (!_wcsicmp(id, L"dim")) id = L"serfrost_dim";
    if (!_wcsicmp(id, L"bright")) id = L"serfrost_bright";
    wchar_t buf[64];
    for (int i = 0; i < CLARITY_NPRESETS; i++)
        if (!_wcsicmp(id, row_id(i + 1, buf))) return i + 1;
    return -1;
}

// ------------------------------------------------------------------ look

// Colours: a clear sky over a dusky slate body.
#define C_SKY_TOP RGB(0x2b, 0x7f, 0xd4)
#define C_SKY_LOW RGB(0xa8, 0xd8, 0xf6)
#define C_SUN RGB(0xff, 0xf1, 0xc2)
#define C_SUN_GLOW RGB(0xd8, 0xec, 0xf6)
#define C_CLOUD RGB(0xf7, 0xfb, 0xff)
#define C_BG RGB(0x10, 0x16, 0x1e)
#define C_PANEL RGB(0x19, 0x22, 0x2d)
#define C_BORDER RGB(0x2c, 0x3a, 0x48)
#define C_TEXT RGB(0xe6, 0xee, 0xf5)
#define C_MUTED RGB(0x8a, 0x9d, 0xb0)
#define C_FAINT RGB(0x5f, 0x72, 0x85)
#define C_ACCENT RGB(0x4f, 0xb0, 0xf0)
#define C_ACCENT_HOT RGB(0x74, 0xc4, 0xf7)
#define C_ACCENT_TEXT RGB(0x05, 0x16, 0x26)

enum { ID_PRESET = 100, ID_GAME, ID_BROWSE, ID_PLAY, ID_EDIT, ID_LICENSES };
static HWND g_wnd, g_combo, g_edit, g_hot;
static HFONT g_f_title, g_f_sub, g_f_body, g_f_small, g_f_label, g_f_button;
static HBRUSH g_b_bg, g_b_panel;
static int g_dpi = 96;
#define S(x) MulDiv((x), g_dpi, 96)

// Layout (in 96-dpi units)
#define W_CLIENT 560
#define H_CLIENT 470
#define H_HEADER 132
#define PAD 28

static HFONT font(int pt, int weight, const wchar_t *face) {
    return CreateFontW(-MulDiv(pt, g_dpi, 72), 0, 0, 0, weight, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, face);
}

static void fill(HDC dc, int x, int y, int w, int h, COLORREF c) {
    RECT r = {x, y, x + w, y + h};
    HBRUSH b = CreateSolidBrush(c);
    FillRect(dc, &r, b);
    DeleteObject(b);
}

static void round_rect(HDC dc, RECT r, int radius, COLORREF fillc, COLORREF edge) {
    HBRUSH b = CreateSolidBrush(fillc);
    HPEN p = CreatePen(PS_SOLID, 1, edge);
    HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, p);
    RoundRect(dc, r.left, r.top, r.right, r.bottom, radius, radius);
    SelectObject(dc, ob);
    SelectObject(dc, op);
    DeleteObject(b);
    DeleteObject(p);
}

static void ellipse(HDC dc, int cx, int cy, int rx, int ry, COLORREF c) {
    HBRUSH b = CreateSolidBrush(c);
    HPEN p = CreatePen(PS_SOLID, 1, c);
    HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, p);
    Ellipse(dc, cx - rx, cy - ry, cx + rx, cy + ry);
    SelectObject(dc, ob);
    SelectObject(dc, op);
    DeleteObject(b);
    DeleteObject(p);
}

static COLORREF mix(COLORREF a, COLORREF b, int t, int n) {
    return RGB(GetRValue(a) + (GetRValue(b) - GetRValue(a)) * t / n, GetGValue(a) + (GetGValue(b) - GetGValue(a)) * t / n,
               GetBValue(a) + (GetBValue(b) - GetBValue(a)) * t / n);
}

static void text(HDC dc, const wchar_t *s, int x, int y, int w, int h, HFONT f, COLORREF c, UINT flags) {
    RECT r = {x, y, x + w, y + h};
    SelectObject(dc, f);
    SetTextColor(dc, c);
    SetBkMode(dc, TRANSPARENT);
    DrawTextW(dc, s, -1, &r, flags | DT_NOPREFIX);
}

static void cloud(HDC dc, int x, int y, int s, COLORREF c) {  // a soft little cumulus, s = its size
    ellipse(dc, x, y, s, s * 6 / 10, c);
    ellipse(dc, x + s * 9 / 10, y - s * 3 / 10, s * 9 / 10, s * 8 / 10, c);
    ellipse(dc, x + s * 19 / 10, y, s * 9 / 10, s * 6 / 10, c);
    fill(dc, x, y, s * 19 / 10, s * 6 / 10, c);
}

static void paint(HWND w, HDC dc) {
    RECT rc;
    GetClientRect(w, &rc);
    int cw = rc.right, hh = S(H_HEADER);

    // Sky, drawn at 3x and scaled down so the sun and clouds have smooth edges (GDI shapes aren't anti-aliased)
    {
        const int k = 3, sw = cw * k, sh = hh * k;
        HDC sky = CreateCompatibleDC(dc);
        BITMAPINFO bi = {.bmiHeader = {.biSize = sizeof bi.bmiHeader, .biWidth = sw, .biHeight = -sh, .biPlanes = 1,
                                       .biBitCount = 32, .biCompression = BI_RGB}};
        uint32_t *px = NULL;
        HBITMAP bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, (void **)&px, NULL, 0);
        HGDIOBJ old = SelectObject(sky, bmp);
        int sx = (cw - S(92)) * k, sy = S(50) * k, sun = S(26) * k, glow = S(80) * k;
        COLORREF warm = mix(C_SUN, C_SUN_GLOW, 1, 2);
        for (int y = 0; px && y < sh; y++) {  // the sky's gradient, and the sun's glow fading smoothly into it
            COLORREF base = mix(C_SKY_TOP, C_SKY_LOW, y, sh);
            for (int x = 0; x < sw; x++) {
                int dx = x - sx, dy = y - sy;
                int d2 = dx * dx + dy * dy, t = 0;  // t: glow strength 0..1024
                if (d2 < glow * glow) {
                    int f = 1024 - (int)((long long)d2 * 1024 / ((long long)glow * glow));  // 1 at the sun .. 0
                    t = f * f / 1024 * 7 / 10;
                }
                COLORREF c = mix(base, warm, t, 1024);
                px[y * sw + x] = (GetRValue(c) << 16) | (GetGValue(c) << 8) | GetBValue(c);
            }
        }
        GdiFlush();
        ellipse(sky, sx, sy, sun, sun, C_SUN);
        cloud(sky, (cw - S(190)) * k, S(96) * k, S(22) * k, mix(C_CLOUD, C_SKY_LOW, 1, 4));
        cloud(sky, (cw - S(120)) * k, S(110) * k, S(30) * k, C_CLOUD);
        SetStretchBltMode(dc, HALFTONE);
        SetBrushOrgEx(dc, 0, 0, NULL);
        StretchBlt(dc, 0, 0, cw, hh, sky, 0, 0, sw, sh, SRCCOPY);
        SelectObject(sky, old);
        DeleteObject(bmp);
        DeleteDC(sky);
    }
    text(dc, APP_TITLE, S(PAD), S(26), S(320), S(48), g_f_title, RGB(255, 255, 255), DT_LEFT | DT_SINGLELINE);
    text(dc, L"Cemu's Clarity presets for Savage Wind", S(PAD + 2), S(76), S(360), S(24), g_f_sub,
         RGB(0xf2, 0xf8, 0xfd), DT_LEFT | DT_SINGLELINE);
    fill(dc, 0, hh, cw, 1, C_BORDER);

    // Labels
    int y = hh + S(22);
    text(dc, L"LOOK", S(PAD), y, S(200), S(18), g_f_label, C_MUTED, DT_LEFT | DT_SINGLELINE);
    int row = g_combo ? (int)SendMessageW(g_combo, CB_GETCURSEL, 0, 0) : -1;
    wchar_t desc[256], id[64];
    if (row <= 0) wcscpy(desc, L"The game's own tone mapping, unchanged.");
    else swprintf(desc, 256, L"Cemu preset %d  \x00B7  clearskies.ini: preset = %ls", k_presets[row - 1].cemu_number, row_id(row, id));
    text(dc, desc, S(PAD), y + S(62), cw - S(2 * PAD), S(20), g_f_small, C_FAINT, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    y += S(98);
    text(dc, L"SAVAGE WIND", S(PAD), y, S(200), S(18), g_f_label, C_MUTED, DT_LEFT | DT_SINGLELINE);
    // the path box's frame
    RECT er = {S(PAD), y + S(24), cw - S(PAD) - S(108), y + S(24) + S(36)};
    round_rect(dc, er, S(8), C_PANEL, C_BORDER);

    // Footer
    int fy = rc.bottom - S(62);
    fill(dc, 0, fy, cw, 1, C_BORDER);
    text(dc, L"A mod for Savage Wind. Not made by or affiliated with the Savage Wind team, Cemu or Nintendo.",
         S(PAD), fy + S(12), cw - S(2 * PAD) - S(140), S(40), g_f_small, C_FAINT, DT_LEFT | DT_WORDBREAK);
}

static void draw_button(const DRAWITEMSTRUCT *d) {
    int primary = d->CtlID == ID_PLAY, link = d->CtlID == ID_LICENSES;
    int pressed = d->itemState & ODS_SELECTED, hot = d->hwndItem == g_hot;
    HDC dc = d->hDC;
    RECT r = d->rcItem;
    fill(dc, r.left, r.top, r.right - r.left, r.bottom - r.top, C_BG);
    wchar_t t[64];
    GetWindowTextW(d->hwndItem, t, 64);
    if (link) {
        text(dc, t, r.left, r.top, r.right - r.left, r.bottom - r.top, g_f_small, hot ? C_ACCENT_HOT : C_ACCENT,
             DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    } else {
        COLORREF f = primary ? (pressed ? C_ACCENT : hot ? C_ACCENT_HOT : C_ACCENT) : (pressed ? C_BG : hot ? RGB(0x23, 0x2e, 0x3b) : C_PANEL);
        round_rect(dc, r, S(10), f, primary ? f : (hot ? C_ACCENT : C_BORDER));
        text(dc, t, r.left, r.top + (pressed ? 1 : 0), r.right - r.left, r.bottom - r.top, g_f_button,
             primary ? C_ACCENT_TEXT : C_TEXT, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    if ((d->itemState & ODS_FOCUS) && !(d->itemState & ODS_NOFOCUSRECT)) {
        RECT fr = r;
        InflateRect(&fr, -S(3), -S(3));
        DrawFocusRect(dc, &fr);
    }
}

// Hover for the owner-drawn buttons
static LRESULT CALLBACK button_proc(HWND b, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR id, DWORD_PTR data) {
    (void)id; (void)data;
    if (msg == WM_MOUSEMOVE && g_hot != b) {
        HWND old = g_hot;
        g_hot = b;
        if (old) InvalidateRect(old, NULL, FALSE);
        InvalidateRect(b, NULL, FALSE);
        TRACKMOUSEEVENT t = {.cbSize = sizeof t, .dwFlags = TME_LEAVE, .hwndTrack = b};
        TrackMouseEvent(&t);
    } else if (msg == WM_MOUSELEAVE && g_hot == b) {
        g_hot = NULL;
        InvalidateRect(b, NULL, FALSE);
    } else if (msg == WM_SETCURSOR) {
        SetCursor(LoadCursorW(NULL, (LPCWSTR)IDC_HAND));
        return TRUE;
    }
    return DefSubclassProc(b, msg, wp, lp);
}

static HWND make(const wchar_t *cls, const wchar_t *t, DWORD style, int x, int y, int w, int h, int id, HFONT f) {
    HWND c = CreateWindowExW(0, cls, t, WS_CHILD | WS_VISIBLE | style, x, y, w, h, g_wnd, (HMENU)(INT_PTR)id,
                             GetModuleHandleW(NULL), NULL);
    SendMessageW(c, WM_SETFONT, (WPARAM)f, TRUE);
    if (style & BS_OWNERDRAW) SetWindowSubclass(c, button_proc, 1, 0);
    return c;
}

static void read_controls(void) {
    GetWindowTextW(g_edit, g_game, MAX_PATH);
    int row = (int)SendMessageW(g_combo, CB_GETCURSEL, 0, 0);
    wchar_t id[64];
    wcsncpy(g_preset, row_id(row, id), 63);
}

static void show_licenses(HWND w) {
    wchar_t p[MAX_PATH];
    swprintf(p, MAX_PATH, L"%lsLICENSES.txt", g_dir);
    if (file_exists(p)) { ShellExecuteW(w, L"open", p, NULL, NULL, SW_SHOWNORMAL); return; }
    MessageBoxW(w,
        L"Clear Skies " APP_VERSION L", a mod for Savage Wind.\n\n"
        L"Clear Skies' code: MIT License.\n"
        L"Clarity shader and presets: from Cemu's graphic packs (cemu-project/cemu_graphic_packs), CC0 1.0. "
        L"Clarity GFX by Jamie and Kiri with Crementif; presets by Serfrost, Bruz, t3nka, CellHunter, The Complaining "
        L"Gamer, Visk, Silentverge, Godot, Hexaae, MaranBr, YeMaoWuXin and Cobalt.\n"
        L"Savage Wind Recomp (MIT) is not included: Clear Skies works with your own copy.\n\n"
        L"Not made by or affiliated with the Savage Wind team, the Cemu team or Nintendo. The Legend of Zelda: Breath "
        L"of the Wild is \x00A9 Nintendo.",
        L"Licenses & credits", MB_ICONINFORMATION);
}

static LRESULT CALLBACK wndproc(HWND w, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        g_wnd = w;
        BOOL dark = TRUE;
        DwmSetWindowAttribute(w, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dark, sizeof dark);
        COLORREF cap = C_SKY_TOP;
        DwmSetWindowAttribute(w, 35 /* DWMWA_CAPTION_COLOR (Windows 11) */, &cap, sizeof cap);
        g_f_title = font(28, FW_SEMIBOLD, L"Segoe UI Variable Display");
        g_f_sub = font(11, FW_NORMAL, L"Segoe UI");
        g_f_body = font(10, FW_NORMAL, L"Segoe UI");
        g_f_small = font(9, FW_NORMAL, L"Segoe UI");
        g_f_label = font(8, FW_BOLD, L"Segoe UI");
        g_f_button = font(10, FW_SEMIBOLD, L"Segoe UI");
        g_b_bg = CreateSolidBrush(C_BG);
        g_b_panel = CreateSolidBrush(C_PANEL);

        int cw = S(W_CLIENT), y = S(H_HEADER) + S(22);
        g_combo = make(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, S(PAD), y + S(22), cw - S(2 * PAD), S(400), ID_PRESET, g_f_body);
        SetWindowTheme(g_combo, L"DarkMode_CFD", NULL);
        SendMessageW(g_combo, CB_SETITEMHEIGHT, (WPARAM)-1, S(26));
        SendMessageW(g_combo, CB_ADDSTRING, 0, (LPARAM)L"Off (the game's own look)");
        for (int i = 0; i < CLARITY_NPRESETS; i++) {
            wchar_t t[160];
            MultiByteToWideChar(CP_UTF8, 0, k_presets[i].title, -1, t, 160);
            SendMessageW(g_combo, CB_ADDSTRING, 0, (LPARAM)t);
        }
        int row = preset_row(g_preset);
        SendMessageW(g_combo, CB_SETCURSEL, row < 0 ? preset_row(L"serfrost") : row, 0);

        y += S(98);
        g_edit = make(L"EDIT", g_game, ES_AUTOHSCROLL | WS_TABSTOP, S(PAD) + S(12), y + S(24) + S(9), cw - S(2 * PAD) - S(108) - S(24), S(20), ID_GAME, g_f_body);
        SendMessageW(g_edit, EM_SETCUEBANNER, TRUE, (LPARAM)L"Where is savagewind.exe?");
        make(L"BUTTON", L"Browse", BS_OWNERDRAW | WS_TABSTOP, cw - S(PAD) - S(96), y + S(24), S(96), S(36), ID_BROWSE, g_f_button);

        y += S(84);
        make(L"BUTTON", L"Edit settings", BS_OWNERDRAW | WS_TABSTOP, S(PAD), y, S(140), S(44), ID_EDIT, g_f_button);
        make(L"BUTTON", L"Play", BS_OWNERDRAW | WS_TABSTOP, cw - S(PAD) - S(160), y, S(160), S(44), ID_PLAY, g_f_button);

        RECT rc;
        GetClientRect(w, &rc);
        make(L"BUTTON", L"Licenses & credits", BS_OWNERDRAW | WS_TABSTOP, cw - S(PAD) - S(140), rc.bottom - S(62) + S(10), S(140), S(24), ID_LICENSES, g_f_small);
        SetFocus(GetDlgItem(w, ID_PLAY));
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(w, &ps);
        RECT rc;
        GetClientRect(w, &rc);
        HDC mem = CreateCompatibleDC(dc);
        HBITMAP bmp = CreateCompatibleBitmap(dc, rc.right, rc.bottom);
        HGDIOBJ old = SelectObject(mem, bmp);
        FillRect(mem, &rc, g_b_bg);
        paint(w, mem);
        BitBlt(dc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
        SelectObject(mem, old);
        DeleteObject(bmp);
        DeleteDC(mem);
        EndPaint(w, &ps);
        return 0;
    }
    case WM_CTLCOLOREDIT:
        SetTextColor((HDC)wp, C_TEXT);
        SetBkColor((HDC)wp, C_PANEL);
        return (LRESULT)g_b_panel;
    case WM_CTLCOLORLISTBOX:
        SetTextColor((HDC)wp, C_TEXT);
        SetBkColor((HDC)wp, C_PANEL);
        return (LRESULT)g_b_panel;
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
        SetBkColor((HDC)wp, C_BG);
        return (LRESULT)g_b_bg;
    case WM_DRAWITEM:
        draw_button((const DRAWITEMSTRUCT *)lp);
        return TRUE;
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case ID_PRESET:
            if (HIWORD(wp) == CBN_SELCHANGE) {
                RECT r;
                GetClientRect(w, &r);
                r.top = S(H_HEADER) + S(80), r.bottom = S(H_HEADER) + S(110);
                InvalidateRect(w, &r, FALSE);
            }
            return 0;
        case ID_BROWSE: {
            wchar_t file[MAX_PATH];
            GetWindowTextW(g_edit, file, MAX_PATH);
            OPENFILENAMEW of = {.lStructSize = sizeof of, .hwndOwner = w, .lpstrFile = file, .nMaxFile = MAX_PATH,
                                .lpstrFilter = L"Savage Wind (savagewind.exe)\0savagewind.exe\0Programs (*.exe)\0*.exe\0",
                                .lpstrTitle = L"Where is savagewind.exe?",
                                .Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR};
            if (GetOpenFileNameW(&of)) SetWindowTextW(g_edit, file);
            return 0;
        }
        case ID_EDIT:
            read_controls();
            save_ini();
            ShellExecuteW(w, L"open", L"notepad.exe", g_ini, NULL, SW_SHOWNORMAL);
            return 0;
        case ID_LICENSES:
            show_licenses(w);
            return 0;
        case ID_PLAY:
        case IDOK:
            read_controls();
            save_ini();
            if (start_game(w)) DestroyWindow(w);
            return 0;
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(w, msg, wp, lp);
}

// ------------------------------------------------------------------ main

static void append_arg(wchar_t *dst, size_t cap, const wchar_t *a) {
    size_t n = wcslen(dst);
    int quote = !a[0] || wcspbrk(a, L" \t\"") != NULL;
    swprintf(dst + n, cap - n, L"%ls%ls%ls%ls", n ? L" " : L"", quote ? L"\"" : L"", a, quote ? L"\"" : L"");
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE prev, PWSTR cmdline, int show) {
    (void)prev; (void)cmdline;
    SetProcessDPIAware();
    DWORD n = GetModuleFileNameW(NULL, g_dir, MAX_PATH);
    if (!n || n >= MAX_PATH) return 1;
    wchar_t *s = wcsrchr(g_dir, L'\\');
    if (s) s[1] = 0;
    swprintf(g_ini, MAX_PATH, L"%lsclearskies.ini", g_dir);
    load_ini();

    int argc, play = 0;
    wchar_t **argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    for (int i = 1; argv && i < argc; i++) {
        if (!wcscmp(argv[i], L"--")) {
            for (i++; i < argc; i++) append_arg(g_game_args, sizeof g_game_args / sizeof g_game_args[0], argv[i]);
            break;
        } else if (!wcscmp(argv[i], L"--play")) play = 1;
        else if (!wcscmp(argv[i], L"--preset") && i + 1 < argc) {
            if (preset_row(argv[++i]) < 0) {
                wchar_t msg[256];
                swprintf(msg, 256, L"Unknown preset \"%ls\". The names are listed in clearskies.ini.", argv[i]);
                MessageBoxW(NULL, msg, APP_TITLE, MB_ICONWARNING);
            } else wcsncpy(g_preset, argv[i], 63);
        } else if (!wcscmp(argv[i], L"--game") && i + 1 < argc) {
            GetFullPathNameW(argv[++i], MAX_PATH, g_game, NULL);
        }
    }
    if (argv) LocalFree(argv);
    guess_game();

    if (play) {
        save_ini();
        return start_game(NULL) ? 0 : 1;
    }

    HDC sdc = GetDC(NULL);
    g_dpi = GetDeviceCaps(sdc, LOGPIXELSY);
    ReleaseDC(NULL, sdc);
    WNDCLASSW wc = {.lpfnWndProc = wndproc, .hInstance = inst, .lpszClassName = L"ClearSkiesLauncher",
                    .hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW), .hbrBackground = NULL,
                    .hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1))};
    if (!wc.hIcon) wc.hIcon = LoadIconW(NULL, (LPCWSTR)IDI_APPLICATION);
    RegisterClassW(&wc);
    RECT r = {0, 0, S(W_CLIENT), S(H_CLIENT)};
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    AdjustWindowRect(&r, style, FALSE);
    HWND w = CreateWindowExW(0, wc.lpszClassName, APP_TITLE, style, CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left,
                             r.bottom - r.top, NULL, NULL, inst, NULL);
    ShowWindow(w, show);
    MSG m;
    while (GetMessageW(&m, NULL, 0, 0) > 0) {
        if (IsDialogMessageW(w, &m)) continue;
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    return 0;
}
