import binascii
import struct
import unittest
from verify_capture import verify

def recording(channel, polarity=1):
    out = bytearray()
    for seq in range(8000):
        frame = bytearray(b'HBCI\x02\x02\x48\x00'+struct.pack('<II',seq,seq*250)+b'\xc0\0\0'*2)
        for i in range(16):
            counts = polarity*(1590 if seq%200<50 else -530) if i==channel-1 else 0
            frame.extend(counts.to_bytes(3,'big',signed=True))
        out.extend(frame+struct.pack('<H',binascii.crc_hqx(frame,65535)))
    return out
class CaptureTest(unittest.TestCase):
    def test_signal_identity(self):
        frame=recording(16)
        verify(frame,16)
        with self.assertRaises(ValueError): verify(frame,15)
        frame[24]^=1
        with self.assertRaises(ValueError): verify(frame,16)
    def test_polarity(self):
        with self.assertRaises(ValueError): verify(recording(1,-1),1)
if __name__=='__main__': unittest.main()
