"""Run compiled tniASM and SDCC MSX/Spectrum players on a Z80 emulator.
Install test-only dependency: pip install --target native/build/test-deps z80==1.2.0
Run native tests first to generate sdk-test, then its two build scripts.
"""
from pathlib import Path
import re, sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'native/build/test-deps'))
from z80 import Z80Machine
D=ROOT/'native/build/sdk-test'

def machine(platform,compiler):
    m=Z80Machine();writes=[];selected=0
    if compiler in ('asm','small'):
        suffix='small_' if compiler=='small' else ''
        image=(D/f'ayfx_{suffix}{platform}.bin').read_bytes();m.memory[0x8000:0x8000+len(image)]=image
        syms={k:int(v,16) for k,v in re.findall(r'^(\S+): %equ ([0-9A-Fa-f]+)h$',(D/f'example_{suffix}{platform}.sym').read_text(),re.M)}
    else:
        for line in (D/f'ayfx_{platform}.ihx').read_text().splitlines():
            b=bytes.fromhex(line[1:]);n=b[0];addr=int.from_bytes(b[1:3],'big')
            assert sum(b)%256==0
            if b[3]==0:m.memory[addr:addr+n]=b[4:4+n]
        syms={k:int(v,16) for v,k in re.findall(r'^\s+([0-9A-F]{8})\s+(_\w+)\s+', (D/f'ayfx_{platform}.map').read_text(),re.M)}
    def output(port,value):
        nonlocal selected
        if (platform=='msx' and port&255==0xa0) or (platform=='spectrum' and port==0xfffd):selected=value
        elif (platform=='msx' and port&255==0xa1) or (platform=='spectrum' and port==0xbffd):writes.append((selected,value))
        else:raise AssertionError(f'Bad {platform} output port {port:04x}')
    def input(port):
        assert (platform=='msx' and port&255==0xa2) or (platform=='spectrum' and port==0xfffd)
        assert selected==7
        return 0x80
    m.set_output_callback(output);m.set_input_callback(input);m.set_breakpoint(0x7000);m.iff1=True;m.iff2=True
    def call(name,**regs):
        for k,v in regs.items():setattr(m,k,v)
        m.pc=syms[name];m.sp=0xf000;m.memory[0xf000:0xf002]=b'\x00\x70';m.ticks_to_stop=200000
        for _ in range(20):
            event=m.run()
            if m.pc==0x7000:break
        assert m.pc==0x7000,f'{name} did not return'
        assert m.sp==0xf002 and m.iff1 and m.iff2,f'{name} damaged stack/interrupts'
        return 200000-m.ticks_to_stop
    return m,syms,writes,call

for platform in ('msx','spectrum'):
 for compiler in ('asm','small','c'):
    peak=0
    for group in range(5):
        m,syms,writes,call=machine(platform,compiler)
        call('AYFX_INIT' if compiler!='c' else '_game_audio_init');writes.clear()
        if compiler!='c':call('AYFX_START',hl=syms[f'AYFX_EFFECT_{group}'])
        else:call('_game_sound',a=group)
        expected=(D/f'expected{group}.bin').read_bytes()
        for frame in range(40):
            writes.clear()
            peak=max(peak,call('EXAMPLE_FRAME' if compiler!='c' else '_game_audio_frame',hl=0))
            index=frame if frame<12 else (3+(frame-12)%9 if group==2 else None)
            if index is None:
                actual=dict(writes);assert actual[7]==0xbf and actual[8]==actual[9]==actual[10]==0
            else:
                regs=list(expected[index*14:index*14+14]);regs[7]|=0x80
                # Unassigned tones stay zero; shared registers only claimed when used.
                if group<2:regs[6]=regs[11]=regs[12]=0
                wanted=[(r,v) for r,v in enumerate(regs) if r!=13 or v!=255]
                assert writes==wanted,(compiler,platform,group,frame,writes,wanted)
        # Priority rejection must not restart playback.
        before=bytes(m.memory[0xc000:0xc025]);entry=bytearray((D/'effects.bin').read_bytes());offset=int.from_bytes(entry[:2],'little');low=entry[offset:];low[1]=0
        m.memory[0xd000:0xd000+len(low)]=low
        if group==2:
            if compiler!='c':call('AYFX_START',hl=0xd000);assert m.f&1
            else:call('_ayfx_start',hl=0xc000,de=0xd000);assert m.a==0
            assert before==bytes(m.memory[0xc000:0xc025])
    # Long repeat run uses both 128-tick and 11-tick hold commands.
    m,syms,writes,call=machine(platform,compiler)
    call('AYFX_INIT' if compiler!='c' else '_game_audio_init')
    registers=[0]*13;registers[7]=62;registers[8]=10
    stream=bytes([95,106,0,0,96,0,0,96,0,0,0,0,0,62,255,138,0])
    entry=bytes([1,200,255,255])+stream
    m.memory[0xd000:0xd000+len(entry)]=entry
    if compiler!='c':call('AYFX_START',hl=0xd000)
    else:call('_ayfx_start',hl=0xc000,de=0xd000)
    music=bytes([1,2,3,4,5,6,7,0,8,9,10,11,12,255]);m.memory[0xd800:0xd80e]=music
    for frame in range(141):
        writes.clear()
        if compiler=='small':call('EXAMPLE_FRAME')
        elif compiler!='c':
            call('AYFX_TICK');call('AYFX_MIX',hl=0xd800);call('AYFX_WRITE')
        else:call('_game_audio_frame',hl=0xd800)
        wanted=list(music)
        if frame<140:
            wanted[0]=wanted[1]=0;wanted[8]=10;wanted[7]=8
        if compiler=='small':
            wanted=registers+[255]
            if frame==140:wanted[7]=63;wanted[8]=0
        wanted[7]|=0x80
        assert writes==[(r,v) for r,v in enumerate(wanted) if r!=13],(compiler,platform,frame,'music mix/hold failed',writes,wanted)
    if compiler=='small':
        print(f'PASS small {platform}: 23-byte state, 1/2/3 channels, VGM, repeated envelope triggers, loops, 128-tick holds, GPIO; peak {peak} T-states/frame')
        continue
    print(f'PASS {compiler} {platform}: 1/2/3 channels, delta/hold/end/loop, envelope retrigger, priority, 128-tick holds, VGM import, bank deduplication, music sharing/restoration, GPIO, interrupt neutrality; peak {peak} T-states/frame')
