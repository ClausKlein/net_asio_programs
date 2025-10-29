#!/usr/bin/env python3

import argparse
import os
import signal
import subprocess
import sys
import tempfile
import time
from pathlib import Path
from typing import List

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
        "--timeout",
        "-t",
        type=int,
        default=3,
        help="Number of seconds to run; defaults to %(default)s",
    )
    args = parser.parse_args(args)

    client = subprocess.Popen([args.client, "localhost", "8000"])

    try:
        subprocess.run(
            [args.server, "8000"],
            timeout=args.timeout,
            check=True,
        )
    except subprocess.TimeoutExpired:
        print("Test expectedly timed out; sending SIGINT")
        client.send_signal(signal.SIGINT)
        time.sleep(1)
        try:
            client.wait(timeout=1)
        except subprocess.TimeoutExpired:
            print("client does not respond to SIGINT in time, sending SIGKILL")
            client.kill()
            client.wait()
    except Exception as ex:
        print("ERROR:", ex)
        client.kill()
        client.wait()
        raise


if __name__ == "__main__":
    main(sys.argv[1:])
