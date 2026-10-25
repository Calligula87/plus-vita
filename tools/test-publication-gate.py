"""Exercise the actual VPK/history gate with synthetic leak fixtures."""
import argparse
import io
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import zipfile
import zlib

parser = argparse.ArgumentParser()
parser.add_argument('vpk', type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
gate = root / 'tools/publication_gate.py'
canary = b'PV_PRIVATE_' + b'CANARY_fixture'


def run(vpk=None, repo=root):
    command = [sys.executable, str(gate), '--repo', str(repo)]
    if vpk:
        command += ['--package-only', '--vpk', str(vpk)]
    result = subprocess.run(command, capture_output=True)
    assert canary not in result.stdout + result.stderr, 'Gate must never print findings containing fixture data'
    return result.returncode, json.loads(result.stdout)


def synthetic_self(payload, compressed):
    data = bytearray(4096)
    data[:4] = b'SCE\0'
    encoded = zlib.compress(payload) if compressed else payload
    struct.pack_into('<10Q', data, 16, 4096, len(payload), 4096+len(encoded), 0, 4, 128, 160, 224, 0, 256)
    data[160:167] = b'\x7fELF\x01\x01\x01'
    struct.pack_into('<H', data, 204, 1)
    struct.pack_into('<4Q', data, 256, 4096, len(encoded), 2 if compressed else 1, 2)
    return bytes(data) + encoded


with zipfile.ZipFile(args.vpk) as package:
    baseline = {member.filename: package.read(member) for member in package.infolist()}
code, report = run(args.vpk)
assert code == 0 and report['software_privacy_pass'], 'Clean built candidate must pass before leak fixtures'
assert not report['publication_authorized'], 'Software checks cannot authorize publication'
with tempfile.TemporaryDirectory(prefix='plus-vita-publication-') as directory:
    work = Path(directory)
    assert work.resolve().is_relative_to(Path(tempfile.gettempdir()).resolve())
    cases = [
        ('account.json', b'{}', 'package.disallowed_resource'),
        ('local_probe_ca.der', b'fixture', 'package.disallowed_resource'),
        ('licenses/opus.txt', baseline['licenses/opus.txt']+canary, 'package.private_canary'),
        ('licenses/opus.txt', b'C:/'+b'Users/'+b'PrivateFixture/project/file.c', 'package.private_build_path'),
        ('licenses/opus.txt', b'{"npsso":"'+b'A'*64+b'"}', 'package.credential_literal'),
        ('eboot.bin', synthetic_self(canary, False), 'executable.private_canary'),
        ('eboot.bin', synthetic_self(canary, True), 'executable.private_canary'),
        ('eboot.bin', synthetic_self(canary.decode().encode('utf-16le'),True), 'executable.private_canary'),
        ('licenses/opus.txt', b'http://10.20.30.40/fixture', 'package.private_network_address'),
    ]
    png = baseline['sce_sys/icon0.png']
    payload = b'Comment\0'+canary
    chunk = b'tEXt'+payload
    dirty_icon = png[:-12]+struct.pack('>I',len(payload))+chunk+struct.pack('>I',zlib.crc32(chunk))+png[-12:]
    cases.append(('sce_sys/icon0.png',dirty_icon,'package.image_metadata'))
    for number, (name, data, expected) in enumerate(cases):
        mutated = dict(baseline); mutated[name] = data
        path = work / ('fixture-'+str(number)+'.vpk')
        with zipfile.ZipFile(path, 'w', compression=zipfile.ZIP_DEFLATED) as package:
            for member, contents in mutated.items():
                package.writestr(member, contents)
        code, report = run(path)
        assert code != 0 and expected in report['findings'], 'A synthetic privacy fixture escaped the actual gate'
    # A clean HEAD cannot hide a secret in a previous Git blob.
    repo = work / 'history'; repo.mkdir()
    def git(*arguments):
        subprocess.run(['git','-C',str(repo),*arguments],check=True,capture_output=True)
    git('init'); git('config','user.name','Synthetic Fixture'); git('config','user.email','test@example.invalid')
    (repo/'fixture.txt').write_bytes(canary)
    git('add','fixture.txt'); git('commit','-m','synthetic history fixture')
    (repo/'fixture.txt').write_text('clean current content')
    git('add','fixture.txt'); git('commit','-m','clean current fixture')
    code, report = run(repo=repo)
    assert code != 0 and 'history.private_canary' in report['findings']
    assert 'source.private_canary' not in report['findings']
    # The actual source-export command must exclude untracked private inputs.
    (repo/'untracked-account.json').write_bytes(canary)
    export = subprocess.run([sys.executable,str(root/'tools/build_anonymous_candidate.py'),
                            '--repo',str(repo),'--output',str(repo/'build/export'),'--export-only'],capture_output=True)
    assert export.returncode==0 and canary not in export.stdout+export.stderr
    assert not (repo/'build/export/source/untracked-account.json').exists()
    assert (repo/'build/export/source/fixture.txt').read_text()=='clean current content'
    # Messages are public history too, even when every committed file is clean.
    messages = work/'messages'; messages.mkdir()
    repo = messages
    git('init'); git('config','user.name','Synthetic Fixture'); git('config','user.email','test@example.invalid')
    git('commit','--allow-empty','-m',canary.decode())
    code, report = run(repo=repo)
    assert code != 0 and 'history.private_canary' in report['findings']
    git('commit','--amend','--allow-empty','-m','clean public message')
    code, report = run(repo=repo)
    assert code == 0
    git('tag','-a','synthetic-tag','-m',canary.decode())
    code, report = run(repo=repo)
    assert code != 0 and 'history.private_canary' in report['findings']
print('publication gate: clean actual VPK, forbidden resources, raw/compressed executable canaries, metadata, paths, credentials and historical leak fixtures passed')
