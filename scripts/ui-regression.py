#!/usr/bin/env python3
"""Compare v0.4's native UI with the current scripted UI using private Xvfb.

Prerequisites: Python + Pillow, Xvfb/xvfb-run, Ninja, and two completed Release
builds with CMAKE_EXPORT_COMPILE_COMMANDS=ON. Build the current JS bundles first.
The baseline must be a detached v0.4 worktree with its original assets.

Example:
    python scripts/ui-regression.py --baseline /tmp/waveforge-v0.4 \
        --baseline-build /tmp/waveforge-v0.4/build-clean

Only generated entry points are instrumented and relinked. Neither original
executable nor source tree is changed. All captures use isolated save files.
Artifacts are saved under .cache/ui-comparison unless --output is supplied.
"""

import argparse
import json
import os
from pathlib import Path
import subprocess
import sys

sys.dont_write_bytecode = True

from ui_regression.build_capture import build_capture
from ui_regression.report import write_report


def revision(root):
    return subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip()


def main():
    current = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--baseline', required=True, type=Path)
    parser.add_argument('--baseline-build', type=Path)
    parser.add_argument('--current-build', type=Path, default=current / 'build')
    parser.add_argument('--output', type=Path, default=current / '.cache/ui-comparison')
    parser.add_argument('--scale', type=int, default=3, choices=(1, 2, 3, 4))
    parser.add_argument('--reuse-baseline', action='store_true')
    parser.add_argument('--report-only', action='store_true')
    args = parser.parse_args()
    baseline = args.baseline.resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    if not args.report_only:
        builds = [('baseline', baseline, (args.baseline_build or baseline / 'build').resolve()),
                  ('after', current, args.current_build.resolve())]
        for label, root, build in builds:
            destination = output / label
            if label == 'baseline' and args.reuse_baseline:
                continue
            if 'CMAKE_BUILD_TYPE:STRING=Release' not in (build / 'CMakeCache.txt').read_text():
                parser.error(f'{build} must be configured as Release to keep debug menus out of the comparison')
            binary = build_capture(root, build, output / f'{label}-driver')
            command = ['xvfb-run', '-a', '-s', '-screen 0 1280x900x24 -nolisten tcp',
                       sys.executable, str(current / 'scripts/ui_regression/capture_cases.py'),
                       str(root), str(binary), str(destination), str(args.scale)]
            subprocess.run(command, env=dict(os.environ, WAVEFORGE_UI_XVFB='1'), check=True)
        metadata = {
            'baseline_commit': revision(baseline),
            'current_commit': revision(current),
            'working_tree_changes': subprocess.check_output(['git', 'diff', '--stat'], cwd=current, text=True),
            'scale': args.scale,
            'build_type': 'Release',
            'baseline_build': str(args.baseline_build or baseline / 'build'),
            'current_build': str(args.current_build),
            'display': 'Private xvfb-run display; software OpenGL; muted listener; isolated XDG_CONFIG_HOME',
            'seed': 'ui-comparison (main physics RNG; independent thermal RNG is not pinned)',
            'limitations': 'Fixed hello level and listed input paths; not an exhaustive gameplay/audio/save migration test',
        }
        (output / 'metadata.json').write_text(json.dumps(metadata, indent=2), encoding='utf-8')
    failed = write_report(output, before=output / 'before')
    print(f'Report: {output / "report.html"}; unexpected differences: {failed}')
    return int(bool(failed))


if __name__ == '__main__':
    sys.exit(main())
