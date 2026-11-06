// The punctuation keys (platform/keymap.h): each must have a code, or the key press is dropped
// before the game sees it ('/' explore, '.' airbase, '[' ']' terraforming, ',' '.' in the base
// window, and these characters in typed names).
#include "platform/keymap.h"
#include "unit.h"

using platform::oem_vk;

TEST_CASE("keymap: every punctuation key has its code") {
    CHECK(oem_vk('/') == 0xBF);
    CHECK(oem_vk('.') == 0xBE);
    CHECK(oem_vk(',') == 0xBC);
    CHECK(oem_vk(';') == 0xBA);
    CHECK(oem_vk('\'') == 0xDE);
    CHECK(oem_vk('[') == 0xDB);
    CHECK(oem_vk(']') == 0xDD);
    CHECK(oem_vk('\\') == 0xDC);
    CHECK(oem_vk('`') == 0xC0);
}

TEST_CASE("keymap: other keys are not punctuation") {
    const int32_t others[] = {0, 'a', 'z', '0', '9', ' ', '=', '-', 0x40000000};
    for (int32_t k : others) CHECK(oem_vk(k) == 0);
    // Shifted characters do not arrive as keycodes: the key's own character with Shift held.
    const int32_t shifted[] = {'?', '<', '>', '{', '}', '|', '"', ':', '~'};
    for (int32_t k : shifted) CHECK(oem_vk(k) == 0);
}
