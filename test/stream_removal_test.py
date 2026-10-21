#!/usr/bin/env python3
"""Exercise stream removal through a local server's real JSON-RPC endpoint.

Usage: python3 test/stream_removal_test.py /path/to/snapserver
Requires FLAC support. To reliably detect the invalid-iterator regression,
build with -DASAN=ON; for libstdc++, also use -D_GLIBCXX_SANITIZE_VECTOR.
On Linux, ASAN_OPTIONS=detect_leaks=0 excludes unrelated shutdown leaks.
"""

import http.client
import json
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import time


def free_port():
    with socket.socket() as reservation:
        reservation.bind(("127.0.0.1", 0))
        return reservation.getsockname()[1]


def run(binary):
    with tempfile.TemporaryDirectory(prefix="snapcast-removal-") as directory:
        root = Path(directory)
        port = free_port()
        config = root / "server.conf"
        config.write_text(f"""[server]
threads = 4
datadir = {root}
mdns_enabled = false
[http]
bind_to_address = 127.0.0.1
port = {port}
[tcp-control]
enabled = false
[tcp-streaming]
enabled = false
[stream]
source = tcp://127.0.0.1:{free_port()}?name=base&codec=pcm
""")

        def rpc(method, params=None):
            connection = http.client.HTTPConnection("127.0.0.1", port, timeout=3)
            try:
                connection.request("POST", "/jsonrpc", json.dumps({
                    "id": 1, "jsonrpc": "2.0", "method": method,
                    "params": params or {},
                }), {"Content-Type": "application/json"})
                response = json.loads(connection.getresponse().read())
                if "error" in response:
                    raise AssertionError(response)
                return response["result"]
            finally:
                connection.close()

        def add(name):
            audio_port = free_port()
            rpc("Stream.AddStream", {"streamUri":
                f"tcp://127.0.0.1:{audio_port}?name={name}&codec=flac&sampleformat=48000:16:2"})
            return audio_port

        def remove(name):
            rpc("Stream.RemoveStream", {"id": name})
            status = rpc("Server.GetStatus")["server"]
            assert name not in [stream["id"] for stream in status["streams"]], status

        def wait_for_state(name, expected):
            deadline = time.monotonic() + 3
            while time.monotonic() < deadline:
                streams = rpc("Server.GetStatus")["server"]["streams"]
                if any(stream["id"] == name and stream["status"] == expected
                       for stream in streams):
                    return
                time.sleep(0.01)
            raise AssertionError(f"Stream {name} did not become {expected}: {streams}")

        with (root / "server.log").open("w+") as log:
            process = subprocess.Popen([str(binary), "--config", str(config)],
                                       stdout=log, stderr=subprocess.STDOUT)
            try:
                deadline = time.monotonic() + 10
                while True:
                    try:
                        rpc("Server.GetStatus")
                        break
                    except OSError:
                        if process.poll() is not None or time.monotonic() > deadline:
                            raise
                        time.sleep(0.05)
                # The announcement is the vector's last element, as in Music Assistant.
                for _ in range(20):
                    audio_port = add("announcement")
                    with socket.create_connection(("127.0.0.1", audio_port), timeout=3) as audio:
                        audio.sendall(b"\x01\x00" * 5760)
                        wait_for_state("announcement", "playing")
                    wait_for_state("announcement", "idle")
                    remove("announcement")
                # Also cover erasure in the middle and removal of the only source.
                add("middle")
                add("tail")
                remove("middle")
                remove("tail")
                remove("base")
                assert rpc("Server.GetStatus")["server"]["streams"] == []
                assert process.poll() is None
            except Exception:
                if process.poll() is None:
                    process.terminate()
                process.wait(timeout=10)
                log.seek(0)
                print(log.read(), file=sys.stderr)
                raise
            finally:
                if process.poll() is None:
                    process.terminate()
                    process.wait(timeout=10)
            log.seek(0)
            output = log.read()
            assert process.returncode == 0, output
            assert "ERROR: AddressSanitizer" not in output, output
            assert "runtime error:" not in output, output
            print("PASS: 20 announcement cycles; middle, last, and only stream removal")


if __name__ == "__main__":
    run(Path(sys.argv[1]).resolve())
