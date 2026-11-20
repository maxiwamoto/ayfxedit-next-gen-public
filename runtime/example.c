#include "ayfx_player.h"
#include "effects.h"
AyfxPlayer ayfx;
void game_audio_init(void) { ayfx_init(&ayfx, ayfx_read_gpio()); }
uint8_t game_sound(uint8_t id) {
    uint16_t offset;
    if (id >= AYFX_EFFECT_COUNT) return 0;
    offset = ayfx_bank[id * 2] | ((uint16_t)ayfx_bank[id * 2 + 1] << 8);
    return ayfx_start(&ayfx, ayfx_bank + offset);
}
/* Call once per exported tick from the game's existing audio service.
   Pass the music player's 14-register shadow, or NULL for no music. */
void game_audio_frame(const uint8_t *music) {
    ayfx_tick(&ayfx);
    ayfx_mix(&ayfx, music);
    ayfx_write(&ayfx);
}
