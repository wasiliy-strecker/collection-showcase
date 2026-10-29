"""Exercise real X11 windows using only synthetic data in a temporary directory."""

import argparse
import json
import os
import sqlite3
import subprocess
import tempfile
import time
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--clean-environment", action="store_true")
    args = parser.parse_args()
    binary = args.binary.resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    environment = dict(os.environ)
    if args.clean_environment:
        for key in list(environment):
            if key.startswith(("QT_", "QML_", "SAMMLERVITRINE_")) or key in (
                "LD_LIBRARY_PATH",
                "CMAKE_PREFIX_PATH",
            ):
                environment.pop(key)
        environment["PATH"] = "/usr/bin:/bin"
    environment["QT_QPA_PLATFORM"] = "xcb"
    environment["QT_SCALE_FACTOR"] = "1"
    environment["QT_QPA_PLATFORMTHEME"] = "gtk3"

    def command(*arguments):
        return subprocess.check_output(
            list(arguments), env=environment, text=True, stderr=subprocess.DEVNULL
        ).strip()

    def key(*keys):
        for value in keys:
            command("xdotool", "key", "--clearmodifiers", value)
            time.sleep(0.18)

    def type_text(text):
        subprocess.run(
            ["xsel", "--clipboard", "--input"],
            input=text,
            text=True,
            env=environment,
            check=True,
        )
        key("ctrl+v")

    def wait_for(predicate, timeout=6):
        end = time.monotonic() + timeout
        while time.monotonic() < end:
            try:
                value = predicate()
                if value:
                    return value
            except (subprocess.CalledProcessError, sqlite3.OperationalError):
                pass
            time.sleep(0.05)
        raise AssertionError("GUI-Zustand nicht rechtzeitig erreicht")

    with tempfile.TemporaryDirectory(prefix="sammlervitrine-gui-") as temporary:
        directory = Path(temporary)
        (directory / "language.txt").write_text("de-DE\n")
        database = directory / "sammlervitrine.sqlite3"
        log = (output / "app.log").open("w")
        process = None

        def rows():
            if not database.exists():
                return None
            with sqlite3.connect(
                f"file:{database}?mode=ro", uri=True, timeout=0.1
            ) as connection:
                return connection.execute(
                    "SELECT id,name,maker,location,note,photo FROM items ORDER BY id"
                ).fetchall()

        def window(title):
            identifier = wait_for(
                lambda: command(
                    "xdotool",
                    "search",
                    "--onlyvisible",
                    "--all",
                    "--pid",
                    str(process.pid),
                    "--name",
                    title,
                ).splitlines()[-1]
            )
            command("xdotool", "windowfocus", identifier)
            time.sleep(0.15)
            return identifier

        def capture(identifier, name):
            time.sleep(0.15)
            command("import", "-window", identifier, str(output / f"{name}.png"))

        def click(identifier, x, y):
            command(
                "xdotool",
                "mousemove",
                "--window",
                identifier,
                str(x),
                str(y),
                "click",
                "1",
            )
            time.sleep(0.15)

        def destructive(identifier, discard=False):
            geometry = dict(
                line.split("=", 1)
                for line in command(
                    "xdotool", "getwindowgeometry", "--shell", identifier
                ).splitlines()
            )
            click(
                identifier,
                int(geometry["WIDTH"]) - (198 if discard else 140),
                int(geometry["HEIGHT"]) - 24,
            )

        def start():
            nonlocal process
            process = subprocess.Popen(
                [str(binary), "--data-dir", str(directory)],
                cwd=directory,
                env=environment,
                stdout=log,
                stderr=log,
            )
            main_window = window("^SammlerVitrine$")
            command("xdotool", "windowfocus", main_window)
            wait_for(lambda: rows() is not None)
            time.sleep(0.25)
            return main_window

        def stop():
            command("xdotool", "windowfocus", main_window)
            key("ctrl+q")
            process.wait(timeout=5)
            assert process.returncode == 0

        def select_photo(path=None):
            key("alt+f")
            chooser = window("Foto auswählen")
            if path is None:
                capture(chooser, "native-chooser-cancel")
                key("Escape")
            else:
                key("ctrl+l")
                type_text(str(path))
                key("Return")
                time.sleep(0.7)
            window("^Sammlerstück (hinzufügen|bearbeiten)$")

        def edit():
            window("^SammlerVitrine$")
            key("alt+b")
            return window("^Sammlerstück bearbeiten$")

        def discard():
            key("Escape")
            confirmation = window("Änderungen verwerfen")
            destructive(confirmation, discard=True)
            window("^SammlerVitrine$")

        try:
            source = directory / "figur.png"
            replacement = directory / "ersatz.jpg"
            svg = directory / "synthetisch.svg"
            svg.write_text(
                '<svg xmlns="http://www.w3.org/2000/svg" width="640" height="480"><rect width="640" height="480" fill="#e6eeeb"/><ellipse cx="320" cy="412" rx="140" ry="22" fill="#bccbc8"/><rect x="228" y="176" width="184" height="204" rx="32" fill="#356f78"/><rect x="228" y="80" width="184" height="120" rx="36" fill="#d3a34e"/><circle cx="280" cy="135" r="18" fill="#244b53"/><circle cx="360" cy="135" r="18" fill="#244b53"/><rect x="165" y="195" width="54" height="148" rx="20" fill="#d3a34e"/><rect x="421" y="195" width="54" height="148" rx="20" fill="#d3a34e"/><rect x="240" y="361" width="62" height="60" rx="12" fill="#244b53"/><rect x="338" y="361" width="62" height="60" rx="12" fill="#244b53"/><circle cx="320" cy="260" r="32" fill="#e6eeeb"/></svg>'
            )
            command("convert", str(svg), str(source))
            command("convert", str(svg), "-modulate", "100,100,150", str(replacement))
            original_bytes = source.read_bytes()
            replacement_bytes = replacement.read_bytes()
            invalid = directory / "kaputt.png"
            invalid.write_text("synthetisch beschädigt")
            main_window = start()
            assert rows() == []
            capture(main_window, "01-empty")
            key("ctrl+n")
            editor = window("^Sammlerstück hinzufügen$")
            key("ctrl+s")
            capture(editor, "02-validation")
            key("alt+b")
            type_text("Blechroboter")
            key("alt+m")
            type_text("Musterwerkstatt")
            key("alt+a")
            type_text("Vitrine, Fach 2")
            key("alt+n")
            type_text("Geschenk von Ömer.")
            key("Return")
            type_text("Originalschachtel im oberen Regal.")
            select_photo()  # real native chooser cancellation
            select_photo(source)
            capture(editor, "03-photo-draft")
            select_photo(invalid)
            capture(editor, "04-invalid-photo-keeps-draft")
            key("ctrl+s")
            wait_for(lambda: len(rows()) == 1)
            original = rows()[0]
            assert original[1:5] == (
                "Blechroboter",
                "Musterwerkstatt",
                "Vitrine, Fach 2",
                "Geschenk von Ömer.\nOriginalschachtel im oberen Regal.",
            )
            assert original[5] == original_bytes
            time.sleep(0.5)
            capture(main_window, "05-saved")
            window("^SammlerVitrine$")
            key("ctrl+f")
            type_text("ÖMER")
            capture(main_window, "06-note-search")
            key("ctrl+a")
            type_text("keine Treffer")
            capture(main_window, "07-no-results")
            key("ctrl+a", "BackSpace")
            editor = edit()
            select_photo(replacement)
            key("Escape")
            confirmation = window("Änderungen verwerfen")
            capture(confirmation, "08-photo-only-safe-default")
            key("Return")
            window("^Sammlerstück bearbeiten$")
            discard()
            assert rows()[0] == original
            editor = edit()
            key("alt+e")
            capture(editor, "09-photo-removed-in-draft")
            discard()
            assert rows()[0] == original
            editor = edit()
            key("alt+a", "ctrl+a")
            type_text("Arbeitszimmer, Vitrine 3")
            select_photo(replacement)
            with sqlite3.connect(database) as blocker:
                blocker.execute("BEGIN EXCLUSIVE")
                key("ctrl+s")
                time.sleep(2.4)
                window("^Sammlerstück bearbeiten$")
                capture(editor, "10-locked-keeps-text-and-photo")
                command("xdotool", "windowsize", editor, "400", "360")
                capture(editor, "11-small-error-dialog")
                blocker.rollback()
            assert rows()[0] == original
            key("ctrl+s")
            wait_for(lambda: rows()[0][3] == "Arbeitszimmer, Vitrine 3")
            assert rows()[0][5] == replacement_bytes
            assert rows()[0][0] == original[0]
            window("^SammlerVitrine$")
            key("ctrl+n")
            window("^Sammlerstück hinzufügen$")
            type_text("Blechroboter")
            select_photo(source)
            key("ctrl+s")
            wait_for(lambda: len(rows()) == 2)
            assert rows()[1][5] == original_bytes
            assert rows()[0][0] != rows()[1][0]
            time.sleep(0.5)
            capture(main_window, "12-independent-duplicates")
            before_restart = rows()
            stop()
            main_window = start()
            assert rows() == before_restart
            time.sleep(0.5)
            capture(main_window, "13-reopened")
            stop()
            source.unlink()
            replacement.unlink()
            main_window = start()
            assert rows() == before_restart
            time.sleep(0.5)
            capture(main_window, "14-reopened-without-sources")
            maps = Path(f"/proc/{process.pid}/maps").read_text()
            libraries = sorted(
                {
                    line.split()[-1]
                    for line in maps.splitlines()
                    if any(
                        part in line
                        for part in (
                            "libQt6",
                            "libqsqlite",
                            "libicu",
                            "libqjpeg",
                            "libqgtk3",
                        )
                    )
                }
            )
            (output / "loaded-libraries.json").write_text(
                json.dumps(libraries, indent=2) + "\n"
            )
            if args.clean_environment:
                assert all(str(binary.parent) in path for path in libraries), libraries
            assert any("libqjpeg" in path for path in libraries)
            assert any("libqgtk3" in path for path in libraries)
            editor = edit()
            key("alt+e", "ctrl+s")
            wait_for(lambda: rows()[0][5] is None)
            assert rows()[1][5] == original_bytes
            window("^SammlerVitrine$")
            key("alt+l")
            window("Sammlerstück löschen")
            key("Escape")
            assert len(rows()) == 2
            window("^SammlerVitrine$")
            key("alt+l")
            confirmation = window("Sammlerstück löschen")
            capture(confirmation, "15-delete-confirmation")
            destructive(confirmation)
            wait_for(lambda: len(rows()) == 1)
            assert rows()[0][0] == before_restart[1][0]
            assert rows()[0][5] == original_bytes
            window("^SammlerVitrine$")
            key("alt+l")
            confirmation = window("Sammlerstück löschen")
            destructive(confirmation)
            wait_for(lambda: rows() == [])
            capture(main_window, "16-empty-again")
            stop()
            (output / "result.json").write_text(
                json.dumps(
                    {
                        "result": "passed",
                        "binary": str(binary),
                        "clean_environment": args.clean_environment,
                        "native_chooser": "Qt GTK3",
                        "source_deleted_before_second_restart": True,
                    },
                    indent=2,
                )
                + "\n"
            )
            print(output / "result.json")
        finally:
            if process is not None and process.poll() is None:
                process.terminate()
                process.wait(timeout=5)
            log.close()


if __name__ == "__main__":
    main()
