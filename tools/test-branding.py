"""Check the shipped public artwork through the actual package boundary."""
import argparse
import io
import zipfile
from PIL import Image

parser = argparse.ArgumentParser()
parser.add_argument('vpk')
args = parser.parse_args()
with zipfile.ZipFile(args.vpk) as package:
    assert 'sce_sys/icon0.png' in package.namelist(), 'Installed app icon is missing'
    image = Image.open(io.BytesIO(package.read('sce_sys/icon0.png')))
    assert image.size == (128, 128) and image.mode == 'P', 'Icon must match VitaSDK native icon format'
    assert not image.info, 'Packaged icon must contain pixel data only'
print('branding package: installed Vita icon format, size and clean metadata passed')
