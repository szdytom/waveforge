"""Capture deterministic frames in an isolated Xvfb session."""

import json
import os
import pathlib
import subprocess
import sys

CASES = [
    ('main-new', 'main-menu', 0, 2, ''),
    ('main-play', 'main-menu', 1, 2, ''),
    ('main-settings-selected', 'main-menu', 1, 3, '2:down'),
    ('help', 'help', 0, 2, ''),
    ('credits', 'credits', 0, 2, ''),
    ('settings', 'settings', 0, 2, ''),
    ('settings-volume', 'settings', 0, 3, '2:down'),
    ('settings-confirm', 'settings', 0, 8, '2:down,3:down,4:down,5:down,6:down,7:enter'),
    ('settings-cheat', 'settings', 0, 7, '2:x,3:y,4:z,5:z,6:y'),
    ('settings-cheat-faded', 'settings', 0, 90, '2:x,3:y,4:z,5:z,6:y'),
    ('levels-locked', 'level-menu', 0, 2, ''),
    ('levels-middle', 'level-menu', 8, 2, ''),
    ('levels-last', 'level-menu', 18, 2, ''),
    ('loading', 'load-level', 0, 5, ''),
    ('game', 'play-level', 0, 10, ''),
    ('paused', 'play-level', 0, 3, '2:esc'),
    ('paused-retry', 'play-level', 0, 4, '2:esc,3:down'),
    ('paused-help', 'play-level', 0, 6, '2:esc,3:down,4:down,5:enter'),
    ('retry-hint', 'play-level', 0, 52, '51:r'),
] + [(f'death-{tick:03}', 'level-death', 0, tick, '') for tick in (1,60,61,63,95,140)] + [
    (f'complete-{tick:03}', 'level-complete', 0, tick, '') for tick in (1,25,35,45,55)
] + [
    ('navigation-settings', 'main-menu', 0, 5, '2:down,3:enter'),
    ('navigation-help', 'main-menu', 0, 6, '2:down,3:down,4:enter'),
    ('navigation-play', 'main-menu', 0, 5, '2:enter'),
    ('help-back', 'help', 0, 5, '2:enter'),
    ('credits-back', 'credits', 0, 5, '2:enter'),
    ('loading-to-game', 'load-level', 0, 40, ''),
    ('pause-resume', 'play-level', 0, 6, '2:esc,4:enter'),
    ('pause-quit', 'play-level', 0, 6, '2:esc,3:up,4:enter'),
    ('death-skip', 'level-death', 0, 6, '2:enter'),
]

def capture_cases(root, binary, out, scale=3, only=None):
    if os.environ.get("WAVEFORGE_UI_XVFB") != "1":
        raise RuntimeError("Run captures through ui-regression.py to allocate a private Xvfb display")
    legacy = (root / 'src/save.cpp').exists()
    out.mkdir(parents=True, exist_ok=True)
    cases = [case for case in CASES if only is None or case[0] in only]
    for name, scene, completed, end, keys in cases:
        config = out / 'config' / name
        save = config / 'waveforge' / 'save-0.4.json'
        save.parent.mkdir(parents=True, exist_ok=True)
        if legacy:
            data = {
                'completed_levels': completed,
                'user_settings': {
                    'scale': 0,
                    'volume': 80,
                    'strict_pixel_perfection': False,
                    'skip_animations': False,
                    'debug_heat_render': False,
                },
            }
        else:
            data = {'completed_levels': str(completed)}
        save.write_text(json.dumps(data))
        route = 'level/hello'
        if scene in ('level-death', 'level-complete'):
            route = json.dumps({
                'levelId': 'level/hello', 'width': 256, 'height': 192,
                'duckX': 40, 'duckY': 60, 'failed': False,
            })
        env = dict(
            os.environ,
            XDG_CONFIG_HOME=str(config),
            WF_SCENE=scene,
            WF_DATA=route,
            WF_END=str(end),
            WF_KEYS=keys,
            WF_OUTPUT=str(out / f'{name}.png'),
            LIBGL_ALWAYS_SOFTWARE='1',
        )
        with (out / f'{name}.log').open('w') as log:
            subprocess.run(
                [str(binary), '--scale', str(scale)], cwd=root, env=env,
                stdout=log, stderr=subprocess.STDOUT, check=True, timeout=30,
            )
        print(name, flush=True)
    (out / 'cases.json').write_text(json.dumps(cases, indent=2))

if __name__ == '__main__':
    capture_cases(*(pathlib.Path(arg).resolve() for arg in sys.argv[1:4]), scale=int(sys.argv[4]))
