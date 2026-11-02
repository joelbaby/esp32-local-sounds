#!/usr/bin/env python3
"""Install/remove reviewed source patches at the pinned XiaoZhi commit; never flash."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys

PIN = 'addb1ed59bd71e09a6a5e0c52ad2f1c474aa8ca1'
ROOT = Path(__file__).resolve().parents[1]
DATA = ROOT / 'integrations/xiaozhi'
PARTS = {
    'shared-playback': {
        'include/local_audio_source.h': 'main/audio/local_audio_source.h',
        'include/local_audio_session.h': 'main/audio/local_audio_session.h',
        'integrations/xiaozhi/local_audio_adapter.cc': 'main/audio/local_audio_adapter.cc',
    },
    'sounds': {
        'include/tone_noise_generator.h': 'main/audio/tone_noise_generator.h',
        'src/tone_noise_generator.cc': 'main/audio/tone_noise_generator.cc',
        'src/brass_wavetables.h': 'main/audio/brass_wavetables.h',
        'src/axel_f_phrase.h': 'main/audio/axel_f_phrase.h',
        'integrations/xiaozhi/generated_audio_adapter.cc': 'main/audio/generated_audio_adapter.cc',
        'integrations/xiaozhi/generated_audio_tools.cc': 'main/audio/generated_audio_tools.cc',
    },
}

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest() if path.is_file() else None

def run(target, *args):
    return subprocess.run(['git', '-C', str(target), *args], check=True,
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE).stdout

def safe_path(target, name):
    file = target / name
    if any(parent.is_symlink() for parent in [file, *file.parents] if parent != target and target in parent.parents):
        raise ValueError('Symlink in destination path: ' + name)
    return file

def install(target, part):
    marker = target / ('.local-sounds-' + part + '.json')
    if marker.exists():
        raise ValueError(part + ' already installed; do not overwrite installation records')
    records = json.loads((DATA / 'patch-files.json').read_text())[part]
    for name, record in records.items():
        file = safe_path(target, name)
        if file.is_symlink() or digest(file) not in record['before']:
            raise ValueError('Incompatible/modified source: ' + name + '. No files changed for this part.')
    for name in PARTS[part].values():
        if safe_path(target, name).exists():
            raise ValueError('Refusing existing destination: ' + name)
    patch = DATA / (part + '.patch')
    run(target, 'apply', '--check', str(patch))
    originals = {name: (target / name).read_text() for name in records}
    copied = {}
    # All compatibility checks happen before mutation. Roll back on local I/O failure.
    try:
        run(target, 'apply', str(patch))
        for source, destination in PARTS[part].items():
            (target / destination).parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / source, target / destination)
            copied[destination] = digest(target / destination)
        after = {name: digest(target / name) for name in records}
        marker.write_text(json.dumps({'format': 1, 'part': part, 'originals': originals,
                                     'after': after, 'copied': copied}, indent=2) + '\n')
    except Exception:
        for name, text in originals.items(): (target / name).write_text(text)
        for name in copied: (target / name).unlink()
        raise

def remove(target, part, dry_run=False, overlay=None):
    overlay = {} if overlay is None else overlay
    marker = target / ('.local-sounds-' + part + '.json')
    if not marker.is_file(): raise ValueError('No installation record for ' + part)
    data = json.loads(marker.read_text())
    expected = set(json.loads((DATA / 'patch-files.json').read_text())[part])
    if data.get('format') != 1 or data.get('part') != part or set(data['originals']) != expected:
        raise ValueError('Invalid installation record; refusing removal')
    if set(data['copied']) != set(PARTS[part].values()) or set(data['after']) != expected:
        raise ValueError('Invalid installation file inventory; refusing removal')
    for name, value in (data['after'] | data['copied']).items():
        file = safe_path(target, name)
        actual = hashlib.sha256(overlay[name].encode()).hexdigest() if name in overlay and overlay[name] is not None else (None if name in overlay else digest(file))
        if file.is_symlink() or actual != value:
            raise ValueError('Modified since installation: ' + name + '; refusing to overwrite your changes')
    for name, text in data['originals'].items():
        valid = json.loads((DATA / 'patch-files.json').read_text())[part][name]['before']
        if hashlib.sha256(text.encode()).hexdigest() not in valid:
            raise ValueError('Invalid original in installation record: ' + name)
    if dry_run:
        overlay.update(data['originals'])
        overlay.update({name: None for name in data['copied']})
        return
    for name, text in data['originals'].items(): (target / name).write_text(text)
    for name in data['copied']: (target / name).unlink()
    marker.unlink()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('checkout', type=Path)
    parser.add_argument('--remove', action='store_true')
    parser.add_argument('--part', choices=['all', *PARTS], default='all')
    args = parser.parse_args()
    target = args.checkout.resolve()
    if run(target, 'rev-parse', 'HEAD').decode().strip() != PIN:
        raise ValueError('Wrong XiaoZhi commit; expected ' + PIN)
    parts = list(PARTS) if args.part == 'all' else [args.part]
    if args.remove: parts.reverse()
    if not args.remove and 'sounds' in parts and 'shared-playback' not in parts:
        marker = target / '.local-sounds-shared-playback.json'
        if not marker.is_file(): raise ValueError('Install shared-playback first')
    if args.remove and 'shared-playback' in parts and 'sounds' not in parts:
        if (target / '.local-sounds-sounds.json').exists():
            raise ValueError('Remove sounds first; shared host glue is still in use')
    if args.remove:
        overlay = {}
        for part in parts: remove(target, part, dry_run=True, overlay=overlay)
    done = []
    try:
        for part in parts:
            (remove if args.remove else install)(target, part)
            done.append(part)
    except Exception:
        # An all-install failing in part 2 leaves no half installation.
        if not args.remove:
            for part in reversed(done): remove(target, part)
        raise
    print(('Removed ' if args.remove else 'Installed ') + ', '.join(parts) + '. No build/flash performed.')

if __name__ == '__main__':
    try: main()
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        print('Refusing: ' + str(error), file=sys.stderr)
        sys.exit(1)
