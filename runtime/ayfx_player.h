#ifndef AYFX_PLAYER_H
#define AYFX_PLAYER_H
/* AYFXEDIT NEXT GEN compact v2. Call start/tick/mix/write from one serialized owner.
   The caller owns the interrupt cadence. No BIOS calls or interrupt changes.
   One prioritized 1/2/3-channel effect overlays the music register image.
   Music and effect must use the same AY clock and exported tick rate.
   Register 13 = 255 means "do not retrigger". */
#include <stdint.h>
typedef struct {
 const uint8_t *stream,*loop;
 uint8_t wait,active,priority,mask,regs[14],output[14],gpio;
} AyfxPlayer;
void ayfx_init(AyfxPlayer *p,uint8_t gpio);
uint8_t ayfx_start(AyfxPlayer *p,const uint8_t *entry);
void ayfx_stop(AyfxPlayer *p);
void ayfx_tick(AyfxPlayer *p);
void ayfx_mix(AyfxPlayer *p,const uint8_t *music);
void ayfx_write(AyfxPlayer *p);
uint8_t ayfx_read_gpio(void);
#endif
