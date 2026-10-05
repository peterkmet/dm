import os
import tempfile

# dm-play logs each call; the tests must not write into the log of a run
os.environ["DM_PLAY_LOG_DIR"] = tempfile.mkdtemp(prefix="dm-play-log")
