"""Produce a standalone screenshot report and machine-readable pixel counts."""

import base64
import html
import io
import json
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw

from .capture_cases import CASES


def mismatch_mask(reference, candidate):
    diff = ImageChops.difference(reference.convert('RGB'), candidate.convert('RGB'))
    red, green, blue = diff.split()
    return ImageChops.lighter(ImageChops.lighter(red, green), blue).point(lambda value: 255 if value else 0)


def count_pixels(mask):
    return mask.histogram()[255]


def image_uri(image):
    buffer = io.BytesIO()
    image.save(buffer, format='PNG')
    return 'data:image/png;base64,' + base64.b64encode(buffer.getvalue()).decode()


def accepted_regions(name):
    regions = []
    if name.startswith('main-') or name in ('help-back', 'credits-back'):
        regions.append(((31, 186, 73, 192), 'Build commit identifier differs between revisions'))
    if name.startswith('settings') or name == 'navigation-settings':
        regions.append(((62, 76, 68, 82), 'Preserve the spelling correction Volumn → Volume'))
    return regions


def write_report(output: Path, before: Path | None = None):
    rows = []
    sections = []
    order = {case[0]: index for index, case in enumerate(CASES)}
    references = sorted((output / 'baseline').glob('*.png'), key=lambda path: order.get(path.stem, len(order)))
    for reference_path in references:
        name = reference_path.stem
        reference = Image.open(reference_path).convert('RGB')
        candidate = Image.open(output / 'after' / reference_path.name).convert('RGB')
        if candidate.size != reference.size:
            raise RuntimeError(f'{name}: screenshot dimensions differ')
        scale = reference.width // 256
        mask = mismatch_mask(reference, candidate)
        unexpected = mask.copy()
        regions = accepted_regions(name)
        for (left, top, right, bottom), _reason in regions:
            ImageDraw.Draw(unexpected).rectangle(
                (left * scale, top * scale, right * scale - 1, bottom * scale - 1), fill=0
            )
        previous = None
        previous_path = before / reference_path.name if before else None
        if previous_path and previous_path.exists():
            previous = Image.open(previous_path).convert('RGB')
            if previous.size != reference.size:
                previous = None
        row = {
            'case': name,
            'scale': scale,
            'before_pixels': count_pixels(mismatch_mask(reference, previous)) / scale**2 if previous else None,
            'after_pixels': count_pixels(mask) / scale**2,
            'unexpected_pixels': count_pixels(unexpected) / scale**2,
            'accepted_regions': regions,
        }
        rows.append(row)
        diff = Image.new('RGB', reference.size, '#222222')
        diff.paste('#b5a000', mask=mask)
        diff.paste('#ff3f63', mask=unexpected)
        panels = [('v0.4', reference)]
        if previous:
            panels.append(('TypeScript · before', previous))
        panels.extend([('TypeScript · after', candidate), ('Difference · yellow = accepted, red = unexpected', diff)])
        figures = ''.join(
            f'<figure><figcaption>{html.escape(label)}</figcaption><img alt="{html.escape(label)}" loading="lazy" src="{image_uri(image)}"></figure>'
            for label, image in panels
        )
        status = 'PASS' if not row['unexpected_pixels'] else 'REVIEW'
        notes = '; '.join(reason for _region, reason in regions)
        opened = ' open' if name in ('main-play', 'settings', 'levels-locked', 'game', 'paused', 'credits') else ''
        sections.append(
            f'<details{opened}><summary><b>{html.escape(name)}</b><span class="{status}">{status}</span> '
            f'{row["after_pixels"]:g} changed pixels; {row["unexpected_pixels"]:g} unexpected</summary>'
            f'<p>{html.escape(notes)}</p><div class="panels">{figures}</div></details>'
        )
    if not rows:
        raise RuntimeError('No baseline screenshots found')
    failed = sum(bool(row['unexpected_pixels']) for row in rows)
    (output / 'results.json').write_text(json.dumps(rows, indent=2), encoding='utf-8')
    metadata_path = output / 'metadata.json'
    metadata = metadata_path.read_text() if metadata_path.exists() else '{}'
    report = '''<!doctype html><html lang="zh-CN"><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>Waveforge v0.4 UI regression comparison</title>
<style>
body{margin:32px auto;padding:0 24px;max-width:1800px;background:#11151b;color:#eef2f5;font:15px/1.6 system-ui}
h1{font-size:28px;margin-bottom:6px}p{color:#b5c0ce}a{color:#fac82e}
summary{cursor:pointer;padding:14px 18px;background:#202833}details{border:1px solid #354253;border-radius:6px;margin:12px 0;overflow:hidden}
summary b{display:inline-block;min-width:220px}summary span{margin-right:12px;font-weight:bold}.PASS{color:#7ae3a1}.REVIEW{color:#ff6c85}
.panels{display:flex;flex-wrap:wrap;gap:12px;padding:12px}figure{flex:1 1 300px;margin:0;min-width:256px}figcaption{font-size:12px;color:#aebcd0;padding-bottom:8px}
img{width:100%;image-rendering:pixelated;display:block}details p{padding:0 18px}pre{white-space:pre-wrap;font-size:12px;color:#aebcd0}
button{padding:8px 14px;margin:0 10px 10px 0;background:#263343;color:#eef2f5;border:1px solid #536578;border-radius:4px;cursor:pointer}
</style>'''
    report += f'<h1>Waveforge v0.4 → TypeScript</h1><p>{len(rows) - failed}/{len(rows)} cases pass after explicit exceptions. '
    report += 'Pixel counts are in native 256×192 pixels; screenshots use nearest-neighbor integer scaling.</p>'
    report += '''<p>两版均为独立 Release 构建，使用专用 Xvfb、软件 OpenGL、相同进度和隔离存档。
只替换测试入口以选择场景、固定主随机种子、发送事件并在指定逻辑帧读取窗口；场景、布局和渲染实现保持各自版本。
对比 RGB 通道，不以相同 alpha 掩盖差异。仅豁免版本哈希和保留的 Volume 拼写修正。</p>
<p>覆盖主菜单、设置及确认/提示状态、帮助、制作人员、锁定/中段/末尾选关、游戏、暂停/帮助、重试提示、死亡和完成动画，以及真实跨场景导航。
此检查验证列出的画面和输入路径，不代替全部关卡、音频及存档迁移测试。</p>
<button onclick="document.querySelectorAll('details').forEach(d=>d.open=true)">展开全部</button>
<button onclick="document.querySelectorAll('details').forEach(d=>d.open=false)">折叠全部</button>'''
    report += ''.join(sections)
    report += '<details><summary>Build and capture metadata</summary><pre>' + html.escape(metadata) + '</pre></details>'
    (output / 'report.html').write_text(report, encoding='utf-8')
    return failed
