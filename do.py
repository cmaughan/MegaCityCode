from __future__ import annotations

import json
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


def platform_suffix() -> str:
    if sys.platform.startswith("win"):
        return "windows"
    if sys.platform.startswith("darwin"):
        return "macos"
    return "linux"


def print_render_report(root: pathlib.Path, scenario_name: str) -> None:
    report_path = root / "tests" / "render" / "out" / f"{scenario_name}.{platform_suffix()}.report.json"
    if not report_path.exists():
        print(f"  [no report found: {report_path}]")
        return
    try:
        data = json.loads(report_path.read_text())
    except Exception as e:
        print(f"  [failed to read report: {e}]")
        return

    if "error" in data:
        print(f"  [{scenario_name}] ERROR: {data['error']}")
        return

    if "changed_pixels_pct" in data:
        passed = data.get("passed", False)
        label = "PASS" if passed else "FAIL"
        print(
            f"  [{scenario_name}] diff: {data['changed_pixels_pct']:.4f}% changed pixels"
            f" ({data['changed_pixels']}/{data['width'] * data['height']})"
            f", max_channel_delta={data['max_channel_diff']}"
            f", mean_abs={data['mean_abs_channel_diff']:.4f}"
            f" [{label}]"
        )
    elif data.get("blessed"):
        print(f"  [{scenario_name}] blessed ({data['width']}x{data['height']})")


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
  api          Build local Doxygen API docs
  docs         Build all docs artifacts

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
  python do.py api
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

    if command == "api":
        return run([sys.executable, str(root / "scripts" / "build_docs.py"), "--api-only"], root)

    if command == "docs":
        return run([sys.executable, str(root / "scripts" / "build_docs.py")], root)

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
        rc = run(cmd, root)
        print_render_report(root, scenario_name)
        return rc

    if command == "renderall":
        if ensure_built(root) != 0:
            return 1
        overall_rc = 0
        for scenario_name in ("basic-view", "cmdline-view", "unicode-view"):
            rc = run([str(spectre_path(root)), "--console", "--render-test", str(scenario_path(root, scenario_name))], root)
            print_render_report(root, scenario_name)
            if rc != 0:
                overall_rc = rc
        return overall_rc

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
