"""Build from committed exports and pinned dependencies, never working-tree data."""
import argparse
import hashlib
import io
import json
import os
import posixpath
import shutil
from pathlib import Path, PurePosixPath
import subprocess
import tarfile
import zipfile

DEPENDENCIES = ('external/pylux', 'external/json-c', 'external/freetype',
                'external/pylux/third-party/nanopb', 'external/pylux/third-party/curl',
                'external/pylux/third-party/gf-complete', 'external/pylux/third-party/jerasure')


def git(repo, *args):
    return subprocess.check_output(['git', '-C', str(repo), *args], stderr=subprocess.DEVNULL)


def export(repo, revision, destination, exclusions=()):
    archive = git(repo, '-c', 'core.autocrlf=false', '-c', 'core.eol=lf',
                  'archive', '--format=tar', revision, '--', '.',
                  *(':(exclude)'+path for path in exclusions))
    destination.mkdir(parents=True, exist_ok=True)
    with tarfile.open(fileobj=io.BytesIO(archive)) as members:
        entries = {member.name.rstrip('/'): member for member in members}

        def write_entry(member, output_name, ancestors=()):
            name = PurePosixPath(output_name)
            if name.is_absolute() or '..' in name.parts or member.islnk():
                raise ValueError('Unsafe source export entry')
            target = destination.joinpath(*name.parts)
            if not target.resolve().is_relative_to(destination.resolve()):
                raise ValueError('Source export escaped its destination')
            if member.isdir():
                target.mkdir(parents=True, exist_ok=True)
            elif member.isfile():
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(members.extractfile(member).read())
                target.chmod(member.mode & 0o777)
            elif member.issym():
                # Materialize only committed archive content, never a host link.
                linked = posixpath.normpath(posixpath.join(posixpath.dirname(member.name), member.linkname))
                if PurePosixPath(linked).is_absolute() or '..' in PurePosixPath(linked).parts:
                    raise ValueError('Source link escaped its archive')
                if linked in ancestors or linked not in entries:
                    raise ValueError('Unsupported source archive link')
                resolved = entries[linked]
                write_entry(resolved, output_name, ancestors+(linked,))
                if resolved.isdir():
                    for child_name, child in entries.items():
                        if child_name.startswith(linked+'/'):
                            write_entry(child, output_name+child_name[len(linked):], ancestors+(linked,))
            else:
                raise ValueError('Unsupported source export entry')

        for name, member in entries.items():
            write_entry(member, name)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--repo', type=Path, default=Path('.'))
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--sdk', type=Path)
    parser.add_argument('--cmake', default='cmake')
    parser.add_argument('--ninja', default='ninja')
    parser.add_argument('--export-only', action='store_true')
    args = parser.parse_args()
    if not args.export_only:
        for name in ('cmake', 'ninja'):
            executable = shutil.which(getattr(args, name))
            if not executable:
                raise ValueError('Required build tool unavailable')
            setattr(args, name, executable)
    root = args.repo.resolve(); output = args.output.resolve()
    if output.exists() or not output.is_relative_to(root / 'build'):
        raise ValueError('Use a new output directory inside this repository build directory')
    # Local changes cannot silently diverge from the recorded source identity.
    subprocess.run(['git','-C',str(root),'-c','diff.ignoreSubmodules=all','diff','--quiet','HEAD'],check=True)
    revision = git(root, 'rev-parse', 'HEAD').decode().strip()
    source = output / 'source'; export(root, revision, source)
    pins = {}
    for dependency in DEPENDENCIES:
        parent = 'external/pylux' if dependency.startswith('external/pylux/') else ''
        relative = dependency[len(parent)+1:] if parent else dependency
        if parent and parent not in pins:
            if args.export_only:
                continue
            raise ValueError('Missing pinned parent dependency')
        try:
            pin = git(root / parent, 'rev-parse', (pins[parent] if parent else revision)+':'+relative).decode().strip()
        except subprocess.CalledProcessError:
            if args.export_only:
                continue
            raise
        # Switch-only links target an unneeded, uninitialized platform submodule.
        export(root / dependency, pin, source / dependency, ('switch',) if dependency=='external/pylux' else ())
        pins[dependency] = pin
    identity = {'source_commit': revision, 'dependency_commits': pins, 'source_export': 'committed_files_only',
                'excluded_platform_sources': ['external/pylux/switch']}
    (output / 'source-identity.json').write_text(json.dumps(identity, indent=2)+'\n')
    if args.export_only:
        print('Exported committed source and pinned dependency inputs')
        return
    if not args.sdk:
        raise ValueError('Specify the pinned VitaSDK location')
    compiler = args.sdk.resolve()/'bin/arm-vita-eabi-gcc.exe'
    if not compiler.exists():
        compiler = compiler.with_suffix('')
    identity['toolchain'] = {
        'sdk_version_manifest_sha256': hashlib.sha256((args.sdk.resolve()/'version_info.txt').read_bytes()).hexdigest().upper(),
        'compiler_version': subprocess.check_output([str(compiler),'-dumpfullversion'],stderr=subprocess.DEVNULL).decode().strip(),
        'compiler_sha256': hashlib.sha256(compiler.read_bytes()).hexdigest().upper(),
        'cmake_version': subprocess.check_output([args.cmake,'--version'],stderr=subprocess.DEVNULL).decode().splitlines()[0],
        'ninja_version': subprocess.check_output([args.ninja,'--version'],stderr=subprocess.DEVNULL).decode().strip(),
    }
    environment = dict(os.environ)
    for name in ('CFLAGS','CXXFLAGS','CPPFLAGS','LDFLAGS','CMAKE_TOOLCHAIN_FILE'):
        environment.pop(name, None)
    environment['VITASDK'] = str(args.sdk.resolve())
    environment['SOURCE_DATE_EPOCH'] = git(root,'show','-s','--format=%ct',revision).decode().strip()
    environment['GIT_CEILING_DIRECTORIES'] = str(output)
    build = output / 'build'
    configure = [args.cmake,'-S',str(source/'vita-client'),'-B',str(build),'-G','Ninja',
                 '-DCMAKE_MAKE_PROGRAM='+str(Path(args.ninja).resolve()),'-DCMAKE_BUILD_TYPE=Release',
                 '-DPLUS_VITA_RELEASE_CANDIDATE=ON','-DPLUS_VITA_DIAGNOSTIC_APP=OFF',
                 '-DPLUS_VITA_FONT_COMPARISON=OFF','-DPLUS_VITA_INPUT_WORKER=ON',
                 '-DPLUS_VITA_DEFERRED_LOG=ON','-DPLUS_VITA_FONT_DEFAULT_FREETYPE=ON',
                 '-DPLUS_VITA_TLS_PROBE_URL=','-DPLUS_VITA_TLS_TRUSTED_CA_FILE=',
                 '-DPLUS_VITA_PUBLIC_HTTPS_URL=','-DPLUS_VITA_POST_ECHO_URL=',
                 '-DPLUS_VITA_PUBLIC_APOLLO_TEST=OFF']
    with (output/'compiler-private.log').open('wb') as log:
        for command in (configure,[args.cmake,'--build',str(build),'--parallel','4']):
            subprocess.run(command,env=environment,stdout=log,stderr=log,check=True)
    original = build / 'plus_vita_smoke.vpk'
    artifact = output / 'plus-vita-v0.1.0-beta-candidate-1.vpk'
    # Package timestamps and host file attributes are not release inputs.
    with zipfile.ZipFile(original) as package, zipfile.ZipFile(artifact,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=9) as clean:
        for name in sorted(package.namelist()):
            entry = zipfile.ZipInfo(name,date_time=(1980,1,1,0,0,0))
            entry.compress_type=zipfile.ZIP_DEFLATED;entry.create_system=3;entry.external_attr=0o100644<<16
            clean.writestr(entry,package.read(name),compresslevel=9)
    identity.update({'artifact':artifact.name,'sha256':hashlib.sha256(artifact.read_bytes()).hexdigest().upper(),
                     'size_bytes':artifact.stat().st_size,'profile':'720p-input-worker-deferred-log',
                     'public_version':'0.1.0 beta candidate','hardware_gate':'pending_23'})
    (output/'candidate-identity.json').write_text(json.dumps(identity,indent=2)+'\n')
    print(json.dumps(identity,indent=2))


if __name__ == '__main__':
    try:
        main()
    except (OSError,ValueError,subprocess.CalledProcessError,tarfile.TarError,zipfile.BadZipFile):
        print('Anonymous candidate build failed; inspect the retained local inputs/compiler log. No private error details are printed.')
        raise SystemExit(1)
