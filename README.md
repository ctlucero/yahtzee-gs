# Yahtzee for KDE Plasma

A native Linux port of **YAHTZEE v1.1** for the Apple IIgs (David W. Buell, 1988,
written in TML Pascal; "Distribute freely but do not sell"), recovered from
the original program and rebuilt in C++ with a thin Qt 6 shell. It keeps the
original's look: the 320×200 IIgs screen with the game's own palette. 

## How to play

The default is the original's: **Player #1 (you) against Player #2 (the
computer)**, three games to a series. To change it, choose **Game ▸ Setup…**
when no game is running: type a name, pick Human or Computer, press **Enroll**
(or Return) for each player in turn, set the options, then **Play**.

Choose **Game ▸ New Game** (Ctrl+G). Click **Roll**. Click the dice you want
to **reroll** (they turn teal), then Roll again — up to three rolls. Click a
category name and then **Score**. Hold the mouse on the long bar at the
bottom to see what each open category would score. Ctrl+Q quits. It's Yahtzee.

## Options

**Game ▸ Options…** has two extras that the 1988 original does not. Both are
off by default, so the game plays exactly as the original until you turn
them on, and both are remembered between runs.

- **Yahtzee Bonus**: once your Yahtzee box holds 50, every further Yahtzee
  earns 100 bonus points and can be scored in any open box.
- **Modern Roll**: every die comes straight from the operating system's
  random source (`getrandom`, the same pool as `/dev/urandom`) instead of
  the game's own seeded generator, with no rounding bias between faces.
  This can be switched at any time.

## Download and install

Download `yahtzee-gs-<version>-linux-x86_64.tar.gz` from the
[Releases page](https://github.com/ctlucero/yahtzee-gs/releases), then:

```sh
tar xzf yahtzee-gs-*-linux-x86_64.tar.gz
cd yahtzee-gs-*-linux-x86_64
./install.sh
```

Yahtzee then appears in your application menu under Games. This installs it
for you only, into `~/.local`; to install it for all users instead, run
`sudo ./install.sh /usr/local`. To remove it, run `./install.sh --uninstall`
(or `sudo ./install.sh --uninstall /usr/local`).

The download is for 64-bit Intel/AMD PCs and needs Qt 6 (6.4 or newer) and
a fairly recent distribution: Ubuntu 24.04 or newer, Debian 13, Fedora 40 or
newer, Arch, or openSUSE Tumbleweed. If Qt 6 is missing, `install.sh` says
which package to install. On older or other systems, build from source.

## Build from source

You need a C++17 compiler, CMake 3.16 or newer and the Qt 6 development
files:

```sh
sudo pacman -S --needed base-devel cmake qt6-base        # Arch
sudo apt install build-essential cmake qt6-base-dev      # Debian / Ubuntu
sudo dnf install gcc-c++ cmake qt6-qtbase-devel          # Fedora
```

Then, from the source folder:

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure   # optional: run the tests
cmake --install build
```

You can also run it without installing: `./build/yahtzee-gs`. For a
system-wide install, use `-DCMAKE_INSTALL_PREFIX=/usr/local` and
`sudo cmake --install build`. To uninstall, delete the files listed in
`build/install_manifest.txt` (`xargs rm -v < build/install_manifest.txt`).

## Layout

- `src/core/` game logic in plain C++: scoring, computer player, game flow
- `src/screen/` the IIgs screen and the original's event handling and drawing
- `src/qt/` the Qt window
- `packaging/` desktop entry, AppStream metadata, icon, and the release's
  `install.sh`
- `tests/` tests; `tests/data/original_vectors.txt` came from running the
  original 65816 code in an emulator
- `tools/` Python tools used to recover the game from the original: disk and
  OMF readers, disassembler, 65816 emulator. They need the original `YAHTZEE`
  file, which is not included; point `YAHTZEE_BIN` at it and run them with
  `PYTHONPATH=tools`.
- `docs/RECOVERED.md` what was learned from the disassembly

## Making a release

Update the version in `CMakeLists.txt` and add a `<release>` entry to
`packaging/io.github.yahtzee_gs.Yahtzee.metainfo.xml`, commit, then push a
tag:

```sh
git tag v1.2
git push origin v1.2
```

GitHub Actions builds and tests it, then creates the release with the
download attached.

## License

The original YAHTZEE v1.1 is by David W. Buell, released as freeware:
"Distribute freely but do not sell". This port follows the same terms under
the [PolyForm Noncommercial License 1.0.0](LICENSE): you may use, copy,
change and share it for any noncommercial purpose, but not sell it.
