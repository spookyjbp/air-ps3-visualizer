#!/usr/bin/env python3
"""Validate the AIR PS3 source artwork (in-app background) and the XMB icon."""
from __future__ import annotations

import sys
from pathlib import Path
from PIL import Image


def fail(message: str) -> None:
    raise SystemExit(f"AIR PS3 artwork validation failed: {message}")


def load_rgb(path: Path) -> Image.Image:
    try:
        with Image.open(path) as image:
            image.load()
            return image.convert("RGB")
    except Exception as exc:
        fail(f"cannot decode {path.name}: {type(exc).__name__}")


def main() -> None:
    root = Path(sys.argv[1]) if len(sys.argv) == 2 else Path("assets/air-ps3")
    source_path = root / "artwork-source.png"
    source = load_rgb(source_path)
    if source.size != (1182, 665):
        fail(f"unexpected source dimensions {source.size}")

    icon_path = root / "ICON0.PNG"
    try:
        with Image.open(icon_path) as icon:
            icon.load()
            if icon.format != "PNG" or icon.size != (320, 176):
                fail(f"ICON0.PNG must be a 320x176 PNG, got {icon.format} {icon.size}")
            mode = icon.mode
    except SystemExit:
        raise
    except Exception as exc:
        fail(f"cannot decode ICON0.PNG: {type(exc).__name__}")
    # The XMB background (PIC1.PNG) is intentionally not shipped, so the
    # console's normal XMB background shows while the app is highlighted.
    if (root / "PIC1.PNG").exists():
        fail("PIC1.PNG should not be present (no XMB background is shipped)")
    print(f"AIR PS3 artwork validation passed: source pixels, ICON0 320x176 {mode}, no PIC1")

if __name__ == "__main__":
    main()
