#!/usr/bin/env python3
"""Explicit optional Pocket C3 hardware example; install BEFORE sound integration."""
import sys
sys.dont_write_bytecode = True
import install_xiaozhi as installer
installer.DATA = installer.ROOT / 'examples/pocket-c3'
installer.PARTS = {'board': {
    'examples/pocket-c3/config.h': 'main/boards/pocket-sounds-c3/config.h',
    'examples/pocket-c3/config.json': 'main/boards/pocket-sounds-c3/config.json',
    'examples/pocket-c3/pocket_sounds_c3.cc': 'main/boards/pocket-sounds-c3/pocket_sounds_c3.cc',
}}
# mkdir is done only after source checks in the shared transaction helper.
if __name__ == '__main__':
    import sys
    try: installer.main()
    except (ValueError, OSError, installer.subprocess.CalledProcessError) as error:
        print('Refusing: ' + str(error), file=sys.stderr)
        sys.exit(1)
