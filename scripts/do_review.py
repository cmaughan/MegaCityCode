#!/usr/bin/env python3
from __future__ import annotations

import subprocess
import sys
from pathlib import Path


def main() -> int:
    repo_root = Path(__file__).resolve().parent.parent
    prompt_file = repo_root / "plans" / "prompts" / "review.md"
    output_file = repo_root / "plans" / "reviews" / "review-latest.md"
    all_code_file = repo_root / "plans" / "reviews" / "all_code.md"
    ask_agent = repo_root / "scripts" / "ask_agent.py"
    export_all_code = repo_root / "scripts" / "export_all_code.py"

    export_completed = subprocess.run([sys.executable, str(export_all_code)], cwd=repo_root, check=False)
    if export_completed.returncode != 0:
        return export_completed.returncode

    forwarded_args = sys.argv[1:]
    if "--prompt-file" in forwarded_args:
        raise SystemExit("do_review.py fixes --prompt-file to plans/prompts/review.md; do not pass it explicitly.")

    if "--output-file" not in forwarded_args:
        forwarded_args = ["--output-file", str(output_file), *forwarded_args]

    if "--interactive" not in forwarded_args and "--parallel" not in forwarded_args:
        forwarded_args = ["--parallel", *forwarded_args]

    if "--gpt-review-safe" not in forwarded_args:
        forwarded_args = ["--gpt-review-safe", *forwarded_args]

    if "--gpt-prepend-file" not in forwarded_args:
        forwarded_args = ["--gpt-prepend-file", str(all_code_file), *forwarded_args]
    if "--claude-prepend-file" not in forwarded_args:
        forwarded_args = ["--claude-prepend-file", str(all_code_file), *forwarded_args]
    if "--gemini-prepend-file" not in forwarded_args:
        forwarded_args = ["--gemini-prepend-file", str(all_code_file), *forwarded_args]

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
