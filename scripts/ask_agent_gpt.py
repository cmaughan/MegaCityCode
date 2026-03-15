#!/usr/bin/env python3
from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from pathlib import Path


def resolve_path(path_str: str, must_exist: bool = False) -> Path:
    path = Path(path_str).expanduser()
    if not path.is_absolute():
        path = Path.cwd() / path
    path = path.resolve()
    if must_exist and not path.exists():
        raise FileNotFoundError(f"Path does not exist: {path}")
    return path


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Run a saved prompt file through codex exec and write the last agent message to a file."
    )
    parser.add_argument("--prompt-file", required=True, help="Path to the prompt markdown/text file.")
    parser.add_argument("--output-file", required=True, help="Path to write the final agent response.")
    parser.add_argument("--model", default="gpt-5.4", help="Codex model name to use. Default: gpt-5.4")
    parser.add_argument("--working-dir", help="Working directory for codex exec. Defaults to repo root.")
    parser.add_argument("--profile", help="Optional Codex profile name.")
    parser.add_argument(
        "--add-dir",
        action="append",
        default=[],
        help="Extra writable directory to pass through to codex exec. Repeatable.",
    )
    parser.add_argument(
        "--image",
        action="append",
        default=[],
        help="Image to attach to the initial prompt. Repeatable.",
    )
    parser.add_argument(
        "--ephemeral",
        action="store_true",
        help="Run codex exec without persisting session files.",
    )
    parser.add_argument(
        "--no-full-auto",
        action="store_true",
        help="Do not pass --full-auto to codex exec.",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Print the resolved command and exit without running codex.",
    )
    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()

    codex = shutil.which("codex")
    if not codex:
        raise RuntimeError("codex CLI is not on PATH.")

    repo_root = Path(__file__).resolve().parent.parent
    prompt_file = resolve_path(args.prompt_file, must_exist=True)
    output_file = resolve_path(args.output_file)
    working_dir = resolve_path(args.working_dir, must_exist=True) if args.working_dir else repo_root

    output_file.parent.mkdir(parents=True, exist_ok=True)
    prompt_text = prompt_file.read_text(encoding="utf-8")

    command = [
        codex,
        "exec",
        "--cd",
        str(working_dir),
        "--model",
        args.model,
        "--output-last-message",
        str(output_file),
    ]

    if not args.no_full_auto:
        command.append("--full-auto")

    if args.ephemeral:
        command.append("--ephemeral")

    if args.profile:
        command.extend(["--profile", args.profile])

    for add_dir in args.add_dir:
        command.extend(["--add-dir", str(resolve_path(add_dir, must_exist=True))])

    for image in args.image:
        command.extend(["--image", str(resolve_path(image, must_exist=True))])

    command.append("-")

    if args.dry_run:
        print(f"Prompt file : {prompt_file}")
        print(f"Output file : {output_file}")
        print(f"Working dir : {working_dir}")
        print(f"Model       : {args.model}")
        print("Command     : " + " ".join(command))
        return 0

    print(f"Running GPT helper with model {args.model}", flush=True)
    completed = subprocess.run(
        command,
        input=prompt_text,
        text=True,
        cwd=working_dir,
        check=False,
    )

    if completed.returncode != 0:
        raise RuntimeError(f"codex exec failed with exit code {completed.returncode}.")

    print(f"Wrote agent output to {output_file}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(exc, file=sys.stderr)
        raise SystemExit(1)
