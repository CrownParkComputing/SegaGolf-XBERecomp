#!/usr/bin/env python3
"""Run two playable golf cabinets against the original local server."""
import argparse
import fcntl
import os
from pathlib import Path
import shutil
import signal
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--seconds", type=float, help="Stop all instances after this many seconds (diagnostics)")
    parser.add_argument("--players", type=int, choices=(1, 2), default=2,
                        help="Number of playable cabinets (default: 2)")
    parser.add_argument("--show-server", action="store_true", help="Keep the server window visible for diagnostics")
    parser.add_argument("--satellite-role", choices=("main", "sub"), default="main")
    parser.add_argument("--capture-at", type=float, nargs="+", default=[], metavar="SECONDS", help="Save diagnostic Wayland captures under .local/linked")
    args = parser.parse_args()
    build = ROOT / "out/build/linux-release"
    executable = build / "SegaGolf"
    if not executable.is_file():
        parser.error("Build SegaGolf first; see README.md")
    if args.seconds is not None and args.seconds <= 0:
        parser.error("--seconds must be positive")
    if args.players == 2 and args.satellite_role != "main":
        parser.error("--satellite-role sub requires --players 1; two players use main and sub")
    if any(mark < 0 for mark in args.capture_at) or (args.capture_at and not shutil.which("grim")):
        parser.error("Captures require grim and nonnegative capture times")
    captures = iter(sorted(args.capture_at))
    next_capture = next(captures, None)
    linked = ROOT / ".local/linked"
    linked.mkdir(parents=True, exist_ok=True)
    lock = (linked / "launcher.lock").open("a")
    try:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except BlockingIOError:
        parser.error("The linked cabinets are already running")
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
