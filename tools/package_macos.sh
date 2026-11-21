#!/usr/bin/env bash
# Bundle only the Widgets and audio plugins used by this application.
set -euo pipefail
app="$1"
base=$(brew --prefix qtbase)
media=$(brew --prefix qtmultimedia)
plugins=$("$base/bin/qmake" -query QT_INSTALL_PLUGINS)
mkdir -p "$app/Contents/PlugIns/platforms" "$app/Contents/PlugIns/multimedia" "$app/Contents/Resources/licenses"
cp "$plugins/platforms/libqcocoa.dylib" "$plugins/platforms/libqoffscreen.dylib" "$app/Contents/PlugIns/platforms/"
cp "$plugins"/multimedia/*.dylib "$app/Contents/PlugIns/multimedia/"
args=(-no-plugins -always-overwrite "-libpath=$base/lib" "-libpath=$media/lib" "-libpath=$(brew --prefix)/lib")
while IFS= read -r plugin; do args+=("-executable=$plugin"); done < <(find "$app/Contents/PlugIns" -name '*.dylib')
"$base/bin/macdeployqt" "$app" "${args[@]}"
printf '[Paths]\nPlugins = PlugIns\n' > "$app/Contents/Resources/qt.conf"
cp CREDITS.md THIRD_PARTY_NOTICES.md docs/NATIVE_PORT.md "$app/Contents/Resources/"
cp -R licenses/qt "$app/Contents/Resources/licenses/"
for dep in qtbase qtmultimedia $(brew deps qtbase qtmultimedia); do
    cellar=$(brew --prefix "$dep")
    dest="$app/Contents/Resources/licenses/$dep"
    mkdir -p "$dest"
    if [ -d "$cellar/.brew" ]; then cp "$cellar"/.brew/*.rb "$dest/"; fi
    find -L "$cellar" -maxdepth 4 -type f \( -iname '*license*' -o -iname '*copying*' \) -exec cp {} "$dest/" \;
done
python3 tools/check_macos_bundle.py "$app"
codesign --force --deep --sign - "$app"
codesign --verify --deep --strict "$app"
