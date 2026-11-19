"""Verify a 2 s bench capture for one selected J700 channel.
Use 20 Hz positive pulses, 25 percent duty cycle, 100 uVpp differential.
Feed an HBCI v2 file from the bench fixture, after settling.
"""
import argparse
import binascii
import statistics
import struct
from pathlib import Path

def verify(data, selected):
    if not 1 <= selected <= 16 or len(data) < 8000*72 or len(data)%72:
        raise ValueError('Need complete frames, >=2 seconds, and channel 1..16')
    values = [[] for _ in range(16)]
    last = None
    elapsed = 0
    for pos in range(0,len(data),72):
        frame = data[pos:pos+72]
        if frame[:8] != b'HBCI\x02\x02\x48\x00' or binascii.crc_hqx(frame[:-2],0xffff) != int.from_bytes(frame[-2:],'little'):
            raise ValueError(f'header/CRC failure at byte {pos}')
        seq, stamp = struct.unpack_from('<II',frame,8)
        if last:
            dt = (stamp-last[1]) % 2**32
            if (seq-last[0]) % 2**32 != 1 or not 150 <= dt <= 350:
                raise ValueError('sample gap or timestamp jitter')
            elapsed += dt
        last = seq, stamp
        if frame[16] >> 4 != 12 or frame[19] >> 4 != 12:
            raise ValueError('ADC status framing')
        for ch in range(16):
            v=int.from_bytes(frame[22+3*ch:25+3*ch],'big',signed=True)
            values[ch].append(v * 375000/8388608)
    if abs((len(values[0])-1)*1e6/elapsed-4000)>8:
        raise ValueError('sample-rate mismatch')
    for ch,series in enumerate(values,1):
        lo,hi=statistics.quantiles(series,n=100)[4::90]
        span=hi-lo
        if ch==selected:
            threshold=(lo+hi)/2
            rising=sum(a<threshold<=b for a,b in zip(series,series[1:]))
            if not (80<span<115 and .18<sum(v>threshold for v in series)/len(series)<.32 and 19<rising/(elapsed*1e-6)<21):
                raise ValueError(f'CH{ch}: amplitude, polarity, duty or frequency mismatch')
        elif span>10:
            raise ValueError(f'CH{ch}: unexpected response {span:.1f} uVpp')
if __name__=='__main__':
    cli=argparse.ArgumentParser(description=__doc__)
    cli.add_argument('capture',type=Path)
    cli.add_argument('channel',type=int)
    args=cli.parse_args()
    verify(args.capture.read_bytes(),args.channel)
    print('PASS',args.channel)
