#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
source scripts/env.sh
: "${SAMMLERVITRINE_QT_ROOT:?Qt-6.8.3-SDK angeben}"
sammler_arch="$(uname -m)"
sammler_version="$(sed -n 's/project(SammlerVitrine VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt)"
sammler_name="sammlervitrine-$sammler_version-macos-$sammler_arch"
sammler_stage="$PWD/build/releases/$sammler_name"
sammler_dmg="$PWD/build/releases/$sammler_name.dmg"
if [[ -e "$sammler_stage" || -e "$sammler_dmg" ]]; then
  echo "Lieferung existiert bereits: $sammler_name" >&2
  exit 1
fi
cmake --preset release -DBUILD_TESTING=ON -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 "-DCMAKE_OSX_ARCHITECTURES=$sammler_arch"
cmake --build --preset release --parallel 4
ctest --preset release
mkdir -p "$sammler_stage"
cp README.md THIRD_PARTY_NOTICES.md "$sammler_stage/"
cp -R docs "$sammler_stage/docs"
cp -R build/release/sammlervitrine.app "$sammler_stage/Collection Showcase.app"
sammler_app="$sammler_stage/Collection Showcase.app"
"$SAMMLERVITRINE_QT_ROOT/bin/macdeployqt" "$sammler_app" -always-overwrite -appstore-compliant
test -f "$sammler_app/Contents/PlugIns/sqldrivers/libqsqlite.dylib"
test -f "$sammler_app/Contents/PlugIns/platforms/libqcocoa.dylib"
test -f "$sammler_app/Contents/PlugIns/imageformats/libqjpeg.dylib"
mkdir -p "$sammler_app/Contents/Resources/translations"
cp "$SAMMLERVITRINE_QT_ROOT/translations/qtbase_de.qm" "$sammler_app/Contents/Resources/translations/"
cat > "$sammler_app/Contents/Resources/qt.conf" <<'QTCONF'
[Paths]
Plugins=PlugIns
Translations=Resources/translations
QTCONF
cp -R licenses "$sammler_app/Contents/Resources/licenses"
cp -R "$SAMMLERVITRINE_QT_ROOT/sbom" "$sammler_app/Contents/Resources/licenses/qt-sdk-sbom"
cp THIRD_PARTY_NOTICES.md "$sammler_app/Contents/Resources/"
sammler_icons="$PWD/build/SammlerVitrine.iconset"
mkdir -p "$sammler_icons"
for sammler_size in 16 32 128 256 512; do
  sips -z "$sammler_size" "$sammler_size" assets/branding/icon-1024.png --out "$sammler_icons/icon_${sammler_size}x${sammler_size}.png" >/dev/null
  sammler_double=$((sammler_size * 2))
  sips -z "$sammler_double" "$sammler_double" assets/branding/icon-1024.png --out "$sammler_icons/icon_${sammler_size}x${sammler_size}@2x.png" >/dev/null
done
iconutil -c icns "$sammler_icons" -o "$sammler_app/Contents/Resources/SammlerVitrine.icns"
/usr/libexec/PlistBuddy -c "Add :CFBundleIconFile string SammlerVitrine.icns" "$sammler_app/Contents/Info.plist"
# Ad-hoc signing permits local Apple-Silicon testing. It is not a distribution identity.
codesign --force --deep --sign - "$sammler_app"
hdiutil create -volname "Collection Showcase" -srcfolder "$sammler_stage" -ov -format UDZO "$sammler_dmg"
shasum -a 256 "$sammler_dmg" > "$sammler_dmg.sha256"
echo "$sammler_dmg"
echo "Native GUI-Abnahme und spätere Store-Signierung bleiben erforderlich."
