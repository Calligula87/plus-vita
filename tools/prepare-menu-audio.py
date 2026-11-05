"""Original Plus Vita ambient synthesis; no recordings or external samples.

The waveform and synthesis recipe are dedicated under CC0-1.0.
See vita-client/assets/audio/NOTICE.txt.
"""
import argparse
import math
from pathlib import Path
import struct

RATE = 48000
SECONDS = 24


def render():
    count = RATE * SECONDS
    samples = bytearray(count * 2)
    pads = [(round(f * SECONDS), amplitude) for f, amplitude in
            [(146.83, 0.055), (220.0, 0.032), (293.66, 0.024), (440.0, 0.008)]]
    for i in range(count):
        phase = 2 * math.pi * i / count
        value = sum(math.sin(cycles * phase) * amplitude *
                    (0.72 + 0.28 * math.cos(phase + index))
                    for index, (cycles, amplitude) in enumerate(pads))
        for index, frequency in enumerate([349.23, 523.25, 392.0]):
            envelope = ((1 + math.cos(phase - (index + 1) * 1.55)) / 2) ** 24
            value += 0.022 * envelope * math.sin(round(frequency * SECONDS) * phase)
        struct.pack_into('<h', samples, i * 2, round(value * 32767))
    return b'PVAUDIO1' + struct.pack('<II', RATE, count) + samples


def validate(data):
    assert data[:8] == b'PVAUDIO1'
    rate, count = struct.unpack_from('<II', data, 8)
    assert rate == RATE and count == RATE * SECONDS and len(data) == 16 + count * 2
    samples = struct.unpack_from(f'<{count}h', data, 16)
    assert 100 < max(abs(sample) for sample in samples) < 10000
    assert max(abs(b - a) for a, b in zip(samples, samples[1:])) < 400
    assert abs(samples[-1] - samples[0]) < 400


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    path = Path(__file__).resolve().parents[1] / 'vita-client/assets/audio/menu.pcm'
    generated = render()
    validate(generated)
    if args.check:
        assert path.read_bytes() == generated, 'committed menu audio differs from synthesis'
    else:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(generated)
    print('Original ambient loop: deterministic, bounded, unclipped and seamless checks passed')
