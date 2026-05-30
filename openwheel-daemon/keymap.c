// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 OpenWheel Contributors

#include "keymap.h"

#include <linux/input-event-codes.h>
#include <strings.h>
#include <stddef.h>

typedef struct {
    const char *name;
    int code;
} KeyEntry;

// Canonical entries. Synonyms are added below as separate entries
// (e.g. "[" -> bracketleft) so the same code can have multiple names.
// The first entry for any given code is what `keymap_name_at` returns.
static const KeyEntry kKeys[] = {
    // Letters
    {"a", KEY_A}, {"b", KEY_B}, {"c", KEY_C}, {"d", KEY_D}, {"e", KEY_E},
    {"f", KEY_F}, {"g", KEY_G}, {"h", KEY_H}, {"i", KEY_I}, {"j", KEY_J},
    {"k", KEY_K}, {"l", KEY_L}, {"m", KEY_M}, {"n", KEY_N}, {"o", KEY_O},
    {"p", KEY_P}, {"q", KEY_Q}, {"r", KEY_R}, {"s", KEY_S}, {"t", KEY_T},
    {"u", KEY_U}, {"v", KEY_V}, {"w", KEY_W}, {"x", KEY_X}, {"y", KEY_Y},
    {"z", KEY_Z},

    // Top-row digits
    {"0", KEY_0}, {"1", KEY_1}, {"2", KEY_2}, {"3", KEY_3}, {"4", KEY_4},
    {"5", KEY_5}, {"6", KEY_6}, {"7", KEY_7}, {"8", KEY_8}, {"9", KEY_9},

    // Function keys
    {"f1", KEY_F1}, {"f2", KEY_F2}, {"f3", KEY_F3}, {"f4", KEY_F4},
    {"f5", KEY_F5}, {"f6", KEY_F6}, {"f7", KEY_F7}, {"f8", KEY_F8},
    {"f9", KEY_F9}, {"f10", KEY_F10}, {"f11", KEY_F11}, {"f12", KEY_F12},

    // Navigation
    {"left",     KEY_LEFT},
    {"right",    KEY_RIGHT},
    {"up",       KEY_UP},
    {"down",     KEY_DOWN},
    {"home",     KEY_HOME},
    {"end",      KEY_END},
    {"pageup",   KEY_PAGEUP},
    {"pagedown", KEY_PAGEDOWN},
    {"insert",   KEY_INSERT},
    {"delete",   KEY_DELETE},

    // Editing / whitespace
    {"escape",    KEY_ESC},
    {"esc",       KEY_ESC},
    {"tab",       KEY_TAB},
    {"return",    KEY_ENTER},
    {"enter",     KEY_ENTER},
    {"backspace", KEY_BACKSPACE},
    {"space",     KEY_SPACE},
    {" ",         KEY_SPACE},

    // Punctuation (canonical names mirror X11 keysym names so existing
    // profile JSON keeps working unchanged).
    {"minus",        KEY_MINUS},
    {"-",            KEY_MINUS},
    {"equal",        KEY_EQUAL},
    {"=",            KEY_EQUAL},
    {"plus",         KEY_EQUAL},  // requires Shift; users typically pair with Shift modifier
    {"+",            KEY_EQUAL},
    {"bracketleft",  KEY_LEFTBRACE},
    {"[",            KEY_LEFTBRACE},
    {"bracketright", KEY_RIGHTBRACE},
    {"]",            KEY_RIGHTBRACE},
    {"semicolon",    KEY_SEMICOLON},
    {";",            KEY_SEMICOLON},
    {"apostrophe",   KEY_APOSTROPHE},
    {"'",            KEY_APOSTROPHE},
    {"grave",        KEY_GRAVE},
    {"`",            KEY_GRAVE},
    {"backslash",    KEY_BACKSLASH},
    {"\\",           KEY_BACKSLASH},
    {"comma",        KEY_COMMA},
    {",",            KEY_COMMA},
    {"period",       KEY_DOT},
    {".",            KEY_DOT},
    {"slash",        KEY_SLASH},
    {"/",            KEY_SLASH},

    // Modifier keys (rarely used as primary key, but valid)
    {"control_l", KEY_LEFTCTRL},
    {"control_r", KEY_RIGHTCTRL},
    {"shift_l",   KEY_LEFTSHIFT},
    {"shift_r",   KEY_RIGHTSHIFT},
    {"alt_l",     KEY_LEFTALT},
    {"alt_r",     KEY_RIGHTALT},
    {"super_l",   KEY_LEFTMETA},
    {"super_r",   KEY_RIGHTMETA},
    {"super",     KEY_LEFTMETA},   // alias — matches "super" key name from profile JSON
    {"meta",      KEY_LEFTMETA},   // alias — Qt calls the same key "Meta"

    // XF86 media keys (names match the strings used in profile JSON files
    // and inherited from X11 keysym conventions).
    {"XF86AudioRaiseVolume", KEY_VOLUMEUP},
    {"XF86AudioLowerVolume", KEY_VOLUMEDOWN},
    {"XF86AudioMute",        KEY_MUTE},
    {"XF86AudioPlay",        KEY_PLAYPAUSE},
    {"XF86AudioPause",       KEY_PLAYPAUSE},
    {"XF86AudioStop",        KEY_STOPCD},
    {"XF86AudioNext",        KEY_NEXTSONG},
    {"XF86AudioPrev",        KEY_PREVIOUSSONG},
    {"XF86MonBrightnessUp",   KEY_BRIGHTNESSUP},
    {"XF86MonBrightnessDown", KEY_BRIGHTNESSDOWN},
};

static const size_t kKeyCount = sizeof(kKeys) / sizeof(kKeys[0]);

int keymap_lookup(const char *name)
{
    if (name == NULL || name[0] == '\0') {
        return -1;
    }
    for (size_t i = 0; i < kKeyCount; ++i) {
        if (strcasecmp(kKeys[i].name, name) == 0) {
            return kKeys[i].code;
        }
    }
    return -1;
}

size_t keymap_count(void)
{
    return kKeyCount;
}

const char *keymap_name_at(size_t i)
{
    if (i >= kKeyCount) {
        return NULL;
    }
    return kKeys[i].name;
}

int keymap_code_at(size_t i)
{
    if (i >= kKeyCount) {
        return -1;
    }
    return kKeys[i].code;
}
