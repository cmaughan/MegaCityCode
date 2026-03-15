#!/usr/bin/env python3
from __future__ import annotations

import argparse
import os
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path


def resolve_path(path_str: str, must_exist: bool = False) -> Path:
    path = Path(path_str).expanduser()
    if not path.is_absolute():
        path = Path.cwd() / path
    path = path.resolve()
    if must_exist and not path.exists():
        raise FileNotFoundError(f"Path does not exist: {path}")
    return path


def derive_output_paths(output_file: Path) -> tuple[Path, Path, Path]:
    suffix = output_file.suffix or ".md"
    stem = output_file.stem if output_file.suffix else output_file.name
    parent = output_file.parent
    gpt_output = parent / f"{stem}.gpt{suffix}"
    claude_output = parent / f"{stem}.claude{suffix}"
    gemini_output = parent / f"{stem}.gemini{suffix}"
    return gpt_output, claude_output, gemini_output


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Run ask_agent_gpt.py and ask_agent_claude.py in parallel from one prompt file. Gemini is optional."
    )
    parser.add_argument("--prompt-file", required=True, help="Path to the prompt markdown/text file.")
    parser.add_argument(
        "--output-file",
        required=True,
        help="Base output file. The wrapper writes sibling files with .gpt and .claude suffixes, plus .gemini when enabled.",
    )
    parser.add_argument("--working-dir", help="Working directory for all agents. Defaults to repo root.")
    parser.add_argument("--gpt-model", default="gpt-5.4", help="GPT model name. Default: gpt-5.4")
    parser.add_argument(
        "--claude-model",
        default="claude-sonnet-4-6",
        help="Claude model ID. Default: claude-sonnet-4-6",
    )
    parser.add_argument(
        "--gemini-model",
        default="flash",
        help="Gemini model name. Default: flash",
    )
    parser.add_argument(
        "--with-gemini",
        action="store_true",
        help="Also run ask_agent_gemini.py in parallel. Disabled by default.",
    )
    parser.add_argument("--profile", help="Optional Codex profile name for the GPT helper.")
    parser.add_argument(
        "--add-dir",
        action="append",
        default=[],
        help="Extra writable directory for the GPT helper. Repeatable.",
    )
    parser.add_argument(
        "--image",
        action="append",
        default=[],
        help="Image to attach to the GPT helper prompt. Repeatable.",
    )
    parser.add_argument(
        "--ephemeral",
        action="store_true",
        help="Run the GPT helper without persisting session files.",
    )
    parser.add_argument(
        "--no-full-auto",
        action="store_true",
        help="Disable full-auto mode for all enabled helpers.",
    )
    parser.add_argument(
        "--allowed-tools",
        default="Bash,Read,Write,Edit,Glob,Grep",
        help="Comma-separated tool allowlist for the Claude helper.",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Print the derived commands and exit without running any helpers.",
    )
    return parser


def build_gpt_command(
    python_exe: str,
    script_path: Path,
    prompt_file: Path,
    output_file: Path,
    working_dir: Path,
    args: argparse.Namespace,
) -> list[str]:
    command = [
        python_exe,
        str(script_path),
        "--prompt-file",
        str(prompt_file),
        "--output-file",
        str(output_file),
        "--model",
        args.gpt_model,
        "--working-dir",
        str(working_dir),
    ]

    if args.profile:
        command.extend(["--profile", args.profile])

    for add_dir in args.add_dir:
        command.extend(["--add-dir", str(resolve_path(add_dir, must_exist=True))])

    for image in args.image:
        command.extend(["--image", str(resolve_path(image, must_exist=True))])

    if args.ephemeral:
        command.append("--ephemeral")

    if args.no_full_auto:
        command.append("--no-full-auto")

    return command


def build_claude_command(
    python_exe: str,
    script_path: Path,
    prompt_file: Path,
    output_file: Path,
    working_dir: Path,
    args: argparse.Namespace,
) -> list[str]:
    command = [
        python_exe,
        str(script_path),
        "--prompt-file",
        str(prompt_file),
        "--output-file",
        str(output_file),
        "--model",
        args.claude_model,
        "--working-dir",
        str(working_dir),
        "--allowed-tools",
        args.allowed_tools,
    ]

    if not args.no_full_auto:
        command.append("--full-auto")

    return command


def build_gemini_command(
    python_exe: str,
    script_path: Path,
    prompt_file: Path,
    output_file: Path,
    working_dir: Path,
    args: argparse.Namespace,
) -> list[str]:
    command = [
        python_exe,
        str(script_path),
        "--prompt-file",
        str(prompt_file),
        "--output-file",
        str(output_file),
        "--model",
        args.gemini_model,
        "--working-dir",
        str(working_dir),
    ]

    if not args.no_full_auto:
        command.append("--full-auto")

    return command


def run_command(label: str, command: list[str], cwd: Path) -> tuple[str, int]:
    print(f"[{label}] starting: {' '.join(command)}", flush=True)
    env = os.environ.copy()
    env["PYTHONUNBUFFERED"] = "1"
    completed = subprocess.run(
        command,
        text=True,
        cwd=cwd,
        check=False,
        env=env,
    )
    print(f"[{label}] finished with exit code {completed.returncode}", flush=True)
    return label, completed.returncode


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parent.parent
    scripts_dir = Path(__file__).resolve().parent
    prompt_file = resolve_path(args.prompt_file, must_exist=True)
    base_output = resolve_path(args.output_file)
    working_dir = resolve_path(args.working_dir, must_exist=True) if args.working_dir else repo_root

    gpt_output, claude_output, gemini_output = derive_output_paths(base_output)
    gpt_output.parent.mkdir(parents=True, exist_ok=True)
    claude_output.parent.mkdir(parents=True, exist_ok=True)
    if args.with_gemini:
        gemini_output.parent.mkdir(parents=True, exist_ok=True)

    python_exe = sys.executable
    gpt_script = scripts_dir / "ask_agent_gpt.py"
    claude_script = scripts_dir / "ask_agent_claude.py"
    gpt_command = build_gpt_command(python_exe, gpt_script, prompt_file, gpt_output, working_dir, args)
    claude_command = build_claude_command(python_exe, claude_script, prompt_file, claude_output, working_dir, args)
    gemini_command = None
    if args.with_gemini:
        gemini_script = scripts_dir / "ask_agent_gemini.py"
        gemini_command = build_gemini_command(python_exe, gemini_script, prompt_file, gemini_output, working_dir, args)

    if args.dry_run:
        print(f"Prompt file   : {prompt_file}")
        print(f"Working dir   : {working_dir}")
        print(f"GPT output    : {gpt_output}")
        print(f"Claude output : {claude_output}")
        print("GPT command   : " + " ".join(gpt_command))
        print("Claude command: " + " ".join(claude_command))
        if gemini_command is not None:
            print(f"Gemini output : {gemini_output}")
            print("Gemini command: " + " ".join(gemini_command))
        return 0

    with ThreadPoolExecutor(max_workers=3 if gemini_command is not None else 2) as executor:
        futures = [
            executor.submit(run_command, "gpt", gpt_command, working_dir),
            executor.submit(run_command, "claude", claude_command, working_dir),
        ]
        if gemini_command is not None:
            futures.append(executor.submit(run_command, "gemini", gemini_command, working_dir))

        results = [future.result() for future in futures]

    failed = False
    for label, returncode in results:
        if returncode != 0:
            failed = True
            print(f"{label} helper failed with exit code {returncode}.", file=sys.stderr)

    if failed:
        return 1

    print(f"GPT output    : {gpt_output}")
    print(f"Claude output : {claude_output}")
    if gemini_command is not None:
        print(f"Gemini output : {gemini_output}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(exc, file=sys.stderr)
        raise SystemExit(1)
