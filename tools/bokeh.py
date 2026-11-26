# Copyright (C) 2026 Koh Swee Teck Dedrick.
# Licensed under the MIT license (https://opensource.org/license/mit/).
#
# Builds a bokeh mask from a stamp image.
# The runtime samples the mask over its texels and scales that pattern by the
# current blur radius.
#
# USAGE
#   python bokeh.py <stamp_path> [--mask-dim DIM] [--out PATH]
#
# BINARY LUT FORMAT (little-endian)
#  struct Header {
#   u32 mask_dim
#  }
#  f32 mask[mask_dim * mask_dim]

import argparse
import os
import struct
import numpy as np
from PIL import Image


def main():
	#~ Dedrick: Parse arguments.
	parser = argparse.ArgumentParser(description="Bokeh mask generator")
	parser.add_argument("stamp_path", metavar="STAMP_PATH", help="Input stamp image")
	parser.add_argument("--mask-dim", type=int, default=64, metavar="DIM", help="Mask dimension, rounded up to odd (default 64)")
	parser.add_argument("--out", default="bokeh_lut.bin", metavar="PATH", help="Output mask binary path (default bokeh_lut.bin)")
	arguments = parser.parse_args()
	mask_dim = arguments.mask_dim | 1

	#~ Dedrick: Build the stamp mask.
	stamp = Image.open(arguments.stamp_path).convert("L")
	stamp_pixels = np.asarray(stamp, dtype=np.float32)
	stamp_rows, stamp_cols = np.nonzero(stamp_pixels)
	stamp_bounds = (int(stamp_cols.min()), int(stamp_rows.min()), int(stamp_cols.max()) + 1, int(stamp_rows.max()) + 1)
	stamp_width = stamp_bounds[2] - stamp_bounds[0]
	stamp_height = stamp_bounds[3] - stamp_bounds[1]
	stamp = stamp.crop(stamp_bounds)
	mask_scale = mask_dim / max(stamp_width, stamp_height)
	mask_width = int(round(stamp_width * mask_scale)) | 1
	mask_height = int(round(stamp_height * mask_scale)) | 1
	stamp_resized = stamp.resize((mask_width, mask_height), Image.Resampling.BOX if mask_scale < 1.0 else Image.Resampling.BILINEAR)
	mask = Image.new("L", (mask_dim, mask_dim), 0)
	mask.paste(stamp_resized, ((mask_dim - mask_width) // 2, (mask_dim - mask_height) // 2))
	# NOTE(Dedrick): The runtime samples with uv + tap, so the mask is rotated
	# 180 degrees here to keep the stamp orientation.
	mask = mask.transpose(Image.Transpose.ROTATE_180)
	mask_pixels = np.asarray(mask, dtype=np.float32) / 255.0

	#~ Dedrick: Export mask.
	header_format = "<I"
	lut_bytes = bytearray(struct.calcsize(header_format))
	struct.pack_into(header_format, lut_bytes, 0, mask_dim)
	lut_bytes += mask_pixels.tobytes()
	out_directory = os.path.dirname(arguments.out)
	if out_directory:
		os.makedirs(out_directory, exist_ok=True)
	with open(arguments.out, "wb") as lut_file:
		lut_file.write(lut_bytes)
	print(f"Exported bokeh mask to '{arguments.out}' ({mask_dim}x{mask_dim}, {len(lut_bytes)} bytes)")


if __name__ == "__main__":
	main()
