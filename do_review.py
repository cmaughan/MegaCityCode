#!/usr/bin/env python3
from __future__ import annotations

import subprocess
import sys
from pathlib import Path


def main() -> int:
    repo_root = Path(__file__).resolve().parent
    prompt_file = repo_root / "plans" / "prompts" / "review.md"
    output_file = repo_root / "plans" / "reviews" / "review-latest.md"
    ask_agent = repo_root / "scripts" / "ask_agent.py"

    forwarded_args = sys.argv[1:]
    if "--prompt-file" in forwarded_args:
        raise SystemExit("do_review.py fixes --prompt-file to plans/prompts/review.md; do not pass it explicitly.")

    if "--output-file" not in forwarded_args:
        forwarded_args = ["--output-file", str(output_file), *forwarded_args]

    command = [
        sys.executable,
        str(ask_agent),
        "--prompt-file",
        str(prompt_file),
        *forwarded_args,
    ]

    completed = subprocess.run(command, cwd=repo_root, check=False)
    return completed.returncode


if __name__ == "__main__":
    raise SystemExit(main())
