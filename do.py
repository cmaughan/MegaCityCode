from __future__ import annotations

import pathlib
import subprocess
import sys


def repo_root() -> pathlib.Path:
    return pathlib.Path(__file__).resolve().parent


def build_dir(root: pathlib.Path) -> pathlib.Path:
    return root / "build"


def spectre_path(root: pathlib.Path) -> pathlib.Path:
    if sys.platform.startswith("win"):
        return build_dir(root) / "Debug" / "spectre.exe"
    return build_dir(root) / "spectre"


def scenario_path(root: pathlib.Path, name: str) -> pathlib.Path:
    return root / "tests" / "render" / f"{name}.toml"


def run(command: list[str], cwd: pathlib.Path) -> int:
    print("> " + " ".join(command))
    completed = subprocess.run(command, cwd=cwd, check=False)
    return completed.returncode


def ensure_built(root: pathlib.Path) -> int:
    exe = spectre_path(root)
    if exe.exists():
        return 0

    if sys.platform.startswith("win"):
        return run(["cmake", "--build", str(build_dir(root)), "--config", "Debug", "--parallel"], root)
    return run(["cmake", "--build", str(build_dir(root)), "--parallel"], root)


def help_text() -> str:
    return """Usage:
  python do.py <command> [--skip-build]

Single-word shortcuts:
  run          Run Spectre normally with a console
  smoke        Run the app smoke test
  test         Run the full local test suite (t.bat / run_tests.sh)
  shot         Regenerate the README hero screenshot

Deterministic render snapshots:
  basic        Run basic-view compare
  cmdline      Run cmdline-view compare
  unicode      Run unicode-view compare
  renderall    Run all three compare snapshots

Bless render references:
  blessbasic   Bless basic-view
  blesscmdline Bless cmdline-view
  blessunicode Bless unicode-view
  blessall     Bless all three deterministic references

Examples:
  python do.py smoke
  python do.py basic
  python do.py blessall
  python do.py shot
  python do.py test
"""


def main() -> int:
    root = repo_root()
    args = sys.argv[1:]

    if not args or args[0] in {"-h", "--help", "help"}:
        print(help_text())
        return 0

    command = args[0].lower()
    skip_build = "--skip-build" in args[1:]

    if command == "test":
        if sys.platform.startswith("win"):
            return run(["cmd", "/c", "t.bat"], root)
        return run(["sh", "./scripts/run_tests.sh"], root)

    if command == "shot":
        cmd = [sys.executable, str(root / "scripts" / "update_screenshot.py")]
        if skip_build:
            cmd.append("--skip-build")
        return run(cmd, root)

    if command == "run":
        if ensure_built(root) != 0:
            return 1
        exe = spectre_path(root)
        return run([str(exe), "--console"], root)

    if command == "smoke":
        if ensure_built(root) != 0:
            return 1
        exe = spectre_path(root)
        return run([str(exe), "--console", "--smoke-test"], root)

    render_map = {
        "basic": ("basic-view", False),
        "cmdline": ("cmdline-view", False),
        "unicode": ("unicode-view", False),
        "blessbasic": ("basic-view", True),
        "blesscmdline": ("cmdline-view", True),
        "blessunicode": ("unicode-view", True),
    }

    if command in render_map:
        if ensure_built(root) != 0:
            return 1
        scenario_name, bless = render_map[command]
        exe = spectre_path(root)
        cmd = [str(exe), "--console", "--render-test", str(scenario_path(root, scenario_name))]
        if bless:
            cmd.append("--bless-render-test")
        return run(cmd, root)

    if command == "renderall":
        for scenario_name in ("basic-view", "cmdline-view", "unicode-view"):
            rc = run([str(spectre_path(root)), "--console", "--render-test", str(scenario_path(root, scenario_name))], root)
            if rc != 0:
                return rc
        return 0

    if command == "blessall":
        if ensure_built(root) != 0:
            return 1
        for scenario_name in ("basic-view", "cmdline-view", "unicode-view"):
            rc = run(
                [str(spectre_path(root)), "--console", "--render-test", str(scenario_path(root, scenario_name)), "--bless-render-test"],
                root,
            )
            if rc != 0:
                return rc
        return 0

    print(f"Unknown command: {command}\n")
    print(help_text())
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
