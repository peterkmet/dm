# SPDX-License-Identifier: GPL-3.0-or-later
"""The control socket of the turn-based mode: the pumped main loop and a socket client."""

import json
import socket
import tempfile
import threading
from contextlib import contextmanager


def socket_dir():
    """A new folder for a control socket in /tmp: sun_path holds about 100 bytes."""
    return tempfile.mkdtemp(prefix="dm", dir="/tmp")


def pump(lib):
    """One frame of the turn-based main loop."""
    lib.control_poll()
    if lib.control_is_turn_pending() and lib.gameloop_run_frame():
        lib.gameloop_tick()


@contextmanager
def pumped(game):
    """The turn-based main loop in a thread, for clients that block while they wait."""
    stop = threading.Event()

    def loop():
        while not stop.is_set():
            pump(game.lib)

    thread = threading.Thread(target=loop)
    thread.start()
    try:
        yield
    finally:
        stop.set()
        thread.join()


class Client:
    """A socket client that pumps the game in the same thread while it waits."""

    def __init__(self, game):
        self.game = game
        self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.sock.connect(game.socket)
        self.sock.setblocking(False)

    def call(self, **payload):
        self.sock.sendall(json.dumps(payload).encode() + b"\n")
        data = b""
        for _ in range(20000):
            pump(self.game.lib)
            try:
                data += self.sock.recv(1 << 20)
            except BlockingIOError:
                pass
            if data.endswith(b"\n"):
                return json.loads(data)
        raise AssertionError("no response")

    def act(self, action, **args):
        response = self.call(cmd="act", action=action, args=args)
        assert response["ok"], response
        return response
