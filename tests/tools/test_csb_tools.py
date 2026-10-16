# SPDX-License-Identifier: GPL-3.0-or-later
"""LLM tools on CSB: the program starts the prison over the control socket and the Python tool layer reads it."""

import os
import shutil
import signal
import subprocess
import tempfile
import time


from dmtest.markers import GAME_CSB, ROOT, needs_csb, needs_csb_mini, needs_dm, needs_sdl


pytestmark = [needs_csb, needs_csb_mini]


@needs_sdl
def test_csb_prison_state_through_the_tools(tmp_path, monkeypatch):
    data = tmp_path / "game-csb"
    shutil.copytree(GAME_CSB, data)
    sock_dir = tempfile.mkdtemp(prefix="dm", dir="/tmp")  # sun_path holds about 100 bytes
    path = os.path.join(sock_dir, "control.sock")
    env = dict(os.environ, SDL_VIDEO_DRIVER="dummy", SDL_AUDIO_DRIVER="dummy")
    program = subprocess.Popen([str(ROOT / "build/dm"), "--adventure", "csb", "--mode", "turnbased", "--data", str(data),
                                "--control", path, "--frames", "100000"],
                               cwd=ROOT, env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    monkeypatch.setenv("DM_CONTROL", path)
    from dmtools import mcp_server, play
    try:
        for _ in range(200):
            try:
                state = mcp_server.get_state()
                if state["ok"]:
                    break
            except OSError:
                pass
            time.sleep(0.05)
        assert state["ok"]
        assert state["state"]["adventure"] == "csb"
        party = state["state"]["party"]
        assert (party["map"], party["x"], party["y"]) == (0, 9, 0)
        assert " | CSB | " in play.brief(state)
        new = mcp_server.new_game()["state"]["party"]
        assert (new["map"], new["x"], new["y"]) == (0, 9, 0)
        assert mcp_server.get_view()["view"]["squares"]
        assert mcp_server.act("turn_left")["ok"]
    finally:
        program.send_signal(signal.SIGINT)
        try:
            program.wait(timeout=10)
        except subprocess.TimeoutExpired:
            program.kill()
        shutil.rmtree(sock_dir, ignore_errors=True)


@needs_sdl
@needs_dm
def test_dm_state_through_the_tools(monkeypatch):
    sock_dir = tempfile.mkdtemp(prefix="dm", dir="/tmp")
    path = os.path.join(sock_dir, "control.sock")
    env = dict(os.environ, SDL_VIDEO_DRIVER="dummy", SDL_AUDIO_DRIVER="dummy")
    program = subprocess.Popen([str(ROOT / "build/dm"), "--mode", "turnbased", "--control", path, "--frames", "100000"],
                               cwd=ROOT, env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    monkeypatch.setenv("DM_CONTROL", path)
    from dmtools import mcp_server, play
    try:
        for _ in range(200):
            try:
                state = mcp_server.get_state()
                if state["ok"]:
                    break
            except OSError:
                pass
            time.sleep(0.05)
        assert state["ok"]
        assert state["state"]["adventure"] == "dm"
        assert "CSB" not in play.brief(state)
    finally:
        program.send_signal(signal.SIGINT)
        try:
            program.wait(timeout=10)
        except subprocess.TimeoutExpired:
            program.kill()
        shutil.rmtree(sock_dir, ignore_errors=True)
