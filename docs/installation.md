# Installation, removal and build

## Existing pinned firmware

Keep your firmware work and backups. Use a separate source checkout or an existing
saved branch for integration. The installer checks exact hashes of every touched
file and the pinned Git HEAD; unrelated changes elsewhere are allowed. It refuses
unknown changes rather than merging or overwriting them. A modified custom
firmware requires a reviewed manual port of the documented hooks.

```sh
python3 /path/to/esp32-local-sounds/tools/install_xiaozhi.py /path/to/xiaozhi
```

This applies two separately owned layers: `shared-playback.patch` installs the
common output/session/cancellation/resume contract; `sounds.patch` installs sound
registration and its thin adapter. No board is installed by this command.
Installation records `.local-sounds-*.json` retain exact original source and hashes
locally for removal. Keep those records with the consumer source; do not copy them
into a public package or delete them before removing the add-on.

An existing dependency lock is preserved. For a new checkout, copy the provided
`integrations/xiaozhi/dependencies.lock` before configuring; it records the tested
C3 dependency set. The add-on does not download dependencies or an SDK.

## Optional Pocket C3 example

This is a separate **explicit hardware choice**, intended for Pocket ESP AI wiring,
ESP32-C3, 4 MiB flash, no PSRAM, shared-clock 24 kHz I2S and an optional SSD1306
128×64 OLED. It creates its own `pocket-sounds-c3` board identity:

| Hardware | GPIO |
|---|---:|
| I2S BCLK / WS | 2 / 1 |
| Speaker DIN / microphone SD | 3 / 8 |
| OLED SDA / SCL | 20 / 10 |
| BOOT/action button | 9 |

Install it **before** the sound integration into a clean pinned source base:

```sh
python3 /path/to/esp32-local-sounds/tools/install_example_board.py /path/to/xiaozhi
python3 /path/to/esp32-local-sounds/tools/install_xiaozhi.py /path/to/xiaozhi
```

The example uses ordinary `OledDisplay` with a `NoDisplay` hardware-init fallback,
the original upstream `NoAudioCodecDuplex` input behavior and standard physical
provisioning/action control. It sets 4 MiB partitions, USB console and OLED geometry
explicitly. It suppresses automatic upstream firmware replacement only for its own
custom board identity, while retaining server discovery/activation. This is a board
policy, not an upstream default or a sound requirement. It does not tune RX buffers,
compressed queues or the microphone. No Otto implementation is installed.

## Build with an existing tool installation

Use the pinned SDK/tools already installed on your machine. Do not use an older
SDK or silently upgrade. Replace paths below with your existing installations:

```sh
export IDF_TOOLS_PATH=/path/to/existing/espressif-v6.1
source /path/to/existing/esp-idf-v6.1/export.sh
idf.py --version
cd /path/to/xiaozhi
# Existing firmware: use its normal board/variant build command.
# The optional example has this exact command:
python3 scripts/build.py pocket-sounds-c3 --name pocket-sounds-c3-4mb --language en-US --wake-word hiesp
```

Check the commits against `versions.json`, preserve the existing dependency cache
and lock, and inspect component resolution before allowing downloads. The tested
build reused an existing stock managed-component cache; it was not a fresh-download
validation. Compilers, Python environments and SDKs are not included here.

Build outputs stay in the receiving firmware's build location. Verify partition
fit, image layout, hardware wiring and the firmware's recovery instructions before
any separately chosen hardware installation. This source package supplies no
universal flash image or command and performs no flash operation.

## Removal

Stop local playback and save source edits. Remove sounds and shared glue together:

```sh
python3 /path/to/esp32-local-sounds/tools/install_xiaozhi.py /path/to/xiaozhi --remove
# If the separate example board was installed, remove it last:
python3 /path/to/esp32-local-sounds/tools/install_example_board.py /path/to/xiaozhi --remove
```

Removal verifies all affected files before changing them and refuses edits made
since installation. It restores exact original source and removes only the files
recorded for this package. It does not restore/deploy a binary or erase configuration.
Rebuild your receiving firmware afterwards. A consumer that intentionally changed
these files must review and port the reverse patch rather than force removal.

When another local-audio feature uses the shared contract, remove only the sounds:

```sh
python3 /path/to/esp32-local-sounds/tools/install_xiaozhi.py /path/to/xiaozhi --remove --part sounds
```

Retain shared glue until all consumers are removed; the installer only knows its own
sound dependency. Future installers must coordinate that ownership explicitly.

## Otto and older/future firmware

The public [animated-faces package](https://github.com/joelbaby/esp32-oled-animated-faces)
is optional. Its existing installer checks whole-file hashes and safely refuses a
sound-first source tree. Supported source workflow: remove sounds/shared glue,
install Otto on the pinned base, then reinstall sounds. The sound installer accepts
the reviewed Otto CMake variant; sound removal preserves those Otto edits. These
source transactions were tested in both directions; a combined firmware/hardware
build remains untested. Do not force the Otto installer or replace its repository.

Older/future XiaoZhi revisions and already customized firmware are **untested**.
The installer deliberately refuses other HEADs or unknown touched files. Port the
small host hooks in `docs/architecture.md`, retain your settings, run the focused
tests and build for your real board. Do not remove version/hash checks as a shortcut.
