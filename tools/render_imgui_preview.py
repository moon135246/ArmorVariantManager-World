"""Render exported ImGui draw data and its font atlas for offline visual QA."""
import json
import sys
from pathlib import Path
import numpy as np
from PIL import Image

folder = Path(sys.argv[1])
data = json.loads((folder / 'drag_preview.json').read_text())
width, height = int(data['width']), int(data['height'])
atlas = np.fromfile(folder / 'atlas.rgba', dtype=np.uint8).reshape(
    data['atlas_height'], data['atlas_width'], 4)
aw, ah = data['atlas_width'], data['atlas_height']
canvas = np.zeros((height, width, 3), dtype=np.float32)
canvas[:] = (14, 19, 28)
for draw_list in data['lists']:
    vertices = np.asarray(draw_list['vertices'], dtype=np.float64)
    indices = np.asarray(draw_list['indices'], dtype=np.int32)
    if not len(vertices):
        continue
    colors = vertices[:, 4].astype(np.uint32)
    rgba = np.stack([(colors >> shift) & 255 for shift in (0, 8, 16, 24)], axis=1).astype(np.float32) / 255
    for count, offset, base, cx0, cy0, cx1, cy1 in draw_list['commands']:
        for start in range(int(offset), int(offset + count), 3):
            vi = indices[start:start+3] + int(base)
            triangle = vertices[vi]
            x0 = max(0, int(np.floor(max(cx0, triangle[:, 0].min()))))
            y0 = max(0, int(np.floor(max(cy0, triangle[:, 1].min()))))
            x1 = min(width, int(np.ceil(min(cx1, triangle[:, 0].max()))))
            y1 = min(height, int(np.ceil(min(cy1, triangle[:, 1].max()))))
            if x1 <= x0 or y1 <= y0:
                continue
            a, b, c = triangle[:, :2]
            denom = (b[1]-c[1])*(a[0]-c[0])+(c[0]-b[0])*(a[1]-c[1])
            if abs(denom) < 1e-7:
                continue
            yy, xx = np.mgrid[y0:y1, x0:x1].astype(np.float32)
            xx += .5
            yy += .5
            wa = ((b[1]-c[1])*(xx-c[0])+(c[0]-b[0])*(yy-c[1])) / denom
            wb = ((c[1]-a[1])*(xx-c[0])+(a[0]-c[0])*(yy-c[1])) / denom
            wc = 1-wa-wb
            mask = (wa >= -1e-5) & (wb >= -1e-5) & (wc >= -1e-5)
            if not mask.any():
                continue
            weights = np.stack((wa[mask], wb[mask], wc[mask]), axis=1)
            uv = weights @ triangle[:, 2:4]
            # Bilinear atlas sampling matches the backend's font sampler.
            tx, ty = uv[:, 0]*aw-.5, uv[:, 1]*ah-.5
            ix, iy = np.floor(tx).astype(int), np.floor(ty).astype(int)
            fx, fy = (tx-ix)[:, None], (ty-iy)[:, None]
            ix0, ix1 = np.clip(ix, 0, aw-1), np.clip(ix+1, 0, aw-1)
            iy0, iy1 = np.clip(iy, 0, ah-1), np.clip(iy+1, 0, ah-1)
            texel = ((atlas[iy0, ix0]*(1-fx)+atlas[iy0, ix1]*fx)*(1-fy)
                     +(atlas[iy1, ix0]*(1-fx)+atlas[iy1, ix1]*fx)*fy) / 255
            ink = texel * (weights @ rgba[vi])
            alpha = ink[:, 3:4]
            patch = canvas[y0:y1, x0:x1]
            patch[mask] = ink[:, :3]*255*alpha + patch[mask]*(1-alpha)
output = Path(sys.argv[2]) if len(sys.argv) > 2 else folder / 'drag_preview.png'
Image.fromarray(np.clip(canvas, 0, 255).astype(np.uint8)).save(output)
print(output)
