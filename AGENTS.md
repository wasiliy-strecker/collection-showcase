# Collection Showcase

Eigenständige C++20-App mit Qt 6.8.3 Widgets, SQLite und eigenem Git-Repository.
Ergänzend gelten die Workspace-Regeln. Keine anderen Portfolio-Apps verändern.
Stufe 2 bleibt CRUD, Textsuche und genau ein optionales JPEG-/PNG-Foto je Eintrag.
Keine Statuswerte, Checklisten, Konten oder Online-Funktionen ergänzen. Sammlungstransfer ist ausdrücklich beauftragt.

## Architektur und Daten

`src/domain` enthält Werte und Regeln, `src/application` Service, Repository-Vertrag
und Fotoimport, `src/persistence` ausschließlich SQL, `src/ui` ausschließlich Widgets
und deren Koordination. Kein gemeinsames Runtime-Paket. Englische C++-Bezeichner,
deutsche Dokumentation und sechs Oberflächensprachen: de-DE, en-US, es-ES,
fr-FR, it-IT und pt-BR. Neue Meldungen über den App-Katalog übersetzen.
Kennungen, Schema und Foto-BLOBs sind sprachunabhängig. Sprachwechsel gelten
ab Neustart. Repository-Namen und GitHub-Metadaten bleiben englisch.

Alle SQL-Verbindungen gehören dem einzelnen Worker-Thread. Fotoeinlesen,
Validierung und Decodierung laufen ebenfalls dort. QImage darf als Wert über
queued Signale transportiert werden. Widgets und Malen bleiben im GUI-Thread.
Auswahlergebnisse nur bei passender Generation anwenden. Kontextgebundene
Signalverbindungen und QPointer für zerstörte Empfänger beibehalten.

Listen enthalten keine Foto-BLOBs. Vor Bearbeitung vorhandene Fotos vollständig
laden und prüfen. Lesefehler dürfen niemals einen Entwurf ohne gespeichertes Foto
öffnen. Texte und Foto bilden einen gemeinsamen Entwurf und eine Transaktion.
Erfolg erst nach Commit, Fehler erhalten den Entwurf. Alle Abbruchwege schützen
auch reine Fotoänderungen. „Weiterbearbeiten“ bleibt sichere Vorauswahl.

Grenzen: 10 MiB, 6000 Pixel pro Seite, 16 Millionen Pixel. Begrenzt einlesen,
Inhalt prüfen, vollständig decodieren, Originaldateien unverändert lassen.
Keine Nutzerdaten in Tests. Ausschließlich QTemporaryDir oder isolierte
`--data-dir`-Verzeichnisse verwenden. App-ID und SQLite-application_id beibehalten.
Beschädigte und fremde Datenbanken niemals ersetzen.

## Prüfen

```bash
source scripts/env.sh
find src tests -type f \( -name '*.cpp' -o -name '*.h' \) -print0 | xargs -0 clang-format -i
bash scripts/check.sh
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 bash scripts/check.sh asan
```

Maschinenpfade gehören nur in die ignorierte `.tools/env.sh`. Bei UI-Änderungen
zusätzlich echte sichtbare Fenster und native Dialoge prüfen. Siehe
`docs/verification.md`. Bei Paketänderungen den vollständigen Bedienablauf mit
eigener Laufzeit einschließlich Fotoimport und Neustart ohne Bildquelle prüfen.

## Abschluss

Nach erfolgreichen Prüfungen Workspace-Changelog ergänzen und app-lokal committen.
Internationale Vorschauen ausschließlich als Ordner liefern, ohne ZIP oder
anderes Lieferarchiv. Siehe `docs/internationalization.md`.
Kein Root-Repository erstellen. Kein Push und keine Veröffentlichung ohne Auftrag.
Veröffentlichungsreihenfolge: zuerst Microsoft Store, später Mac App Store.
Kostenfreie dynamische Qt-LGPL-Basis ist gewählt. Native Abnahmen, konkrete
Lizenzkonformität der Store-Auslieferung, Signierung und Einreichung gesondert prüfen.

Offizielle Fremdlizenztexte unverändert erhalten. `.gitattributes` nimmt nur
importierte Qt-Lizenzdateien von der Whitespace-Prüfung aus. Eigener Code und
eigene Dokumentation werden weiterhin vollständig geprüft.

## Gemeinsame C++-Verbesserungen, 1. November 2026

Der öffentliche Produktname ist **Collection Showcase** in allen sechs Sprachen.
Bestehende App-Kennungen, Datenpfade und SQLite-Schemata beibehalten.
Der Nutzer bestätigt für alle fünf C++-Apps die unterstützte Systemsprache beim
Erststart mit Englisch als Fallback. Explizite Sprachwahlen bleiben erhalten.
Dies ersetzt die allgemeine Englisch-Vorgabe ausschließlich für diese C++-Reihe.

Info enthält Über die App, Datenschutz und Lizenzen mit roten Schließen-Buttons.
Die Original-Lizenztexte werden offline eingebettet. Sammlungstransfer per JSON
ist in dieser Stufe ausdrücklich beauftragt, einschließlich aller vorhandenen
Fotos und gegebenenfalls Status und Checklisten. Kein Austausch zwischen Apps.
Import ergänzt fehlende Exemplare atomar. Vorhandene Einträge bleiben erhalten.
Dateien verwenden das eigene Format in Version 1. Höchstens 10.000 Einträge,
256 MiB und 10.000 Schritte je Projekt. Fotos behalten die bisherigen Grenzen.

Auf macOS wird zuerst der Exportordner nativ gewählt, dann der Dateiname.
Nur ausgewählte Dateien und Ordner erhalten Lese-/Schreibzugriff. Neue Dateien
werden ohne Überschreiben veröffentlicht. Ohne Hardlinks darf eine Datei exklusiv
neu angelegt und kopiert werden. Nach hartem Abbruch kann dabei eine unvollständige
neue Datei zurückbleiben. Bestehende Ziele werden ausschließlich nach Bestätigung
und erneuter Prüfung atomar ersetzt.

## Farbauswahl ab Stufe 2, 1. November 2026

Auf ausdrücklichen Nutzerauftrag wird die optionale Farbauswahl aus Yarn Box
auf alle vier C++-Apps ab Collection Showcase übertragen. Die Begrenzung auf
die bisherigen Hauptfunktionen bleibt bestehen. Farben gehören zum atomaren
Entwurf und zum JSON-Transfer. Schema 2 ergänzt das Farbfeld nach vollständiger
Prüfung und mit atomarer Migration von Schema 1. App-IDs, Datenpfade und
vorhandene Inhalte bleiben erhalten. Neue Werte verwenden #RRGGBB oder
#RRGGBBAA. Alte Exporte ohne Farbe bleiben lesbar. Alle sechs Sprachen abdecken.
