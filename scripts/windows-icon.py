"""Wrap the 256px project icon PNG in a Windows ICO directory."""
from pathlib import Path
from struct import pack
from sys import argv

source, destination = map(Path, argv[1:])
png = source.read_bytes()
header = pack("<HHH", 0, 1, 1)
entry = pack("<BBBBHHII", 0, 0, 0, 0, 1, 32, len(png), 22)
destination.write_bytes(header + entry + png)
