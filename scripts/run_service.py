"""Lanzado por el addon: cierre limpio mediante archivo de parada y PID del padre."""
import argparse
from pathlib import Path
import sys
import threading
import time

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))


def main():
    import psutil
    import uvicorn
    from realtime.server import create_app
    parser = argparse.ArgumentParser()
    parser.add_argument("--stop-file", type=Path, required=True)
    parser.add_argument("--parent-pid", type=int, required=True)
    args = parser.parse_args()
    server = uvicorn.Server(uvicorn.Config(create_app(), host="127.0.0.1", port=8770, access_log=False))
    done = threading.Event()

    def watch():
        while not done.wait(0.2):
            if args.stop_file.exists() or not psutil.pid_exists(args.parent_pid):
                server.should_exit = True
                return

    watcher = threading.Thread(target=watch, daemon=True)
    watcher.start()
    try:
        server.run()
    finally:
        done.set()
        watcher.join(timeout=1)
        args.stop_file.unlink(missing_ok=True)


if __name__ == "__main__":
    main()
