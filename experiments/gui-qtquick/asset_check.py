#!/usr/bin/env python3
"""Local-only PNG transparency/resolution audit (requires Pillow).

Does not modify assets or upload images. A clean RGBA header is not evidence
that a character cutout's corners or hair edges are actually transparent.
"""
import argparse
import json
from pathlib import Path
from PIL import Image


def audit(path: Path, kind: str, target: tuple[int, int]) -> dict:
    if kind not in ("character", "background"):
        raise ValueError("kind must be character or background")
    with Image.open(path) as im:
        im.load()
        w, h = im.size
        out = {
            "file_name": path.name,
            "kind": kind,
            "actual_pixels": [w, h],
            "requested_reference_pixels": list(target),
            "resolution_meets_reference": w >= target[0] and h >= target[1],
            "mode": im.mode,
        }
        if kind == "character":
            if "A" not in im.getbands():
                out.update(alpha_present=False, transparent_fraction=0.0,
                           corner_transparent_fraction=0.0,
                           alpha_warning="Image has no alpha channel")
                return out
            alpha = im.getchannel("A")
            hist = alpha.histogram()
            total = w * h
            # 0..8 is effectively transparent. 248..255 effectively opaque.
            transparent = sum(hist[:9])
            opaque = sum(hist[248:])
            corner_size = max(1, min(w, h, 24))
            corner_boxes = [
                (0, 0, corner_size, corner_size),
                (w - corner_size, 0, w, corner_size),
                (0, h - corner_size, corner_size, h),
                (w - corner_size, h - corner_size, w, h),
            ]
            corner_pixels = []
            for box in corner_boxes:
                corner_pixels.extend(alpha.crop(box).getdata())
            corner_clear = sum(px <= 8 for px in corner_pixels) / len(corner_pixels)
            warning = []
            if transparent == 0:
                warning.append("No transparent pixels: full opaque rectangle")
            if corner_clear < 0.75:
                warning.append("Most corners not transparent: possible rectangular matte")
            if w < target[0] or h < target[1]:
                warning.append("Source smaller than configured comparison target")
            out.update(
                alpha_present=True,
                transparent_fraction=round(transparent / total, 5),
                opaque_fraction=round(opaque / total, 5),
                corner_transparent_fraction=round(corner_clear, 5),
                alpha_warning="; ".join(warning) or None,
            )
        return out


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("file", type=Path)
    p.add_argument("--kind", choices=("character", "background"), required=True)
    p.add_argument("--target", default="1672x941",
                   help="Comparison target, not a claim about 4K generation")
    args = p.parse_args()
    try:
        sw, sh = args.target.lower().split("x")
        target = (int(sw), int(sh))
        if min(target) <= 0:
            raise ValueError("target dimensions must be positive")
        print(json.dumps(audit(args.file, args.kind, target), ensure_ascii=False,
                         indent=2))
    except (OSError, ValueError, ZeroDivisionError) as exc:
        p.error(str(exc))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
