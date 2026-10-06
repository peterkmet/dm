import os
import tempfile
from pathlib import Path

# dm-play logs each call; the tests must not write into the log of a run
os.environ["DM_PLAY_LOG_DIR"] = tempfile.mkdtemp(prefix="dm-play-log")

PORT_SRC = Path(__file__).resolve().parent.parent / "port" / "src"
PORT_INCLUDES = [f"-I{PORT_SRC / layer}" for layer in ("data", "engine", "ui", "platform")]


def game_sources():
    """The .c files of libdm: all layers without the SDL entry point and window."""
    return [str(p) for p in sorted(PORT_SRC.rglob("*.c")) if p.name not in ("main.c", "platform.c")]
