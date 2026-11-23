#!/bin/sh
# Install or remove Yahtzee from a downloaded release.
#
#   ./install.sh                 install for you only (into ~/.local)
#   sudo ./install.sh /usr/local install for all users
#   ./install.sh --uninstall     remove it again (same prefix as installed)
set -eu

APP_ID=io.github.yahtzee_gs.Yahtzee
HERE=$(cd "$(dirname "$0")" && pwd)

uninstall=0
if [ "${1:-}" = "--uninstall" ]; then uninstall=1; shift; fi
PREFIX=${1:-"$HOME/.local"}

BIN="$PREFIX/bin/yahtzee-gs"
DESKTOP="$PREFIX/share/applications/$APP_ID.desktop"
META="$PREFIX/share/metainfo/$APP_ID.metainfo.xml"
ICON="$PREFIX/share/icons/hicolor/scalable/apps/$APP_ID.svg"

refresh() {
    command -v update-desktop-database >/dev/null 2>&1 &&
        update-desktop-database -q "$PREFIX/share/applications" 2>/dev/null || true
    command -v gtk-update-icon-cache >/dev/null 2>&1 &&
        gtk-update-icon-cache -q -t "$PREFIX/share/icons/hicolor" 2>/dev/null || true
}

if [ "$uninstall" = 1 ]; then
    rm -fv "$BIN" "$DESKTOP" "$META" "$ICON"
    refresh
    echo "Yahtzee removed from $PREFIX."
    exit 0
fi

# Check that Qt 6 is installed before copying anything.
if command -v ldd >/dev/null 2>&1 && ldd "$HERE/bin/yahtzee-gs" | grep -q 'not found'; then
    echo "Yahtzee needs the Qt 6 libraries, which are missing:" >&2
    ldd "$HERE/bin/yahtzee-gs" | grep 'not found' >&2
    cat >&2 <<'EOF'

Install them with your package manager, then run this script again:
  Arch / Manjaro:   sudo pacman -S qt6-base
  Debian / Ubuntu:  sudo apt install libqt6widgets6 qt6-wayland
  Fedora:           sudo dnf install qt6-qtbase-gui
  openSUSE:         sudo zypper install libQt6Widgets6
EOF
    exit 1
fi

install -Dm755 "$HERE/bin/yahtzee-gs" "$BIN"
install -Dm644 "$HERE/share/metainfo/$APP_ID.metainfo.xml" "$META"
install -Dm644 "$HERE/share/icons/hicolor/scalable/apps/$APP_ID.svg" "$ICON"
# Point the launcher entry at the full path, so it works even when
# $PREFIX/bin is not on the desktop session's PATH.
mkdir -p "$(dirname "$DESKTOP")"
sed "s|^Exec=.*|Exec=$BIN|" "$HERE/share/applications/$APP_ID.desktop" > "$DESKTOP"
chmod 644 "$DESKTOP"
refresh

echo "Yahtzee installed to $PREFIX."
echo "Start it from your application menu (under Games) or run: $BIN"
