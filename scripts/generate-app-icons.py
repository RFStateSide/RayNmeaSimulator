# Copyright (C) 2026 RF StateSide LLC; SPDX-License-Identifier: AGPL-3.0-or-later
"""Regenerate platform icons from the owner-approved transparent source.

Run from any directory with Python/Pillow. Tight framing, no decorative padding.
Pillow is a development asset tool, not an application/runtime dependency.
"""
from pathlib import Path
from PIL import Image

root = Path(__file__).resolve().parents[1] / "images"
source = Image.open(root / "Glossy Wireless Signal Ship Icon.png").convert("RGBA")
assert source.width == source.height and source.getextrema()[3][0] == 0
# Faint near-transparent export speckles must not reserve a wide invisible margin.
# Frame the visible artwork, retaining its aspect ratio on the smallest square.
# The authoritative source stays untouched; no colored background is introduced.
bounds = source.getchannel("A").point(lambda alpha: 255 if alpha >= 16 else 0).getbbox()
assert bounds is not None
artwork = source.crop(bounds)
side = max(artwork.size)
source = Image.new("RGBA", (side, side), (0, 0, 0, 0))
source.alpha_composite(artwork, ((side - artwork.width) // 2, (side - artwork.height) // 2))
source.resize((1024, 1024), Image.Resampling.LANCZOS).save(root / "RayNmeaSimulator_1024.png")
source.save(root / "RayNmeaSimulator.ico", sizes=[(s, s) for s in (16, 24, 32, 48, 64, 128, 256)])
# ICNS otherwise uses Pillow's default resize filter. Supply each representation
# explicitly so macOS and the Qt/Linux PNG use the same resampling policy.
source.save(root / "RayNmeaSimulator.icns", append_images=[
    source.resize((size, size), Image.Resampling.LANCZOS)
    for size in (32, 64, 128, 256, 512, 1024)
])
