"""Capture the real app and verify CRUD, failures and saved language after restart."""

import argparse
import hashlib
import json
import re
import shutil
import sqlite3
import subprocess
import tempfile
import time
from pathlib import Path
from xml.etree import ElementTree as ET

from common import LANGUAGES, ROOT, png_dimensions, source_hashes, virtual_display


class Journey:
    def __init__(self, binary, output, environment, language):
        self.binary = binary
        self.output = output
        self.environment = environment
        self.language = language
        self.process = None
        self.catalog = self.messages(language)
        self.log = (output / "app.log").open("w")

    @staticmethod
    def messages(language):
        return {
            message.findtext("source"): message.findtext("translation")
            for message in ET.parse(
                ROOT / "translations" / f"app_{language}.ts"
            ).findall("./context/message")
        }

    def command(self, *arguments):
        return subprocess.check_output(
            list(arguments), env=self.environment, text=True, stderr=subprocess.DEVNULL
        ).strip()

    def key(self, *keys):
        self.command("xdotool", "key", "--clearmodifiers", *keys)
        time.sleep(0.16)

    def paste(self, value):
        subprocess.run(
            ["xsel", "--clipboard", "--input"],
            input=value,
            text=True,
            env=self.environment,
            check=True,
        )
        self.key("ctrl+v")

    def wait(self, predicate, timeout=8):
        end = time.monotonic() + timeout
        while time.monotonic() < end:
            if self.process and self.process.poll() is not None:
                raise AssertionError(
                    f"App exited unexpectedly: {self.process.returncode}"
                )
            try:
                result = predicate()
                if result:
                    return result
            except (subprocess.CalledProcessError, sqlite3.OperationalError):
                pass
            time.sleep(0.08)
        raise AssertionError("Expected GUI state did not appear")

    def window(self, title):
        identifier = self.wait(
            lambda: self.command(
                "xdotool",
                "search",
                "--all",
                "--onlyvisible",
                "--pid",
                str(self.process.pid),
                "--name",
                "^" + re.escape(title) + "$",
            ).splitlines()[-1]
        )
        self.command("xdotool", "windowfocus", identifier)
        time.sleep(0.2)
        return identifier

    def click(self, window, x, y, twice=False):
        arguments = [
            "xdotool",
            "mousemove",
            "--window",
            window,
            str(x),
            str(y),
            "click",
        ]
        if twice:
            arguments.extend(["--repeat", "2", "--delay", "100"])
        self.command(*arguments, "1")
        time.sleep(0.2)

    def geometry(self, window):
        return {
            key: int(value)
            for key, value in (
                line.split("=", 1)
                for line in self.command(
                    "xdotool", "getwindowgeometry", "--shell", window
                ).splitlines()
            )
        }

    def capture(self, name, store=False):
        destination = self.output / ("screenshots" if store else "verification")
        destination.mkdir(exist_ok=True)
        self.command("xdotool", "mousemove", "1918", "1078")
        time.sleep(0.3)
        path = destination / f"{name}.png"
        self.command("import", "-window", "root", str(path))
        assert png_dimensions(path) == (1920, 1080)

    def rows(self):
        with sqlite3.connect(
            f"file:{self.database}?mode=ro", uri=True, timeout=0.1
        ) as connection:
            return connection.execute(
                "SELECT name,maker,location,note,photo FROM items ORDER BY id"
            ).fetchall()

    def start(self):
        self.process = subprocess.Popen(
            [str(self.binary), "--data-dir", str(self.directory)],
            cwd=self.directory,
            env=self.environment,
            stdout=self.log,
            stderr=self.log,
        )
        self.main = self.window("Collection Showcase")
        self.command("xdotool", "windowactivate", "--sync", self.main)
        self.key("alt+F10")
        time.sleep(0.7)
        if self.binary.name.endswith(".sh"):
            mapped = {
                line.split()[-1]
                for line in Path(f"/proc/{self.process.pid}/maps")
                .read_text()
                .splitlines()
                if any(
                    name in line
                    for name in (
                        "libQt6",
                        "libqsqlite.so",
                        "libqjpeg.so",
                        "libqgtk3.so",
                    )
                )
            }
            assert any("libqsqlite.so" in path for path in mapped)
            assert all(
                Path(path).is_relative_to(self.binary.parent) for path in mapped
            ), mapped

    def stop(self):
        self.command("xdotool", "windowfocus", self.main)
        self.key("ctrl+q")
        self.process.wait(timeout=8)
        assert self.process.returncode == 0
        self.process = None

    def find(self, text):
        self.command("xdotool", "windowfocus", self.main)
        self.key("ctrl+f")
        self.key("BackSpace")
        if text:
            self.paste(text)

    def save_language(self, dialog):
        bounds = self.geometry(dialog)
        self.click(dialog, bounds["WIDTH"] - 65, bounds["HEIGHT"] - 35)

    def action(self, source):
        mnemonic = re.search(r"&(.)", self.catalog[source])[1].lower()
        self.key("alt+" + mnemonic)

    def field(self, source, value):
        self.action(source)
        self.key("ctrl+a")
        self.paste(value)

    def edit(self):
        self.window("Collection Showcase")
        self.action("&Bearbeiten")
        return self.window(self.catalog["Sammlerstück bearbeiten"])

    def photo(self, source=None):
        self.action("&Foto hinzufügen")
        self.window(self.catalog["Foto auswählen"])
        if source is None:
            self.key("Escape")
        else:
            self.key("ctrl+l")
            self.paste(str(source))
            self.key("Return")
            time.sleep(0.8)
        self.window(self.catalog[self.editor_title])

    def discard(self):
        self.key("Escape")
        self.window(self.catalog["Änderungen verwerfen?"])
        self.key("Tab", "space")
        self.window("Collection Showcase")

    def run(self):
        fixture = json.loads(
            (ROOT / "assets/store_demo" / f"{self.language}.json").read_text(
                encoding="utf-8"
            )
        )
        with tempfile.TemporaryDirectory(
            prefix="collection-showcase-preview-"
        ) as temporary:
            self.directory = Path(temporary)
            self.database = self.directory / "sammlervitrine.sqlite3"
            preference = self.directory / "language.txt"
            preference.write_text(self.language + "\n", encoding="utf-8")
            photo_sources = []
            for index, source in enumerate(fixture["photos"]):
                if source:
                    target = self.directory / (str(index) + Path(source).suffix)
                    shutil.copy2(ROOT / "assets/store_demo" / source, target)
                    photo_sources.append(target)
            with sqlite3.connect(self.database) as connection:
                connection.execute(
                    "CREATE TABLE items (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL, maker TEXT NOT NULL DEFAULT '', location TEXT NOT NULL DEFAULT '', note TEXT NOT NULL DEFAULT '', photo BLOB)"
                )
                connection.execute("PRAGMA user_version=1")
                connection.execute("PRAGMA application_id=1398166612")
                connection.executemany(
                    "INSERT INTO items(name,maker,location,note,photo) VALUES(?,?,?,?,?)",
                    [
                        row
                        + [
                            (ROOT / "assets/store_demo" / photo).read_bytes()
                            if photo
                            else None
                        ]
                        for row, photo in zip(fixture["items"], fixture["photos"])
                    ],
                )
            original = self.rows()
            try:
                self.start()
                self.capture("01-collection", store=True)
                self.editor_title = "Sammlerstück bearbeiten"
                editor = self.edit()
                self.command("xdotool", "windowsize", editor, "760", "950")
                self.command("xdotool", "windowmove", editor, "580", "55")
                self.capture("02-editor", store=True)
                self.key("Escape")
                self.find(fixture["search"])
                self.capture("03-search", store=True)
                self.find("")
                self.key("ctrl+comma")
                dialog = self.window(self.catalog["Sprache"])
                self.key("space")
                self.capture("04-language", store=True)
                self.key("Escape", "Escape")

                self.window("Collection Showcase")
                self.key("ctrl+n")
                self.editor_title = "Sammlerstück hinzufügen"
                editor = self.window(self.catalog[self.editor_title])
                self.key("ctrl+s")
                self.capture("validation")
                assert self.rows() == original
                values = [
                    "Étoile 東京 ação",
                    "Müller & Fils",
                    "Regal 7 · São Paulo",
                    "  Grüße, café, coração\n第二行\n",
                ]
                for source, value in zip(
                    (
                        "&Bezeichnung *",
                        "&Marke oder Hersteller",
                        "&Aufbewahrungsort",
                        "&Notiz",
                    ),
                    values,
                ):
                    self.field(source, value)
                self.photo()  # Native chooser cancellation preserves the draft.
                self.photo(photo_sources[0])
                invalid = self.directory / "invalid.png"
                invalid.write_text("Synthetic invalid image")
                self.photo(invalid)
                self.capture("invalid-photo-keeps-draft")
                self.photo(photo_sources[1])
                photo_bytes = photo_sources[1].read_bytes()
                blocker = sqlite3.connect(self.database)
                try:
                    blocker.execute("BEGIN EXCLUSIVE")
                    self.key("ctrl+s")
                    time.sleep(2.4)
                    self.window(self.catalog[self.editor_title])
                    self.capture("database-locked")
                    self.command("xdotool", "windowsize", editor, "440", "380")
                    self.capture("small-editor-error")
                finally:
                    blocker.rollback()
                    blocker.close()
                assert self.rows() == original
                self.key("ctrl+s")
                self.wait(lambda: len(self.rows()) == 6)
                assert self.rows()[-1] == (*values, photo_bytes), self.rows()[-1][:4]

                self.find(values[0])
                self.editor_title = "Sammlerstück bearbeiten"
                self.edit()
                self.field("&Bezeichnung *", values[0] + " +")
                self.key("Escape")
                self.window(self.catalog["Änderungen verwerfen?"])
                self.key("Return")
                self.window(self.catalog[self.editor_title])
                self.key("ctrl+s")
                values[0] += " +"
                self.wait(lambda: self.rows()[-1][0] == values[0])
                self.find(values[0])
                self.edit()
                self.action("Foto &entfernen")
                self.discard()
                assert self.rows()[-1] == (*values, photo_bytes)
                self.edit()
                self.action("Foto &entfernen")
                self.key("ctrl+s")
                self.wait(lambda: self.rows()[-1][4] is None)
                self.edit()
                self.photo(photo_sources[0])
                self.key("ctrl+s")
                self.wait(lambda: self.rows()[-1][4] == photo_sources[0].read_bytes())
                self.window("Collection Showcase")
                self.action("&Löschen")
                self.window(self.catalog["Sammlerstück löschen?"])
                self.key("Return")
                assert len(self.rows()) == 6
                self.action("&Löschen")
                self.window(self.catalog["Sammlerstück löschen?"])
                self.key("Tab", "space")
                self.wait(lambda: len(self.rows()) == 5)
                assert self.rows() == original
                self.find("")

                self.edit()
                self.field("&Notiz", "Grüße, café, coração\nÉtoile 東京 ação")
                self.key("ctrl+s")
                self.wait(
                    lambda: any(
                        row[3].endswith("Étoile 東京 ação") for row in self.rows()
                    )
                )
                preserved = self.rows()
                assert [row[4] for row in preserved] == [row[4] for row in original]
                self.window("Collection Showcase")
                self.key("alt+F10")
                self.command("xdotool", "windowsize", self.main, "560", "520")
                self.capture("small-window")
                self.key("alt+F10")

                preference.unlink()
                preference.mkdir()
                self.key("ctrl+comma")
                dialog = self.window(self.catalog["Sprache"])
                next_language = LANGUAGES[
                    (LANGUAGES.index(self.language) + 1) % len(LANGUAGES)
                ]
                self.key("Home", *(["Down"] * (LANGUAGES.index(next_language) + 1)))
                self.save_language(dialog)
                time.sleep(0.3)
                self.window(self.catalog["Sprache"])
                assert preference.is_dir()
                self.capture("settings-error")
                preference.rmdir()
                self.save_language(dialog)
                self.wait(
                    lambda: (
                        preference.is_file()
                        and preference.read_text().strip() == next_language
                    )
                )
                self.stop()
                assert self.rows() == preserved
                for source in photo_sources:
                    source.unlink()
                self.start()
                self.capture("photo-after-restart")
                self.key("ctrl+n")
                self.window(self.messages(next_language)["Sammlerstück hinzufügen"])
                self.capture("language-after-restart")
                self.key("Escape")
                assert self.rows() == preserved
                self.stop()
                return {
                    "language": self.language,
                    "restart_language": next_language,
                    "processes": 2,
                    "screenshots": 4,
                    "checks": [
                        "translated editor",
                        "required name",
                        "native photo chooser cancellation",
                        "PNG import",
                        "invalid photo keeps draft",
                        "JPEG replacement",
                        "SQLite lock and retry preserves text and photo",
                        "Unicode create and update",
                        "keep editing default",
                        "discard photo removal",
                        "save photo removal",
                        "reimport PNG",
                        "delete confirmation and cancellation",
                        "small windows",
                        "settings failure and retry",
                        "saved language on restart",
                        "unchanged Unicode data and photo bytes after source deletion and restart",
                    ],
                }
            except Exception:
                self.capture("failure")
                raise
            finally:
                if self.process and self.process.poll() is None:
                    self.process.terminate()
                    self.process.wait(timeout=8)
                self.log.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--xvfb", type=Path)
    parser.add_argument("--window-manager", type=Path)
    parser.add_argument("--language", choices=LANGUAGES, action="append")
    parser.add_argument("--clean-environment", action="store_true")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    report = {
        "platform": "Linux X11 preview",
        "dimensions": [1920, 1080],
        "source_hashes": source_hashes(),
        "binary": str(args.binary.resolve()),
        "binary_sha256": hashlib.sha256(args.binary.resolve().read_bytes()).hexdigest(),
        "journeys": [],
    }
    runtime_files = [args.binary.resolve()]
    if args.binary.name.endswith(".sh"):
        runtime_files.extend(
            path
            for path in args.binary.resolve().parent.rglob("*")
            if path.is_file() and path.suffix != ".md" and "docs" not in path.parts
        )
    report["runtime_hashes"] = {
        str(path.relative_to(args.binary.resolve().parent)): hashlib.sha256(
            path.read_bytes()
        ).hexdigest()
        for path in sorted(set(runtime_files))
    }
    with virtual_display(args.xvfb, args.output, args.window_manager) as environment:
        if args.clean_environment:
            for key in list(environment):
                if (
                    key.startswith(("QT_", "QML", "SAMMLERVITRINE_"))
                    or key in ("LD_LIBRARY_PATH", "CMAKE_PREFIX_PATH")
                ) and key not in {
                    "QT_QPA_PLATFORM",
                    "QT_SCALE_FACTOR",
                    "QT_AUTO_SCREEN_SCALE_FACTOR",
                }:
                    environment.pop(key)
            environment["PATH"] = "/usr/bin:/bin"
        for language in args.language or LANGUAGES:
            output = args.output / language
            output.mkdir()
            result = Journey(args.binary.resolve(), output, environment, language).run()
            report["journeys"].append(result)
            print(f"{language}: real GUI and restart passed", flush=True)
    assert report["source_hashes"] == source_hashes(), "Sources changed during capture"
    assert report["runtime_hashes"] == {
        str(path.relative_to(args.binary.resolve().parent)): hashlib.sha256(
            path.read_bytes()
        ).hexdigest()
        for path in sorted(set(runtime_files))
    }, "Runtime changed during verification"
    report["evidence_hashes"] = {
        str(path.relative_to(args.output)): hashlib.sha256(
            path.read_bytes()
        ).hexdigest()
        for path in sorted(args.output.rglob("*"))
        if path.is_file() and path.name != "xvfb.log"
    }
    report["screenshots"] = {
        str(path.relative_to(args.output)): hashlib.sha256(
            path.read_bytes()
        ).hexdigest()
        for path in sorted(args.output.glob("*/screenshots/*.png"))
    }
    (args.output / "report.json").write_text(
        json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )


if __name__ == "__main__":
    main()
