#include "ayfx_player.h"
void ayfx_init(AyfxPlayer *p,uint8_t gpio){
 uint8_t i;p->stream=0;p->loop=0;p->wait=0;p->active=0;p->priority=0;p->mask=0;p->gpio=gpio&0xc0;
 for(i=0;i<14;i++)p->regs[i]=p->output[i]=0;
 p->regs[7]=p->output[7]=63;p->regs[13]=p->output[13]=255;
}
uint8_t ayfx_start(AyfxPlayer *p,const uint8_t *e){
 uint16_t loop;
 if(p->active&&e[1]<p->priority)return 0;
 p->priority=e[1];p->mask=e[0];loop=e[2]|((uint16_t)e[3]<<8);
 p->loop=loop==65535?0:e+loop;p->stream=e+4;p->wait=0;p->active=1;return 1;
}
void ayfx_stop(AyfxPlayer *p){p->active=0;p->wait=0;p->regs[8]=p->regs[9]=p->regs[10]=0;p->regs[13]=255;p->regs[7]=63;}
void ayfx_tick(AyfxPlayer *p){
 uint8_t command,ch,flags;p->regs[13]=255;
 if(!p->active)return;
 if(p->wait){--p->wait;return;}
 command=*p->stream++;
 if(!command){if(!p->loop){ayfx_stop(p);return;}p->stream=p->loop;command=*p->stream++;}
 if(command&128){p->wait=command&127;return;}
 for(ch=0;ch<3;ch++)if(command&(1<<ch)){
  flags=*p->stream++;p->regs[8+ch]=flags&31;
  if(flags&32)p->regs[ch*2]=*p->stream++;
  if(flags&64)p->regs[ch*2+1]=*p->stream++;
 }
 if(command&8)p->regs[6]=*p->stream++;
 if(command&16){p->regs[11]=*p->stream++;p->regs[12]=*p->stream++;}
 if(command&32)p->regs[13]=*p->stream++;
 if(command&64)p->regs[7]=*p->stream++;
}
void ayfx_mix(AyfxPlayer *p,const uint8_t *music){
 uint8_t i,ch,bits;
 for(i=0;i<14;i++)p->output[i]=music?music[i]:0;
 if(!music){p->output[7]=63;p->output[13]=255;}
 if(!p->active)return;
 for(ch=0;ch<3;ch++)if(p->mask&(1<<ch)){
  p->output[ch*2]=p->regs[ch*2];p->output[ch*2+1]=p->regs[ch*2+1];p->output[8+ch]=p->regs[8+ch];
  bits=9<<ch;p->output[7]=(p->output[7]&~bits)|(p->regs[7]&bits);
 }
 if(p->mask&8){p->output[6]=p->regs[6];p->output[7]|=((p->mask^7)&7)<<3;}
 if(p->mask&16){
  p->output[11]=p->regs[11];p->output[12]=p->regs[12];p->output[13]=p->regs[13];
  for(ch=0;ch<3;ch++)if(!(p->mask&(1<<ch))&&(p->output[8+ch]&16))p->output[8+ch]=0;
 }
}
/* Compile with -DAYFX_MSX=1 for MSX; otherwise Spectrum 128/+2/+3 or
   a compatible external AY interface. A plain 48K machine has no AY. */
#ifdef __SDCC
#ifdef AYFX_MSX
__sfr __at(0xa0) ay_select;
__sfr __at(0xa1) ay_data;
__sfr __at(0xa2) ay_read;
#else
__sfr __banked __at(0xfffd) ay_select;
__sfr __banked __at(0xbffd) ay_data;
#define ay_read ay_select
#endif
uint8_t ayfx_read_gpio(void){ay_select=7;return ay_read&0xc0;}
void ayfx_write(AyfxPlayer *p){
 uint8_t i,value;
 for(i=0;i<14;i++){value=p->output[i];if(i==13&&value==255)continue;if(i==7)value=(value&63)|p->gpio;ay_select=i;ay_data=value;}
}
#endif
