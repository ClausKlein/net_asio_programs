#!/usr/bin/env python3
"""
Simple test harness that runs a server and a client.

Behavior:

Start the server first. If the server cannot be started (or exits immediately),
terminate and exit non-zero.
Then start the client.
Let the server run for --timeout seconds.
If the server times out: try to shut it down gracefully (SIGINT),
wait a short grace period, then SIGKILL if necessary.
After the server has stopped, ask the client to exit (SIGINT) and wait a little.
If the client does not exit, kill it hard.
On any failure to start a subprocess, make sure the other process is terminated.
"""

import argparse
import signal
import subprocess
import sys
import time

from pathlib import Path
from typing import List


def send_and_wait(proc: subprocess.Popen, sig: int, wait: float) -> bool:
    """Send signal to proc and wait up to timeout seconds. Return True if exited."""
    if proc is None:
        return True
    if proc.poll() is not None:
        return True
    try:
        proc.send_signal(sig)
    except Exception:
        # fallback to terminate if send_signal fails
        try:
            proc.terminate()
        except Exception:
            pass
        try:
            proc.wait(timeout=wait)
            return True
        except subprocess.TimeoutExpired:
            return False


def force_kill(proc: subprocess.Popen) -> None:
    """Kill proc and wait (best-effort)."""
    if proc is None:
        return
    try:
        proc.kill()
    except Exception:
        pass
    try:
        proc.wait(timeout=1.0)
    except Exception:
        # give up
        pass


HERE = Path(__file__).resolve().parent
PROJECT_DIR = HERE.parent.parent.parent


def main(args: List[str]):
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--client",
        help="The client to test",
    )
    parser.add_argument(
        "--server",
        help="The server to run",
    )
    parser.add_argument(
        "--input",
        help="The input file to read",
    )
    parser.add_argument(
        "--timeout",
        "-t",
        type=int,
        default=13,
        help="Number of seconds to run; defaults to %(default)s",
    )
    args = parser.parse_args(args)
    port = "8000"

    # client = subprocess.Popen([args.client, "localhost", port, args.input])
    # try:
    #     subprocess.run(
    #         [args.server, port],
    #         timeout=args.timeout,
    #         check=True,
    #     )
    # except subprocess.TimeoutExpired:
    #     print("Test expectedly timed out; sending SIGINT")
    #     client.send_signal(signal.SIGINT)
    #     time.sleep(1)
    #     try:
    #         client.wait(timeout=1)
    #     except subprocess.TimeoutExpired:
    #         print("client does not respond to SIGINT in time, sending SIGKILL")
    #         client.kill()
    #         client.wait()
    # except Exception as ex:
    #     print("ERROR:", ex)
    #     client.kill()
    #     client.wait()
    #     raise

    server = None
    client = None

    try:
        # Start server
        server_cmd = [args.server, port]
        print("Starting server:", " ".join(server_cmd))
        try:
            server = subprocess.Popen(server_cmd)
        except Exception as e:
            print("Failed to start server:", e, file=sys.stderr)
            return 2

        # Give the server a short moment to fail fast (binary not found or immediate error)
        time.sleep(0.1)
        if server.poll() is not None:
            print(
                f"Server exited immediately with code {server.returncode}",
                file=sys.stderr,
            )
            return 3

        # Start client
        client_cmd = [args.client, "localhost", port]
        if args.input:
            client_cmd.append(args.input)
        print("Starting client:", " ".join(client_cmd))
        try:
            client = subprocess.Popen(client_cmd)
        except Exception as e:
            print("Failed to start client:", e, file=sys.stderr)
            # Make sure server is torn down
            if server.poll() is None:
                force_kill(server)
            return 4

        # Wait for server to finish or timeout
        try:
            print(f"Waiting up to {args.timeout} seconds for server to finish...")
            server.wait(timeout=args.timeout)
            print(f"Server exited (code {server.returncode}).")
        except subprocess.TimeoutExpired:
            print("Timeout expired. Attempting graceful server shutdown (SIGINT).")
            # Try graceful shutdown via SIGINT
            graceful = send_and_wait(server, signal.SIGINT, wait=3.0)
            if not graceful:
                print("Server did not exit after SIGINT; killing it.")
                force_kill(server)
            else:
                print("Server exited gracefully after SIGINT.")

        # Ensure server is not running
        if server.poll() is None:
            # As a last resort
            print("Server still alive after attempts; killing.")
            force_kill(server)

        # Now ask client to exit gracefully
        if client and client.poll() is None:
            print("Requesting client to exit (SIGINT).")
            client_graceful = send_and_wait(client, signal.SIGINT, wait=1.0)
            if not client_graceful:
                print("Client did not exit after SIGINT; killing client.")
                force_kill(client)
            else:
                print("Client exited gracefully.")
        else:
            if client:
                print(f"Client already exited (code {client.returncode}).")

        # Return server exit code if non-zero, else client's exit code (or 0)
        # XXX if server.returncode not in (None, 0): return server.returncode
        if client:
            return client.returncode if client.returncode is not None else 0
        return 0

    finally:
        # Cleanup any lingering processes
        if client and client.poll() is None:
            print("Final cleanup: killing client.")
            force_kill(client)
        if server and server.poll() is None:
            print("Final cleanup: killing server.")
            force_kill(server)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
