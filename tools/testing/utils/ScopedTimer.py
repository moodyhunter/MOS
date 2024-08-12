# SPDX-License-Identifier: GPL-3.0-or-later

class ScopedTimer:
    def __init__(self, interval, function, args=None, kwargs=None):
        import threading
        self.timer = threading.Timer(interval, function, args, kwargs)

    def __enter__(self):
        self.timer.start()
        return self

    def __exit__(self, *args):
        self.timer.cancel()
