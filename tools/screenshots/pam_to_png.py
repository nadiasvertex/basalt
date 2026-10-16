#!/usr/bin/env python3
"""Converts the PAM screenshots of the widget examples to PNG files.

usage: pam_to_png.py <output directory> <file.pam>...

Each input becomes <output directory>/<input name>.png. Pillow does not read PAM files, so this
script reads the header itself.
"""
import pathlib
import sys

from PIL import Image


def read_pam(path: pathlib.Path) -> Image.Image:
    data = path.read_bytes()
    marker = b"ENDHDR\n"
    end = data.index(marker) + len(marker)
    header = dict(line.split(maxsplit=1) for line in data[:end].decode().splitlines()[1:-1])
    size = (int(header["WIDTH"]), int(header["HEIGHT"]))
    return Image.frombytes("RGBA", size, data[end:]).convert("RGB")


def main(arguments: list[str]) -> int:
    if len(arguments) < 2:
        print(__doc__, file=sys.stderr)
        return 2
    output = pathlib.Path(arguments[0])
    output.mkdir(parents=True, exist_ok=True)
    for name in arguments[1:]:
        source = pathlib.Path(name)
        read_pam(source).save(output / (source.stem + ".png"), optimize=True)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
