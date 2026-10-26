"""Check the shipped public artwork through the actual package boundary."""
import argparse
import io
import zipfile
import xml.etree.ElementTree as ET
from PIL import Image

parser = argparse.ArgumentParser()
parser.add_argument('vpk')
args = parser.parse_args()
with zipfile.ZipFile(args.vpk) as package:
    assert 'sce_sys/icon0.png' in package.namelist(), 'Installed app icon is missing'
    image = Image.open(io.BytesIO(package.read('sce_sys/icon0.png')))
    assert image.size == (128, 128) and image.mode == 'P', 'Icon must match VitaSDK native icon format'
    assert not image.info, 'Packaged icon must contain pixel data only'
    base='sce_sys/livearea/contents/'
    template_bytes=package.read(base+'template.xml')
    assert b'\r\n' in template_bytes and b'\n' not in template_bytes.replace(b'\r\n',b''), 'LiveArea XML must retain VitaSDK CRLF line endings'
    template=ET.fromstring(template_bytes)
    assert template.tag=='livearea' and template.attrib['content-rev']=='2'
    for name,size in ((template.findtext('livearea-background/image'),(840,500)),
                      (template.findtext('gate/startup-image'),(280,158))):
        assert name in ('bg.png','startup.png')
        data=package.read(base+name)
        image=Image.open(io.BytesIO(data))
        assert image.size==size and image.mode=='P' and not image.info
        assert len(data)<=420*1024
    assert 'branding/logo.pvi' not in package.namelist()
    menu=package.read('branding/menu.pvi')
    assert menu[:8]==b'PVIMAGE1' and len(menu)==16+960*544*4
print('branding package: installed Vita icon format, size and clean metadata passed')
