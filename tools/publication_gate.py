"""Fail-closed publication checks. Reports categories/counts, never matched data."""
import argparse
import collections
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import zipfile
import zlib

MAX_MEMBER = 32 * 1024 * 1024
RESOURCES = {
    'sce_sys/icon0.png': 'vita-client/assets/branding/icon0.png',
    'branding/menu.pvi': 'vita-client/assets/branding/menu.pvi',
    'branding/wordmark.pvi': 'vita-client/assets/branding/wordmark.pvi',
    'sce_sys/livearea/contents/bg.png': 'vita-client/assets/branding/livearea/bg.png',
    'sce_sys/livearea/contents/startup.png': 'vita-client/assets/branding/livearea/startup.png',
    'sce_sys/livearea/contents/template.xml': 'vita-client/assets/branding/livearea/template.xml',
    'branding/splash.pvi': 'vita-client/assets/branding/splash.pvi',
    'cacert.pem': 'vita-client/cacert.pem',
    'fonts/NotoSans-Regular.ttf': 'vita-client/assets/fonts/NotoSans-Regular.ttf',
    'licenses/opus.txt': 'external/opus-1.6.1/COPYING',
    'licenses/gf-complete.txt': 'external/pylux/third-party/gf-complete/License.txt',
    'licenses/jerasure.txt': 'external/pylux/third-party/jerasure/License.txt',
    'licenses/stb-image.txt': 'vita-client/third_party/stb-image-license.txt',
    'licenses/noto-sans.txt': 'vita-client/assets/fonts/OFL.txt',
    'licenses/freetype.txt': 'external/freetype/docs/FTL.TXT',
    'licenses/font-notice.txt': 'vita-client/assets/fonts/NOTICE.txt',
    'licenses/freetype-third-party.txt': 'vita-client/assets/fonts/FreeType-ThirdParty.txt',
}
ALLOWLIST = set(RESOURCES) | {'eboot.bin', 'sce_sys/param.sfo'}


def findings(data, artifact=False):
    # Also inspect native UTF-16 string literals without exposing their contents.
    wide = [match.group()[::2] for match in re.finditer(rb'(?:[\x20-\x7e]\0){8,}',data)]
    if wide:
        data += b'\n'+b'\n'.join(wide)
    result = set()
    if b'PV_PRIVATE_' + b'CANARY_' in data:
        result.add('private_canary')
    if re.search(rb'(?:[A-Za-z]:[/\\]+Users[/\\]+[A-Za-z0-9_.-]+[/\\]+|/Users/[A-Za-z0-9_.-]+/)', data):
        result.add('private_build_path')
    if artifact and re.search(rb'(?:https?|ftp)://(?:192\.168\.|10\.\d+\.|172\.(?:1[6-9]|2\d|3[01])\.)', data):
        result.add('local_test_url')
    if artifact and re.search(rb'(?<![\d.])(?:192\.168\.\d{1,3}\.\d{1,3}|10\.\d{1,3}\.\d{1,3}\.\d{1,3}|172\.(?:1[6-9]|2\d|3[01])\.\d{1,3}\.\d{1,3})(?![\d.])',data):
        result.add('private_network_address')
    for match in re.finditer(rb'(?<![A-Za-z0-9])([A-Za-z0-9]{64})(?![A-Za-z0-9])', data):
        context = data[max(0, match.start()-100):match.start()].lower()
        if b'npsso' not in context:
            continue
        if not artifact and len(set(match.group())) <= 2:
            continue  # Existing explicit synthetic test fixtures, never accepted in artifacts.
        prefix = data[max(0, match.start()-100):match.start()].lower()
        if re.fullmatch(rb'[0-9a-fA-F]{64}', match.group()) and re.search(rb'(?:sha-?256|checksum|hash)[^\r\n]{0,70}$', prefix):
            continue
        result.add('credential_literal')
    secret = rb'(?:access_token|refresh_token|handshake_key|session_key|password)'
    if re.search(secret + rb'["\x27\\ ]{0,4}[:=][ \\]{0,4}["\x27][A-Za-z0-9_+/=-]{24,}', data, re.I):
        result.add('credential_literal')
    if re.search(rb'(?:Bearer\s+[A-Za-z0-9._~+/-]{24,}|eyJ[A-Za-z0-9_-]{10,}\.[A-Za-z0-9_-]{10,}\.[A-Za-z0-9_-]{10,})',data):
        result.add('credential_literal')
    return result


def self_segments(data):
    """Decode the shipped VitaSDK SELF segments, including zlib-compressed ones.

    Layout: vitasdk/vita-toolchain src/self.h and vita-make-fself.c.
    Unknown/encrypted/truncated executables fail rather than skipping their data.
    """
    if len(data) < 128 or data[:4] != b'SCE\0':
        raise ValueError('unsupported_executable')
    header_size, _, size, _, _, _, elf, _, _, table = struct.unpack_from('<10Q', data, 16)
    if size != len(data) or not 128 <= header_size <= len(data) or elf + 52 > header_size:
        raise ValueError('invalid_executable')
    if data[elf:elf+7] != b'\x7fELF\x01\x01\x01':
        raise ValueError('invalid_executable')
    count = struct.unpack_from('<H', data, elf+44)[0]
    if not 1 <= count <= 128 or table + count*32 > header_size:
        raise ValueError('invalid_executable')
    total = 0
    for index in range(count):
        offset, length, compression, encryption = struct.unpack_from('<4Q', data, table+index*32)
        if offset < header_size or offset+length > len(data) or length > MAX_MEMBER or encryption != 2:
            raise ValueError('invalid_executable')
        segment = data[offset:offset+length]
        if compression == 2:
            decoder = zlib.decompressobj()
            segment = decoder.decompress(segment, MAX_MEMBER+1)
            if not decoder.eof or decoder.unconsumed_tail or len(decoder.unused_data) > 3 or decoder.unused_data.strip(b'\0'):
                raise ValueError('invalid_executable')
        elif compression != 1:
            raise ValueError('invalid_executable')
        total += len(segment)
        if total > 2*MAX_MEMBER or len(segment) > MAX_MEMBER:
            raise ValueError('executable_limit')
        yield segment


def package_findings(vpk, root):
    errors = collections.Counter()
    with zipfile.ZipFile(vpk) as package:
        if package.comment:
            errors['package.archive_metadata'] += 1
        names = package.namelist()
        if set(names) != ALLOWLIST or len(names) != len(set(names)):
            errors['package.resource_inventory'] += 1
        for member in package.infolist():
            if member.extra or member.comment:
                errors['package.archive_metadata'] += 1
            if member.filename not in ALLOWLIST or member.file_size > MAX_MEMBER or member.flag_bits & 1:
                errors['package.disallowed_resource'] += 1
                continue
            data = package.read(member)
            for category in findings(data, artifact=True):
                errors['package.'+category] += 1
            if member.filename in RESOURCES:
                expected = committed_resource(root, RESOURCES[member.filename])
                if hashlib.sha256(data).digest() != hashlib.sha256(expected).digest():
                    errors['package.resource_identity'] += 1
            if member.filename.endswith('.png'):
                position = 8
                while position + 12 <= len(data):
                    length = int.from_bytes(data[position:position+4], 'big')
                    kind = data[position+4:position+8]
                    if kind not in (b'IHDR', b'IDAT', b'PLTE', b'tRNS', b'IEND') or position+12+length > len(data):
                        errors['package.image_metadata'] += 1
                        break
                    position += 12+length
                if position != len(data):
                    errors['package.image_metadata'] += 1
            if member.filename == 'eboot.bin':
                for segment in self_segments(data):
                    if re.search(rb'beta[ -]candidate',segment,re.I):
                        errors['executable.obsolete_branding'] += 1
                    for category in findings(segment, artifact=True):
                        errors['executable.'+category] += 1
                    if b'plus-vita-replay-720p.bin' in segment or b'local_probe_ca.der' in segment:
                        errors['executable.test_input'] += 1
            if member.filename == 'sce_sys/param.sfo':
                if len(data)<20 or data[:4]!=b'\0PSF':
                    errors['package.app_identity'] += 1
                    continue
                keys,values,count=struct.unpack_from('<III',data,8)
                identity={}
                if count>64 or 20+count*16>len(data):
                    raise ValueError('invalid_app_identity')
                for index in range(count):
                    key,kind,length,capacity,offset=struct.unpack_from('<HHIII',data,20+index*16)
                    if keys+key>=len(data) or values+offset+length>len(data) or length>capacity:
                        raise ValueError('invalid_app_identity')
                    name=data[keys+key:data.index(b'\0',keys+key)]
                    identity[name]=data[values+offset:values+offset+length].rstrip(b'\0')
                if identity.get(b'APP_VER')!=b'00.11' or identity.get(b'TITLE_ID')!=b'PLVT00001' or identity.get(b'TITLE')!=b'Plus-Vita':
                    errors['package.app_identity'] += 1
    return errors


def git(root, *args, data=None):
    return subprocess.check_output(['git', '-C', str(root), *args], input=data, stderr=subprocess.DEVNULL)


def committed_resource(root, name, revision='HEAD', depth=0):
    """Read exact pinned bytes, independent of checkout newline conversion."""
    if depth > 8:
        raise ValueError('Unsupported dependency nesting')
    for entry in git(root, 'ls-tree', '-r', '-z', revision).split(b'\0'):
        if not entry:
            continue
        metadata, path = entry.split(b'\t', 1)
        mode, kind, oid = metadata.split()
        prefix = path.decode('utf-8')
        if mode == b'160000' and name.startswith(prefix+'/'):
            return committed_resource(root/prefix, name[len(prefix)+1:], oid.decode(), depth+1)
    return git(root, 'show', revision+':'+name)


def repository_findings(root, reviewed):
    errors = collections.Counter()
    stats = collections.Counter()
    if git(root, 'rev-parse', '--is-shallow-repository').strip() == b'true':
        errors['history.incomplete'] += 1
    for name in git(root, 'ls-files', '-z').split(b'\0'):
        if not name:
            continue
        path = root / name.decode('utf-8')
        if path.is_symlink():
            errors['source.symlink'] += 1
        elif path.is_file():
            stats['source_files'] += 1
            for category in findings(path.read_bytes()):
                errors['source.'+category] += 1
    ids = dict.fromkeys(line.split(b' ', 1)[0] for line in git(root, 'rev-list', '--objects', '--all').splitlines())
    records = git(root, 'cat-file', '--batch', data=b''.join(oid+b'\n' for oid in ids))
    position = 0
    for oid in ids:
        end = records.index(b'\n', position)
        header = records[position:end].split()
        size = int(header[2]); blob = records[end+1:end+1+size]; position = end+size+2
        kind = header[1]
        if kind not in (b'blob', b'commit', b'tag'):
            continue
        if kind != b'blob':
            # Public Git author metadata is not release payload. Messages are.
            blob = blob.partition(b'\n\n')[2]
        stats['history_'+{b'blob':'blobs',b'commit':'commit_messages',b'tag':'tag_messages'}[kind]] += 1
        for category in findings(blob):
            if kind == b'blob' and category == 'private_build_path' and oid.decode() in reviewed:
                stats['reviewed_legacy_workspace_paths'] += 1
            else:
                errors['history.'+category] += 1
    return errors, stats


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--vpk', type=Path)
    parser.add_argument('--repo', type=Path, default=Path('.'))
    parser.add_argument('--package-only', action='store_true')
    parser.add_argument('--report', type=Path)
    parser.add_argument('--materials', type=Path, nargs='*', default=[])
    args = parser.parse_args()
    errors = collections.Counter(); stats = collections.Counter()
    checks = []
    try:
        if not args.package_only:
            policy = args.repo / 'release/reviewed-history.json'
            reviewed = json.loads(policy.read_text())['legacy_workspace_path_blobs'] if policy.exists() else []
            source_errors, source_stats = repository_findings(args.repo, reviewed)
            errors.update(source_errors); stats.update(source_stats); checks += ['source', 'history']
        if args.vpk:
            errors.update(package_findings(args.vpk, args.repo)); checks.append('actual_vpk_and_executable')
        for material in args.materials:
            for category in findings(material.read_bytes(), artifact=True):
                errors['public_material.'+category] += 1
        if args.materials:
            checks.append('selected_public_materials')
        if not checks:
            errors['gate.no_inputs'] += 1
    except (OSError, ValueError, KeyError, struct.error, zlib.error, zipfile.BadZipFile, subprocess.CalledProcessError):
        errors['gate.unreadable_or_unsupported_input'] += 1
    report = {'software_privacy_pass': not errors, 'checks': checks,
              'findings': dict(errors), 'counts': dict(stats), 'hardware_release_gate': 'pending_23',
              'publication_authorized': False}
    encoded = json.dumps(report, indent=2) + '\n'
    if args.report:
        args.report.write_text(encoded)
    print(encoded, end='')
    return 1 if errors else 0


if __name__ == '__main__':
    raise SystemExit(main())
