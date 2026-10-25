"""Prepare deterministic public Vita artwork; never retain source metadata."""
import argparse
from pathlib import Path
import struct
from PIL import Image


def clean_image(path):
    with Image.open(path) as source:
        source.load()
        rgb = source.convert('RGB')
        return Image.frombytes('RGB', rgb.size, rgb.tobytes())


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--logo', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--splash', type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    logo = clean_image(args.logo)
    logo.save(args.output / 'logo-source.png', optimize=False, compress_level=9)
    icon = logo.resize((128, 128), Image.Resampling.LANCZOS).quantize(colors=256)
    icon.save(args.output / 'icon0.png', optimize=False, compress_level=9)
    local_logo = logo.resize((192, 192), Image.Resampling.LANCZOS).convert('RGBA')
    (args.output / 'logo.pvi').write_bytes(b'PVIMAGE1' + struct.pack('<II', 192, 192) + local_logo.tobytes())
    if args.splash:
        splash = clean_image(args.splash)
        splash.save(args.output / 'splash-source.png', optimize=False, compress_level=9)
        fitted = splash.copy()
        fitted.thumbnail((960, 544), Image.Resampling.LANCZOS)
        canvas = Image.new('RGB', (960, 544), (0, 12, 36))
        canvas.paste(fitted, ((960 - fitted.width)//2, (544 - fitted.height)//2))
        (args.output / 'splash.pvi').write_bytes(b'PVIMAGE1' + struct.pack('<II', 960, 544) + canvas.convert('RGBA').tobytes())
    print('Prepared clean public logo source and Vita app icon')


if __name__ == '__main__':
    main()
