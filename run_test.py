#!/usr/bin/env python3
"""
Simple test harness that runs a server and a client.

Behavior:

Start the server first. If the server cannot be started (or exits immediately),
terminate and exit non-zero.
Then start the client.
Let the client run for --timeout seconds.
If the client times out: try to shut it down gracefully (SIGINT),
wait a short grace period, then SIGKILL if necessary.
After the client has stopped, ask the server to exit (SIGINT) and wait a little.
If the server does not exit, kill it hard.
On any failure to start a subprocess, make sure the other process is terminated.
return the exit result of client only (that is our SW to test)!

Created from Claus Klein and ChatGPT as reviewer
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
    return True


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


def start_process(
    cmd: list[str], role: str, check_delay: float = 0.1
) -> subprocess.Popen:
    """
    Start a subprocess (client or server) and ensure it doesn't fail immediately.

    Args:
        cmd: Command line list to run (e.g. ['python3', 'server.py', '8000']).
        role: A label used for logging ('server', 'client', etc.).
        check_delay: Seconds to wait before checking if the process has exited.

    Returns:
        subprocess.Popen instance of the started process.

    Raises:
        RuntimeError if the process cannot start or exits immediately.
    """
    print(f"Starting {role}:", " ".join(cmd))
    try:
        proc = subprocess.Popen(cmd)
    except Exception as e:
        raise RuntimeError(f"Failed to start {role}: {e}") from e

    # Allow short delay to detect immediate failure (e.g., missing binary)
    time.sleep(check_delay)
    if proc.poll() is not None:
        raise RuntimeError(
            f"{role.capitalize()} exited immediately with code {proc.returncode}"
        )

    return proc


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
        default=29,
        help="Number of seconds to run; defaults to %(default)s",
    )
    args = parser.parse_args(args)
    port = "8000"
    server = None
    client = None

    try:
        # Start server
        try:
            server = start_process([args.server, port], role="server")
        except Exception as e:
            print(e, file=sys.stderr)
            return 2

        # Start client
        try:
            client_cmd = [args.client, "localhost", port]
            if args.input:
                client_cmd.append(args.input)
            client = start_process(client_cmd, role="client")
        except Exception as e:
            print(e, file=sys.stderr)
            if server and server.poll() is None:
                force_kill(server)
            return 4

        # Wait for client to finish or timeout
        try:
            print(f"Waiting up to {args.timeout} seconds for client to finish...")
            client.wait(timeout=args.timeout)
            print(f"Client exited (code {client.returncode}).")
        except subprocess.TimeoutExpired:
            print("Timeout expired. Attempting graceful client shutdown (SIGINT).")
            # Try graceful shutdown via SIGINT
            graceful = send_and_wait(client, signal.SIGINT, wait=3.0)
            if not graceful:
                print("Client did not exit after SIGINT; killing it.")
                force_kill(client)
            else:
                print("Client exited gracefully after SIGINT.")

        # Now ask server to exit gracefully
        if server and server.poll() is None:
            print("Requesting server to exit (SIGINT).")
            client_graceful = send_and_wait(server, signal.SIGINT, wait=1.0)
            if not client_graceful:
                print("Server did not exit after SIGINT; killing server.")
                force_kill(server)
            else:
                print("Server exited gracefully.")
        else:
            if server:
                print(f"Server already exited (code {server.returncode}).")

        # NOTE: We ignore server exit code; We return only  client's exit code (or 0)
        # NO! if server.returncode not in (None, 0): return server.returncode
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
