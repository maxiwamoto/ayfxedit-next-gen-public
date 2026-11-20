"""Generate original synthetic AFX/AFB fixtures; no game assets required."""
from pathlib import Path
import struct
root = Path(__file__).resolve().parents[1] / 'native/build/fixtures'
root.mkdir(parents=True, exist_ok=True)
effects=[]
for index in range(8):
    data=bytearray();tone_old=noise_old=-1
    for frame in range(64):
        tone=80+index*31+frame*2;noise=(index+frame//8)%32
        volume=max(1,12-frame//6);tone_on=index%3!=2;noise_on=index%3!=0
        flags=volume|(0 if tone_on else 16)|(0 if noise_on else 128)
        if tone!=tone_old:flags|=32
        if noise!=noise_old:flags|=64
        data.append(flags)
        if flags&32:data.extend(struct.pack('<H',tone))
        if flags&64:data.append(noise)
        tone_old,noise_old=tone,noise
    data.extend(b'\xd0\x20');effects.append(bytes(data))
    (root/f'synthetic-{index+1}.afx').write_bytes(data)
bank=bytearray(1+2*len(effects));bank[0]=len(effects)
for index,data in enumerate(effects):
    struct.pack_into('<H',bank,1+index*2,len(bank)-(2+index*2))
    bank.extend(data);bank.extend(f'Synthetic effect {index+1}'.encode()+b'\0')
(root/'sample.afb').write_bytes(bank)
print('Generated 8 synthetic effects and 1 bank.')
