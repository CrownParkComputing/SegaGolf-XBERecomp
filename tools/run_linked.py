#!/usr/bin/env python3
"""Launch one or two Sega Golf players with English or Japanese text."""
import argparse
import fcntl
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
PREFERENCES = ROOT / ".local/launcher.json"


def preferences():
    """Accept only supported choices from the local launcher preferences."""
    result = {"players": 2, "language": "en"}
    try:
        saved = json.loads(PREFERENCES.read_text())
        if isinstance(saved, dict):
            if type(saved.get("players")) is int and saved["players"] in (1, 2):
                result["players"] = saved["players"]
            if saved.get("language") in ("en", "ja"):
                result["language"] = saved["language"]
    except (OSError, ValueError):
        pass
    return result


def choose_options(players, language):
    """One visible selection, with the last setup selected by default."""
    if not shutil.which("zenity"):
        raise ValueError("The options window needs zenity; use --players 1|2 --language en|ja instead")
    rows = []
    for count in (1, 2):
        for code, name in (("en", "English (partial translation)"), ("ja", "Japanese (original)")):
            rows += [str(count == players and code == language).upper(),
                     f"{count}:{code}", str(count), name]
    selection = subprocess.run([
        "zenity", "--list", "--radiolist", "--title=Sega Golf Club - Launch options",
        "--text=Choose player windows and interface language.\nThe network server runs in the background. Allow about 90 seconds to start.\nEnglish translates selected menus and controls; some Japanese text remains.",
        "--column=Select", "--column=Setup", "--column=Players", "--column=Language",
        "--hide-column=2", "--print-column=2", "--width=640", "--height=420",
        "--ok-label=Start game", "--cancel-label=Cancel", *rows,
    ], capture_output=True, text=True)
    if selection.returncode == 1:
        return None
    if selection.returncode != 0:
        raise ValueError(f"Could not open launch options: {selection.stderr.strip()}")
    key = selection.stdout.strip()
    if key not in {"1:en", "2:en", "1:ja", "2:ja"}:
        raise ValueError("Choose a player count and language before starting")
    count, code = key.split(":")
    return int(count), code


def main(argv=None):
    argv = sys.argv[1:] if argv is None else argv
    saved = preferences()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--seconds", type=float, help="Stop all instances after this many seconds (diagnostics)")
    parser.add_argument("--players", type=int, choices=(1, 2),
                        help="Number of playable windows (saved choice, initially 2)")
    parser.add_argument("--language", choices=("en", "ja"),
                        help="English translation or original Japanese (saved choice, initially en)")
    menu = parser.add_mutually_exclusive_group()
    menu.add_argument("--menu", "--options", action="store_true", help="Show launch options (automatic with no arguments on a desktop)")
    menu.add_argument("--no-menu", action="store_true", help="Start immediately using saved choices")
    parser.add_argument("--show-server", action="store_true", help="Keep the server window visible for diagnostics")
    parser.add_argument("--satellite-role", choices=("main", "sub"), default="main")
    parser.add_argument("--capture-at", type=float, nargs="+", default=[], metavar="SECONDS", help="Save diagnostic Wayland captures under .local/linked")
    args = parser.parse_args(argv)
    args.players = args.players if args.players is not None else saved["players"]
    args.language = args.language or os.environ.get("SEGAGOLF_LANGUAGE") or saved["language"]
    if args.language not in ("en", "ja"):
        parser.error("SEGAGOLF_LANGUAGE must be en or ja")
    build = ROOT / "out/build/linux-release"
    executable = build / "SegaGolf"
    if not executable.is_file():
        parser.error("Build SegaGolf first; see README.md")
    if args.seconds is not None and args.seconds <= 0:
        parser.error("--seconds must be positive")
    if any(mark < 0 for mark in args.capture_at) or (args.capture_at and not shutil.which("grim")):
        parser.error("Captures require grim and nonnegative capture times")
    linked = ROOT / ".local/linked"
    linked.mkdir(parents=True, exist_ok=True)
    with (linked / "launcher.lock").open("a") as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            parser.error("The linked cabinets are already running")
        return launch(args, parser, argv, build, linked)


def launch(args, parser, argv, build, linked):
    executable = build / "SegaGolf"
    if args.menu or (not argv and shutil.which("zenity") and
                     (os.environ.get("DISPLAY") or os.environ.get("WAYLAND_DISPLAY"))):
        try:
            selected = choose_options(args.players, args.language)
        except ValueError as error:
            parser.error(str(error))
        if selected is None:
            return 0
        args.players, args.language = selected
    if args.players == 2 and args.satellite_role != "main":
        parser.error("--satellite-role sub requires --players 1; two players use main and sub")
    PREFERENCES.write_text(json.dumps({"players": args.players, "language": args.language}, indent=2) + "\n")
    print(f"Starting {args.players} player window(s); language: {args.language}", flush=True)
    captures = iter(sorted(args.capture_at))
    next_capture = next(captures, None)
    processes = []
    logs = []
    def stop(signum, frame):
        raise KeyboardInterrupt
    signal.signal(signal.SIGTERM, stop)
    try:
        cabinets = [("server", "127.0.0.1"), (args.satellite_role, "127.0.0.2")]
        if args.players == 2:
            cabinets.append(("sub", "127.0.0.3"))
        for index, (role, ip) in enumerate(cabinets):
            directory = linked / role
            directory.mkdir(parents=True, exist_ok=True)
            # Distinct executable paths isolate settings, logs and guest HDDs.
            shutil.copy2(executable, directory / "SegaGolf")
            for library in build.glob("*.so*"):
                link = directory / library.name
                if not link.exists():
                    link.symlink_to(library.resolve())
            settings = directory / "settings.toml"
            if not settings.exists():
                settings.write_text('display_mode = "windowed"\nwidth = 800\nheight = 600\nfps_cap = 60\nvsync = false\n')
            env = os.environ.copy()
            env.update(SEGAGOLF_ROLE=role, SEGAGOLF_LOCAL_IP=ip)
            env["SEGAGOLF_LANGUAGE"] = args.language
            env["SEGAGOLF_PLAYERS"] = str(args.players)
            env["SEGAGOLF_PLAYER"] = str(index)
            env["SEGAGOLF_BACKGROUND_SERVER"] = "1" if role == "server" and not args.show_server else "0"
            env["XBE_VULKAN_OFFSCREEN"] = env["SEGAGOLF_BACKGROUND_SERVER"]
            if env.get("SEGAGOLF_TRACE_STARTUP"):
                env["XBE_LOG_VERBOSE"] = "1"
            log = (directory / "stdout.log").open("w")
            logs.append(log)
            processes.append(subprocess.Popen([str(directory / "SegaGolf")], cwd=directory, env=env, stdout=log, stderr=subprocess.STDOUT))
            print(f"{role}: {ip}; log {directory / 'stdout.log'}", flush=True)
        start = time.monotonic()
        while all(process.poll() is None for process in processes):
            elapsed = time.monotonic() - start
            if next_capture is not None and elapsed >= next_capture:
                subprocess.run(["grim", str(linked / f"screen-{next_capture:g}s.png")], capture_output=True, timeout=5, check=True)
                next_capture = next(captures, None)
            if args.seconds and elapsed >= args.seconds:
                break
            time.sleep(0.2)
        failed = any(process.poll() not in (None, 0) for process in processes)
    except KeyboardInterrupt:
        failed = False
    finally:
        for process in processes:
            if process.poll() is None:
                process.terminate()
        for process in processes:
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
        for log in logs:
            log.close()
    return int(failed)

if __name__ == "__main__":
    raise SystemExit(main())
