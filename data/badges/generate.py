#!/usr/bin/env python3
"""Writes the badge icons (sc-emblems-folderbadges-<id>.svg) next to this script.

The SVGs are committed, so this only needs to be run after editing the table below.
Each badge is a coloured rounded square with a short label or a simple glyph, drawn to
stay readable at the 16-22 px Dolphin uses for overlays.
"""

from pathlib import Path

# id: (background, foreground, label)
LABELS = {
    "neovim": ("#3F8F3A", "#FFFFFF", "NV"),
    "termux": ("#111111", "#FFFFFF", ">_"),
    "rust": ("#B7410E", "#FFFFFF", "RS"),
    "nix": ("#5277C3", "#FFFFFF", "NIX"),
    "typescript": ("#3178C6", "#FFFFFF", "TS"),
    "javascript": ("#F7DF1E", "#111111", "JS"),
    "go": ("#00ADD8", "#FFFFFF", "GO"),
    "python": ("#3776AB", "#FFD43B", "PY"),
    "java": ("#E76F00", "#FFFFFF", "JV"),
    "kotlin": ("#7F52FF", "#FFFFFF", "KT"),
    "haskell": ("#5E5086", "#FFFFFF", "HS"),
    "ruby": ("#CC342D", "#FFFFFF", "RB"),
    "php": ("#777BB4", "#FFFFFF", "PHP"),
    "cpp": ("#00599C", "#FFFFFF", "C++"),
    "c": ("#5C6BC0", "#FFFFFF", "C"),
    "lua": ("#000080", "#FFFFFF", "LUA"),
    "qml": ("#2CA84A", "#FFFFFF", "QML"),
    "shell": ("#2E3436", "#8AE234", "SH"),
    "sql": ("#D98200", "#FFFFFF", "SQL"),
    "r": ("#276DC3", "#FFFFFF", "R"),
    "latex": ("#008080", "#FFFFFF", "TEX"),
    "web": ("#E34F26", "#FFFFFF", "&lt;/&gt;"),
    "pdf": ("#D32F2F", "#FFFFFF", "PDF"),
}

# id: (background, svg body drawn in white on the 64x64 canvas)
GLYPHS = {
    # "M" and a down arrow, as in the Markdown mark
    "markdown": (
        "#24292F",
        '<path d="M10 44V20h7l7 9 7-9h7v24h-7V31l-7 9-7-9v13z" fill="#fff"/>'
        '<path d="M48 20v12h6l-9.5 12L35 32h6V20z" fill="#fff" transform="translate(4 0)"/>',
    ),
    # picture: sun and mountains
    "images": (
        "#8E24AA",
        '<circle cx="22" cy="23" r="6" fill="#fff"/>'
        '<path d="M8 50l14-17 9 10 8-9 17 16z" fill="#fff"/>',
    ),
    # eighth notes
    "audio": (
        "#EF6C00",
        '<path d="M26 14l24-5v29a7 7 0 1 1-6-6.9V20l-12 2.5V44a7 7 0 1 1-6-6.9z" fill="#fff"/>',
    ),
    # play triangle
    "video": (
        "#37474F",
        '<path d="M24 16l26 16-26 16z" fill="#fff"/>',
    ),
}

FRAME = (
    '<svg xmlns="http://www.w3.org/2000/svg" width="64" height="64" viewBox="0 0 64 64">\n'
    # white rim so the badge separates from any folder colour
    '  <rect x="1" y="1" width="62" height="62" rx="14" fill="#fff"/>\n'
    '  <rect x="4" y="4" width="56" height="56" rx="11" fill="{bg}"/>\n'
    "  {body}\n"
    "</svg>\n"
)

FONT_SIZE = {1: 46, 2: 33, 3: 24}


def label_body(fg: str, label: str) -> str:
    visible = label.replace("&lt;", "<").replace("&gt;", ">")
    size = FONT_SIZE[len(visible)]
    baseline = 32 + size * 0.36
    return (
        f'<text x="32" y="{baseline:.1f}" font-family="Noto Sans, DejaVu Sans, sans-serif" '
        f'font-weight="bold" font-size="{size}" text-anchor="middle" fill="{fg}">{label}</text>'
    )


def main() -> None:
    here = Path(__file__).parent
    for badge_id, (bg, fg, label) in LABELS.items():
        svg = FRAME.format(bg=bg, body=label_body(fg, label))
        (here / f"sc-emblems-folderbadges-{badge_id}.svg").write_text(svg)
    for badge_id, (bg, body) in GLYPHS.items():
        (here / f"sc-emblems-folderbadges-{badge_id}.svg").write_text(FRAME.format(bg=bg, body=body))


if __name__ == "__main__":
    main()
