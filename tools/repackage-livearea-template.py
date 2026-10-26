"""Create a template-only resource variant of the recorded candidate 2 VPK."""
import argparse
import hashlib
import json
from pathlib import Path
import zipfile
import xml.etree.ElementTree as ET

root=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser()
parser.add_argument('input',type=Path)
parser.add_argument('output',type=Path)
args=parser.parse_args()
identity=json.loads((root/'release/candidate-2.json').read_text())
if hashlib.sha256(args.input.read_bytes()).hexdigest().upper()!=identity['sha256']:
    raise ValueError('Input must be the recorded candidate 2')
if args.output.exists():
    raise ValueError('Preserve existing artifacts; choose a new output path')
name='sce_sys/livearea/contents/template.xml'
replacement=(root/'vita-client/assets/branding/livearea/template.xml').read_bytes()
if b'\r\n' not in replacement or b'\n' in replacement.replace(b'\r\n',b''):
    raise ValueError('LiveArea template must retain CRLF line endings')
args.output.parent.mkdir(parents=True,exist_ok=True)
def structure(node):
    return node.tag,node.attrib,(node.text or '').strip(),tuple(structure(child) for child in node)

with zipfile.ZipFile(args.input) as source:
    if structure(ET.fromstring(source.read(name)))!=structure(ET.fromstring(replacement)):
        raise ValueError('Formatting trial must preserve the parsed LiveArea layout')
    with zipfile.ZipFile(args.output,'w') as output:
        for member in source.infolist():
            output.writestr(member,replacement if member.filename==name else source.read(member),compresslevel=9)
with zipfile.ZipFile(args.input) as original, zipfile.ZipFile(args.output) as revised:
    if original.namelist()!=revised.namelist():
        raise ValueError('Package inventory changed')
    changed=[entry for entry in original.namelist() if original.read(entry)!=revised.read(entry)]
    if changed!=[name] or original.read('eboot.bin')!=revised.read('eboot.bin'):
        raise ValueError('Only the LiveArea template may change')
print(json.dumps({'artifact':args.output.name,'size_bytes':args.output.stat().st_size,
                 'sha256':hashlib.sha256(args.output.read_bytes()).hexdigest().upper(),
                 'base_candidate_sha256':identity['sha256'],'changed_resources':changed,
                 'runtime_source_commit':identity['source_commit'],'hardware_verified':False},indent=2))
