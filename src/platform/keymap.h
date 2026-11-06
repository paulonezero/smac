// The punctuation keys' Windows virtual-key codes (VK_OEM_*), without SDL so that the unit tests
// can check them. win.cpp's vk_char turns these codes back into characters (WM_CHAR).
#pragma once
#include <cstdint>

namespace platform {

// The code of the key whose unshifted character is `key` (0: not a punctuation key). A backend's
// keycode for such a key is that character (SDL2: SDLK_SLASH is '/', and Shift does not change
// it, so '?' arrives as '/' with Shift held). '=' and '-' have VKEY_OEM_PLUS / VKEY_OEM_MINUS.
constexpr int32_t oem_vk(int32_t key) {
    switch (key) {
    case ';': return 0xBA;
    case ',': return 0xBC;
    case '.': return 0xBE;
    case '/': return 0xBF;
    case '`': return 0xC0;
    case '[': return 0xDB;
    case '\\': return 0xDC;
    case ']': return 0xDD;
    case '\'': return 0xDE;
    default: return 0;
    }
}

}  // namespace platform
