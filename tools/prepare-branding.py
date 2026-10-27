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


def fit_image(source, size):
    fitted = source.copy()
    fitted.thumbnail(size, Image.Resampling.LANCZOS)
    canvas = Image.new('RGB', size, (0, 12, 36))
    canvas.paste(fitted, ((size[0]-fitted.width)//2, (size[1]-fitted.height)//2))
    return canvas


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--logo', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--splash', type=Path)
    parser.add_argument('--menu', type=Path)
    parser.add_argument('--wordmark', type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    logo = clean_image(args.logo)
    logo.save(args.output / 'logo-source.png', optimize=False, compress_level=9)
    icon = logo.resize((128, 128), Image.Resampling.LANCZOS).quantize(colors=256)
    icon.save(args.output / 'icon0.png', optimize=False, compress_level=9)
    if args.splash:
        splash = clean_image(args.splash)
        splash.save(args.output / 'splash-source.png', optimize=False, compress_level=9)
        canvas = fit_image(splash, (960, 544))
        (args.output / 'splash.pvi').write_bytes(b'PVIMAGE1' + struct.pack('<II', 960, 544) + canvas.convert('RGBA').tobytes())
        livearea = args.output / 'livearea'
        livearea.mkdir(exist_ok=True)
        for name, size in (('bg.png', (840, 500)), ('startup.png', (280, 158))):
            fit_image(splash, size).quantize(colors=128).save(livearea/name, optimize=False, compress_level=9)
    if args.menu:
        menu = clean_image(args.menu)
        menu.save(args.output/'menu-source.png', optimize=False, compress_level=9)
        fitted = menu.resize((960, 544), Image.Resampling.LANCZOS)
        # Precompute the dark reading surface; no per-frame blending on Vita.
        fitted = fitted.point(lambda channel: round(channel*0.45))
        (args.output/'menu.pvi').write_bytes(b'PVIMAGE1'+struct.pack('<II',960,544)+fitted.convert('RGBA').tobytes())
    if args.wordmark:
        with Image.open(args.wordmark) as source:
            rgba=source.convert('RGBA')
            rgba=Image.frombytes('RGBA',rgba.size,rgba.tobytes())
        rgba.save(args.output/'wordmark-source.png',optimize=False,compress_level=9)
        bounds=rgba.getchannel('A').getbbox()
        if not bounds:
            raise ValueError('Wordmark must contain visible text')
        fitted=rgba.crop(bounds)
        fitted.thumbnail((224,40),Image.Resampling.LANCZOS)
        canvas=Image.new('RGBA',(224,40),(0,0,0,0))
        canvas.paste(fitted,((224-fitted.width)//2,(40-fitted.height)//2))
        canvas.save(args.output/'wordmark.png',optimize=False,compress_level=9)
        (args.output/'wordmark.pvi').write_bytes(b'PVIMAGE1'+struct.pack('<II',224,40)+canvas.tobytes())
    print('Prepared clean public logo source and Vita app icon')


if __name__ == '__main__':
    main()
