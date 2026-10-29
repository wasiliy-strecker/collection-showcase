# SammlerVitrine

Eigenständige C++20-App mit Qt 6.8.3 Widgets, SQLite und eigenem Git-Repository.
Ergänzend gelten die Workspace-Regeln. Keine anderen Portfolio-Apps verändern.
Stufe 2 bleibt CRUD, Textsuche und genau ein optionales JPEG-/PNG-Foto je Eintrag.
Keine Statuswerte, Checklisten, Exporte, Konten oder Online-Funktionen ergänzen.

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
