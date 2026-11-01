# Paketierung und spätere Store-Schritte

Aktueller öffentlicher Name: **Collection Showcase**. Alle sechs Oberflächensprachen bleiben
verfügbar. Neue Installationen verwenden die unterstützte Systemsprache mit Englisch
als Fallback. Die technischen Kennungen und Datenverzeichnisse bleiben unverändert.
macOS erlaubt Zugriff auf ausgewählte Dateien und Ordner mit Lese-/Schreibrechten.
Die Export-Ordnerwahl gewährt Zugriff auch für temporäre Nachbardateien.
Native macOS-Sandbox und ein physischer exFAT-Datenträger benötigen eigene Abnahme.


Veröffentlichungsreihenfolge: **zuerst Microsoft Store, danach Mac App Store**.
Der Auftrag umfasst Entwicklung und vorbereitete Paketierung. Keine
Store-Einreichung, Veröffentlichung, kostenpflichtige Lizenz oder Dienstleistung.
Die kostenfreie dynamische Qt-LGPL-Basis ist gewählt. Die konkrete
Lizenzkonformität jedes späteren Store-Auslieferungswegs bleibt gesondert zu prüfen.

## Gemeinsame Laufzeit

Die Pakete verwenden Qt 6.8.3 aus derselben ABI-kompatiblen SDK-Installation
wie der Build. Sie enthalten Core, Gui, Widgets, Sql, Plattform-Plugin,
QSQLITE, JPEG-Plugin und die in Qt Gui integrierte PNG-Unterstützung. Sechs
App-Kataloge und die Qt-Basisübersetzungen für Deutsch, Spanisch, Französisch,
Italienisch und brasilianisches Portugiesisch sind im Binary eingebettet.
Englisch verwendet die Qt-Originaltexte. Native Systemdialoge folgen zusätzlich
der Betriebssystemsprache. Lizenztexte, Komponentenhinweise und
SDK-SPDX-Metadaten werden beigelegt.
Qt Test wird nicht ausgeliefert. Ein Nutzer benötigt weder Entwicklerwerkzeuge
noch ein separates Qt oder eine andere Portfolio-App.

Vor Paketierung müssen Compilerwarnungen, Formatter und Tests bestehen.
Internationale Vorschauen werden auf Nutzerwunsch ausschließlich als Ordner
ohne ZIP oder anderes Lieferarchiv erstellt. Die vorhandenen Helfer für
spätere native Pakete bleiben separate Releasewege. Lieferverzeichnisse
enthalten App, Version, System und Architektur.
Die Skripte ersetzen keine vorhandene Lieferung. Eine ungeteilte Zwischenstufe
kann für einen erneuten Prüflauf separat archiviert werden. Bereits übergebene
Artefakte bleiben erhalten. SHA-256-Dateien dienen der Integritätskontrolle.

## Linux-Prüfpaket

```bash
source scripts/env.sh
cmake --preset release
cmake --build --preset release --parallel 4
ctest --preset release
python3 scripts/package_linux.py --qt "$SAMMLERVITRINE_QT_ROOT" \
  --stage-only --output-dir build/staging/international \
  --cursor-library /pfad/libxcb-cursor.so.0 \
  --cursor-license /pfad/copyright
```

Die Cursor-Parameter können entfallen, wenn Bibliothek und Lizenztext systemweit
an den üblichen Ubuntu-Pfaden liegen. Das Skript bündelt Qt, ICU 73, QSQLITE,
JPEG, XCB, Eingabehelfer und das GTK3-Plattformthema für die native Dateiauswahl.
PNG ist im tatsächlich verwendeten Qt Gui vorhanden. Der Paketlauncher setzt
seine eigenen Bibliotheks-/Pluginpfade und das GTK3-Thema.

```bash
./build/staging/international/sammlervitrine-0.1.0-linux-x64/SammlerVitrine.sh --data-dir /tmp/sammlervitrine-pakettest
```

Das ist ein Ubuntu-24.04-x64-Prüfpaket. Übliche Systembibliotheken wie glibc,
X11, GTK3, Fontconfig und OpenGL bleiben Voraussetzungen. Kein universelles
Linux-Format und kein zusätzlich geplantes Linux-Store-Produkt.
Das Skript prüft mit ldd, dass seine Bibliotheken auflösbar sind und nicht
unbeabsichtigt auf das Entwicklungs-SDK zeigen.

Das bereitgestellte Paket außerhalb des Projekts mit bereinigter Umgebung starten.
Native Dateiauswahl, JPEG und PNG, Fotoanzeige, CRUD, Speicherfehler und
Prozessneustart prüfen. Danach die Bildquellen löschen und erneut starten.
`--version` allein ist keine Abnahme. Siehe [Prüfbericht](verification.md).

Die app-lokale Aufnahme und Zusammenstellung von sechs Store-Textpaketen und
24 echten Vorschau-Bildern beschreibt [Internationalisierung](internationalization.md).

## Windows x64 und Microsoft Store

Vorgesehen sind Visual Studio 2022 x64, Windows-SDK, CMake, Ninja und
Qt 6.8.3 MSVC 2022 x64. In der Developer-PowerShell:

```powershell
$env:SAMMLERVITRINE_QT_ROOT = 'C:\Qt\6.8.3\msvc2022_64'
.\scripts\package_windows.ps1 -Format zip
```

Der Helfer baut und testet, führt windeployqt aus und übernimmt QSQLITE explizit.
Er prüft JPEG, Plattform-Plugin und Qt-Bibliotheken. Die VC143-CRT-DLLs aus
`VCToolsRedistDir` liegen app-lokal bei. Deren Distributionsbedingungen und der
native Start ohne installiertes SDK sind auf Windows zu prüfen.

Für ein späteres MSIX werden echte Werte aus dem eigenen Partner-Center-Produkt
übergeben. Die interne App-ID ist kein Ersatz für diese Store-Identität:

```powershell
.\scripts\package_windows.ps1 -Format msix -IdentityName '<Paketname aus Partner Center>' -Publisher '<Publisher aus Partner Center>' -PublisherDisplayName '<Anzeigename>'
```

Fehlende Werte brechen ab. Keine fremde Paketidentität und keine erfundenen
Publisher-Werte sind eingebaut. App-Anzeigename und Manifest-Anwendungs-ID sind
SammlerVitrine, die EXE heißt `sammlervitrine.exe`. Die Vorlage verwendet
`Windows.FullTrustApplication` und `runFullTrust`. `makeappx.exe` erstellt das
Paket. Signierung und Upload werden nicht ausgeführt.

Vor Einreichung offen: eigenes Produkt registrieren, tatsächliche Identität
festlegen, konkrete LGPL/MSIX-Distribution prüfen, Metadaten und
Datenschutzauskunft erstellen, native Abnahme durchführen und Zertifizierung
abschließen. Lokales Sideloading braucht einen gesonderten Testsignierungsweg.
Die spätere Store-Signierung ist kein Anlass, private Schlüssel zu committen.

## macOS und späterer Mac App Store

Vorgesehen sind Xcode 15 oder neuer und Qt 6.8.3 macOS. Deployment-Target ist
macOS 12. Intel und Apple Silicon müssen jeweils nativ abgenommen werden:

```bash
export SAMMLERVITRINE_QT_ROOT=/pfad/Qt/6.8.3/macos
bash scripts/package_macos.sh
```

Der Helfer baut und testet nativ, erzeugt `SammlerVitrine.app` und bündelt Qt
mit macdeployqt. QSQLITE, JPEG und Cocoa werden geprüft. Übersetzungen,
Lizenzhinweise und eigenes Icon werden ergänzt. Bundle-ID ist
`de.appfabrik.sammlervitrine`. Eine DMG dient der Testlieferung.
Ad-hoc-Signierung ermöglicht lokale Tests, ist aber keine vertrauenswürdige
Veröffentlichungssignatur. `-appstore-compliant` ist keine Store-Freigabe.

`packaging/macos/app-store.entitlements` bereitet App Sandbox und ausschließlich
`com.apple.security.files.user-selected.read-only` für den einmaligen Fotoimport
über den nativen Dialog vor. Dauerhafter Zugriff, Security-Scoped Bookmarks,
Netzwerkrechte und Schreibrechte auf die Quelle werden nicht benötigt.
Der Import aus dem Hintergrundthread und die Dateiauswahl müssen im tatsächlich
signierten Sandbox-Bundle auf einem Mac abgenommen werden.

QStandardPaths liefert in der Sandbox einen Pfad im eigenen App-Container,
typischerweise unter `~/Library/Containers/de.appfabrik.sammlervitrine/Data/`.
Den vollständigen Datenpfad dort in „Über die App“ prüfen. Keine automatische
Migration anderer Sammlungen. `--data-dir` muss bei Sandbox-Tests zugänglich sein.

Offen bleiben Apple-Team, Provisioning, echte Signierung aller Bundleteile,
konkrete LGPL/Store-Distributionsprüfung, Sandbox-Abnahme und Store-Einreichung.
Eine notarisierte DMG für Direktvertrieb ist ein anderer Weg und ersetzt keine
Mac-App-Store-Abnahme. Die Microsoft-Store-Veröffentlichung hat Vorrang.

## Lizenzbasis und native Abnahme

Qt-Bibliotheken sind dynamisch eingebunden und stehen unter LGPL-3.0. Passende
Qt-Quellen, Bauhinweise, Bibliotheksaustausch und die Untersuchung von Änderungen
müssen bei der konkreten Auslieferung berücksichtigt werden. Paketformat,
Signierung, Store-Bedingungen und sonstige Nutzungsbedingungen müssen gemeinsam
bewertet werden. Diese Vorbereitung behauptet keine pauschale Store-Eignung.
Siehe [Komponentenhinweise](../THIRD_PARTY_NOTICES.md) und
[Quellenherkunft](../licenses/PROVENANCE.md).

Auf jedem Zielsystem mit frischem Benutzerprofil ohne SDK prüfen: nativer
Dateidialog und Abbruch, JPEG-/PNG-Import und Orientierung, vollständige Anzeige,
CRUD mit gleichnamigen IDs, Unicode-Suche, Verwerfschutz, Fehler und Wiederholung,
Neustart ohne Bildquelle, große Schrift, kleine Fenster und Tastaturbedienung.
Zusätzlich Installation/Update ohne Datenverlust, tatsächliche Datenpfade,
Bilddecoderherkunft und OS-Zugriffsrechte kontrollieren.

Der vorbereitete GitHub-Workflow baut und testet auf Linux, Windows und macOS.
Manuell ausgelöste Läufe können Testartefakte erzeugen, veröffentlichen aber
keinen Release und reichen nichts bei Stores ein. Er wurde hier nicht auf GitHub
ausgeführt. Beide Mac-Architekturen bleiben gesonderte native Prüfziele.

## Offizielle Quellen

Grundlagen der ursprünglichen Umsetzung vom 27. Oktober 2026. Vor einem
nativen Release erneut bewerten:

[Qt 6.8 Windows-Deployment](https://doc.qt.io/qt-6.8/windows-deployment.html),
[Qt 6.8 macOS-Deployment](https://doc.qt.io/qt-6.8/macos-deployment.html),
[Qt-LGPL-Pflichten](https://www.qt.io/development/open-source-lgpl-obligations),
[Microsoft MSIX-Komponenten](https://learn.microsoft.com/en-us/windows/msix/desktop/desktop-to-uwp-manual-conversion),
[Apple lesender Zugriff auf ausgewählte Dateien](https://developer.apple.com/documentation/bundleresources/entitlements/com.apple.security.files.user-selected.read-only).

Die rollenden Qt-6.8-Webseiten können neuere Patchstände nennen. Für konkrete
Decoder-Versionen wurden deshalb zusätzlich Qt-Quellen und SPDX-Metadaten
von **v6.8.3** geprüft. Das lokale SDK enthält libjpeg-turbo 3.1.0 und libpng 1.6.47.
