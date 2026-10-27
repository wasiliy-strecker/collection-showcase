# Prüfbericht

Stand: 27. Oktober 2026, SammlerVitrine 0.1.0. Alle nachfolgend als bestanden
bezeichneten Prüfungen wurden für diese App ausgeführt. Zahlen und Ergebnisse
wurden nicht aus BrettspielRegal übernommen.

## Toolchain und Datenpfad

Ubuntu 24.04.4 LTS x64, GCC 13.3.0, Qt 6.8.3, CMake 4.4.4, Ninja 1.11.1,
clang-format 23.1.3 und clang-tidy 22.1.8. QSQLITE meldet SQLite 3.49.1.
Das dynamische Qt-SDK ist mit GCC 10.3.1 vorgebaut, die eigenen C++20-Targets
wurden mit GCC 13.3.0 gebaut. Build, Link und ausgeführte Prüfungen bestätigen
die verwendete ABI-Kombination auf diesem Linux-System.

Qt-SDK-SPDX-Metadaten und Quellen von v6.8.3 bestätigen libjpeg-turbo 3.1.0 und
libpng 1.6.47. JPEG wird als Plugin geladen, PNG ist in diesem Qt Gui eingebaut.
Das Qt-Basisarchiv wurde erneut heruntergeladen und per SHA-256 geprüft.
Siehe [Quellenherkunft](../licenses/PROVENANCE.md).

Ein separater QCoreApplication-/QStandardPaths-Probeaufruf hat als normalen
App-Pfad `/home/unknown/.local/share/de.appfabrik.sammlervitrine` ermittelt.
Dabei wurde keine normale Benutzerdatenbank geöffnet. Der SQLite-Versionsaufruf
verwendete ausschließlich `:memory:`. Alle übrigen Tests nutzten QTemporaryDir
oder vom Testhelfer neu erzeugte temporäre `--data-dir`-Verzeichnisse.

## Automatische Prüfungen

```bash
bash scripts/check.sh
source scripts/env.sh
cmake --preset release
cmake --build --preset release --parallel 4
ctest --preset release -V
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 bash scripts/check.sh asan
```

**75 fachliche Testausführungen** bestehen in vier CTest-Suiten. Qt Test meldet
zusätzlich acht erfolgreiche Initialisierungs-/Abschlussaufrufe. Keine Fehler
und keine übersprungenen Fälle auf der geprüften Linux-Umgebung.

| Suite | Fachliche Ausführungen | Abdeckung |
| --- | --- | --- |
| Domain | 16 | Pflichtfeld einschließlich Unicode-Leerraum, optionale Felder, mehrzeilige Notizen, Suche über alle Textfelder, Umlaute, Unicode-Normalisierung, stabile Sortierung und isolierte Pfade |
| Repository | 20 | CRUD und IDs, gleichnamige Einträge, Persistenz, SQL-ähnliche Texte, atomare Änderungen, Sperren, Wiederholung, schreibgeschützte/beschädigte/fremde/neuere Datenbanken, Schemafehler, Instanzschutz und Foto-BLOBs |
| Dialoge | 21 | Echte Qt-Widgets, Text-/Fotoentwurf, Verwerfschutz, sichere Vorauswahl, Speicherfehler und Wiederholung, Auswahlwechsel, Hintergrundarbeit, zerstörte Empfänger und kleine Dialoge |
| Foto | 18 | JPEG und PNG nach Inhalt, falsche Endung, beschädigte Dateien, nicht unterstütztes BMP, Byte-/Seiten-/Pixelgrenzen, begrenzter Datenstrom und EXIF-Orientierung |

Die Repository-Tests laden nach Wiederöffnen Fotos ohne Quelldatei und vergleichen
die gespeicherten Bytes. Gleichnamige Datensätze behalten unterschiedliche Fotos.
Die Listenabfrage enthält ausschließlich Texte und Foto-Kennzeichnung, keine
Foto-BLOBs. Überlange, leere oder falsch typisierte gespeicherte Bildwerte werden
als Fehler behandelt.

Ein externer SQLite-Leser verhindert gezielt den Commit einer vollständigen
Text-/Fotoänderung. Nach Rollback bleibt der komplette vorherige Datensatz
unverändert. Derselbe Entwurf lässt sich anschließend erfolgreich speichern.
Auch fehlgeschlagene Fotoentfernung und fehlgeschlagenes Löschen erhalten Daten.
Exklusive Sperren und synthetische Schreibfehler ergänzen diese Prüfung.

Fototests prüfen 6000 gegen 6001 Pixel sowie genau 16 Millionen gegen mehr Pixel.
Ein begrenzter Stream prüft Wachstum unabhängig von der ursprünglichen Dateigröße.
PNG-Prüfsummenfehler, fehlende Endstücke und beschädigtes JPEG mit vorhandenem
Endmarker werden abgewiesen. Erwartete Decoderdiagnosen dieser absichtlich
beschädigten Bilder sind keine Compiler- oder Sanitizerbefunde.

GUI-Tests prüfen ausschließliches Hinzufügen, Ersetzen und Entfernen von Fotos,
veränderte Bytes bei gleichem Dateinamen, exakte Wiederherstellung des ursprünglichen
Fotozustands sowie Speicherfehler mit erhaltenem Bild. Ein verzögertes Repository
belegt die Threadzuordnung und verhindert reproduzierbar veraltete Vorschauen.
Ein Foto-Lesefehler verhindert das Öffnen eines leeren Bearbeitungsentwurfs.
Zerstörte Fenster und Editorinstanzen ignorieren späte Resultate sicher.

Formatter, strenge Compilerwarnungen als Fehler und clang-tidy bestehen ohne
Befunde im eigenen Code. AddressSanitizer, LeakSanitizer und UndefinedBehaviorSanitizer
bestehen ohne Unterdrückungsdateien. Qt und das darin enthaltene SQLite sind
vorgebaute Bibliotheken und selbst nicht mit diesen Sanitizern instrumentiert.
Die automatisierten Dialogtests verwenden Qt offscreen. Die nachfolgende sichtbare
Abnahme ist eine zusätzliche eigenständige Prüfung.

Ruff-Prüfung und Formatprüfung der Python-Helfer, `bash -n` der Shell-Helfer,
JSON-Prüfung der Presets sowie XML-Prüfung von Manifest, Plist und Entitlements
bestehen. PowerShell, Windows-Ressourcencompiler und Apple-Werkzeuge wurden auf
diesem Linux-Rechner nicht ausgeführt.

## Sichtbare Bedienabnahme

Die App wurde in einem eigenen, auf dem Desktop sichtbaren Xephyr-X11-Fenster
auf DISPLAY `:92` gestartet. Echte Qt-Fenster und der native GTK3-Dateidialog
wurden über Tastatur/Maus bedient. Der Helfer verwendet ausschließlich
Fenster-IDs des gestarteten Prozesses mit passendem Fenstertitel. Unicode-Eingaben
werden über die Zwischenablage dieses isolierten Displays eingefügt.
Screenshots wurden angesehen und die Datenbank unabhängig mit Python SQLite
kontrolliert. Fensterlose Tests ersetzen diese Abnahme nicht.

```bash
Xephyr :92 -screen 1400x1050 -ac -noreset
source scripts/env.sh
DISPLAY=:92 python3 scripts/gui_smoke.py --binary build/dev/sammlervitrine --output build/manual/dev-complete
```

Die erste Zeile benötigt eine eigene laufende Terminalsitzung. Ein freies Display
verwenden. Der Testhelfer benötigt Python 3, xdotool, xsel und ImageMagick.
Diese Werkzeuge gehören nicht zur ausgelieferten App. Die App wird ausschließlich
mit einem vom Helfer angelegten temporären Datenverzeichnis gestartet.

| Sichtbarer Ablauf | Ergebnis |
| --- | --- |
| Leere Sammlung und Speichern ohne Bezeichnung | Eigene Hinweise, kein Datensatz gespeichert |
| Vollständiger Textentwurf mit mehrzeiliger Unicode-Notiz | Exakter Inhalt nach Speichern unabhängig gelesen |
| Native Dateiauswahl abbrechen | Entwurf unverändert |
| PNG importieren, beschädigte Datei als Ersatz wählen | Bisherige Texte und Foto bleiben erhalten |
| Notizsuche mit Umlaut, Suche ohne Treffer, Suche leeren | Korrekte Liste und unterscheidbare Leerzustände |
| JPEG-Ersatz als reine Fotoänderung, Escape und Enter | „Weiterbearbeiten“ bleibt sichere Vorauswahl |
| Ersatz und Entfernung verwerfen | Gespeichertes Original bleibt unverändert |
| Text und Foto unter echter exklusiver SQLite-Sperre speichern | Fehler verständlich, kompletter Entwurf erhalten |
| Sperre lösen und erneut speichern | Änderung mit derselben ID vollständig übernommen |
| Gleichnamiger Eintrag mit anderem Foto | Zwei unabhängige IDs und BLOBs |
| Prozessneustart, anschließend Quellen löschen und erneut starten | Beide Bilder und alle Texte weiterhin verfügbar |
| Foto tatsächlich entfernen | Nur gewählte ID verliert ihr Foto |
| Löschabbruch, bestätigtes Löschen beider Einträge | Abbruch erhält Daten, Bestätigung löscht gezielt, Leerzustand kehrt zurück |

Zusätzlich wurde ein echter X11-Dialogtest mit 18-Punkt-Schrift und einem Fenster
von 400 × 340 Pixeln ausgeführt. Der Formularbereich scrollt, Speichern und
Abbrechen bleiben sichtbar. Die sichtbare Abnahme enthält außerdem einen kleinen
Dialog mit Speicherfehler. Die Meldung ist scrollbar und die Aktionen bleiben
erreichbar. Ausgewählte Belege liegen unter [screenshots](screenshots/).

## Eigenständiges Paket

Ein Linux-x64-Prüfpaket wurde mit `scripts/package_linux.py --stage-only` erzeugt,
in ein neues Verzeichnis außerhalb des Projekts kopiert und dort getestet.
Qt-, QML-, SDK- und externe Bibliotheksvariablen wurden entfernt. PATH war auf
`/usr/bin:/bin` begrenzt. Der Launcher setzt ausschließlich seine eigenen
Laufzeitpfade. Der vollständige oben beschriebene Ablauf besteht auch mit diesem
Paket, einschließlich nativer Dateiauswahl, JPEG/PNG, echter Datenbanksperre,
Wiederholung, unabhängiger Löschung und zwei Prozessneustarts.

```bash
DISPLAY=:92 python3 scripts/gui_smoke.py --binary /absoluter/pfad/zum/pruefpaket/SammlerVitrine.sh --output build/manual/package-complete --clean-environment
```

Die Prozessabbildung unter `/proc/<pid>/maps` belegt die geladenen Qt- und
ICU-Bibliotheken, QSQLITE, JPEG und GTK3-Plattformhelfer aus dem eigenen Paket.
Die Dateien `build/manual/package-complete/result.json` und
`loaded-libraries.json` dokumentieren diesen Lauf. Übliche Ubuntu-Systembibliotheken
bleiben Voraussetzungen. Nach Abschluss der Dokumentation wird das Lieferarchiv
unter `build/releases` aus demselben geprüften Release-Binary und denselben
Laufzeitbibliotheken erstellt. Bibliotheks- und Binary-Prüfsummen werden verglichen.

## Grenzen und offene Schritte

Windows-x64-ZIP/MSIX und macOS-App-Bundle/DMG sind vorbereitet. Auf diesem
Linux-Rechner wurden keine nativen Windows- oder macOS-Pakete gebaut oder gestartet.
Offen sind dort native Dateidialoge, Windows-ACLs, Installation und Updates,
macOS-Sandbox mit lesendem Fotoimport, tatsächliche Datenpfade, Skalierung und
plattformspezifische Tastenkürzel. Beide Mac-Architekturen brauchen eine Abnahme.

Die vorbereiteten GitHub-Workflows wurden nicht ausgeführt. Echte Store-Identität,
Publisher, Entwicklerteam, Signierung, konkrete Store-Lizenzprüfung, Metadaten,
Einreichung und Veröffentlichung bleiben offen. Die Reihenfolge bleibt zuerst
Microsoft Store und später Mac App Store. Es wurde nichts gepusht oder veröffentlicht.
