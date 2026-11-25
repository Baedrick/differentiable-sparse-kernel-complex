# Copyright (C) 2026 Koh Swee Teck Dedrick.
# Licensed under the MIT license (https://opensource.org/license/mit/).
#
# Offline trainer for a differentiable sparse kernel complex.
# Samples an ordered basis of M kernels, trains each member into a cascade of L
# passes with N samples per pass, and writes the blend-ready runtime LUT binary.
#
# USAGE
#   python dskc.py --gaussian <sigma_min> <sigma_max> [options]
#   python dskc.py --custom <stamp_path> <px_min> <px_max> [options]
#
# BINARY LUT FORMAT (little-endian)
#  struct Header {
#   f32 param_min
#   f32 param_max
#   u32 layers
#   u32 samples
#   u32 directory_offset
#   u32 directory_count
#  }
#  struct DirectoryEntry {
#   f32 param
#   u32 tap_offset
#   u32 tap_count
#  }
#  struct Tap {
#   f32 offset_x
#   f32 offset_y
#   f32 weight
#  }
#
# IMPLEMENTATION DETAILS
# 1. The ring step is dr = 2 * extent / (L * (L + 1)) so the cumulative ring
#     walk covers the full target extent.
# 2. The training canvas starts at 49 pixels. It grows when the kernel extent
#     plus the cumulative ring walk no longer fits. Border padding clamps
#     samples that land past the edge and zeroes the gradients of taps that
#     wander outside the canvas. Trained offsets clamp to the member support
#     extent.
# 3. Custom stamps crop to their ink bounding box and center on the canvas.
#     Runtime sampling fetches at position plus offset, so the trainer stores
#     center minus position to compute convolution.
# 4. Accepted coordinates are an unordered set. A raster prefix scan would
#     strand early taps in the top band of ink because greedy acceptance
#     exhausts after about (pi / 2) * S pixels. Candidates come from a random
#     permutation of the support instead. Accepted coordinates sort
#     lexicographically so slot positions line up across basis members.
# 5. Weights use sum-to-one normalization. Unconstrained signed weights produce
#     negative values in the composite impulse response and dark halos around
#     bright features after tonemapping. A softplus gate keeps weights positive
#     before normalization. A gate scale of 8 keeps weight gradients on par
#     with coordinate offset updates at the default learning rate.
# 6. Each basis member trains on its own. Adjacent members share aligned tap
#     slots by initializing member k from scaled parameters of member k - 1.
#     Independent training produces inconsistent slot permutations that cause
#     visual splitting during runtime linear interpolation.
# 7. The member count follows the scale ratio gap as
#     ceil(log(param_max / param_min) / log(gap)). The count clamps to two so
#     runtime interpolation always has a bracket. A gap of 1.25 preserves
#     reconstruction accuracy across adjacent tap blends.
# 8. The default schedule runs 3000 steps and decays the learning rate from
#     1e-2 to 1e-3, which converges across the tested range.

import argparse
import math
import os
import struct
import numpy as np
import torch
import torch.nn.functional as F
from PIL import Image


# NOTE(Dedrick): Stamps treat pixels below the line as no support. The gaussian
# render window lands on the line. One percent of peak bounds 99 percent of the
# kernel mass.
DSKC_CanvasDimMin = 49
DSKC_SupportFloor = 0.01


def dk_gated_weights(weights):
	weights = F.softplus(weights * 8.0) / 8.0 + 1e-8
	weights = weights / weights.sum()
	return weights


def main():
	#~ Dedrick: Set up parser.
	parser = argparse.ArgumentParser(description="DSKC offline trainer")
	mode = parser.add_mutually_exclusive_group(required=True)
	mode.add_argument("--gaussian", nargs=2, type=float, metavar=("SIGMA_MIN", "SIGMA_MAX"), help="Gaussian basis across [sigma_min, sigma_max]")
	mode.add_argument("--custom", nargs=3, metavar=("STAMP_PATH", "PX_MIN", "PX_MAX"), help="Custom stamp basis across ink radius [px_min, px_max]")
	parser.add_argument("--out", default="dskc_lut.bin", metavar="PATH", help="Output runtime LUT binary path (default dskc_lut.bin)")
	parser.add_argument("--gap", type=float, default=1.25, help="Scale ratio between adjacent members (default 1.25)")
	parser.add_argument("--layers", type=int, default=4, help="Cascade passes L (default 4)")
	parser.add_argument("--samples", type=int, default=32, help="Samples per pass N (default 32)")
	parser.add_argument("--steps", type=int, default=3000, help="Optimization steps per member (default 3000)")
	parser.add_argument("--lr", type=float, default=1e-2, help="Initial Adam rate (default 1e-2)")
	parser.add_argument("--backend", choices=("auto", "cpu", "cuda"), default="auto", help="Torch device for training (default auto)")

	#~ Dedrick: Parse arguments.
	arguments = parser.parse_args()
	device = arguments.backend
	if device == "auto":
		device = "cuda" if torch.cuda.is_available() else "cpu"

	#~ Dedrick: Generate gaussian or custom kernels.
	if arguments.gaussian:
		param_min, param_max = arguments.gaussian
	else:
		stamp_path = arguments.custom[0]
		param_min = float(arguments.custom[1])
		param_max = float(arguments.custom[2])

	member_count = max(2, math.ceil(math.log(param_max / param_min) / math.log(arguments.gap)))
	member_params = [param_min * (param_max / param_min) ** (member_index / (member_count - 1)) for member_index in range(member_count)]

	kernels = []
	if arguments.gaussian:
		for member_param in member_params:
			canvas_half_dim = max(DSKC_CanvasDimMin // 2, math.ceil(member_param * math.sqrt(-2.0 * math.log(DSKC_SupportFloor))))
			axis = np.arange(-canvas_half_dim, canvas_half_dim + 1, dtype=np.float32)
			mesh_col, mesh_row = np.meshgrid(axis, axis)
			radius_squared = mesh_col * mesh_col + mesh_row * mesh_row
			kernel = np.exp(-radius_squared / (2.0 * member_param * member_param))
			kernel[kernel < DSKC_SupportFloor * kernel.max()] = 0.0
			kernels.append((member_param, kernel / float(kernel.sum())))
	else:
		stamp = Image.open(stamp_path).convert("L")
		stamp_pixels = np.asarray(stamp, dtype=np.float32)
		stamp_rows, stamp_cols = np.nonzero(stamp_pixels > DSKC_SupportFloor * stamp_pixels.max())
		stamp_bounds = (int(stamp_cols.min()), int(stamp_rows.min()), int(stamp_cols.max()) + 1, int(stamp_rows.max()) + 1)
		stamp_width = stamp_bounds[2] - stamp_bounds[0]
		stamp_height = stamp_bounds[3] - stamp_bounds[1]
		stamp = stamp.crop(stamp_bounds)
		for member_param in member_params:
			member_scale = 2.0 * member_param / max(stamp_width, stamp_height)
			member_width = int(round(stamp_width * member_scale)) | 1
			member_height = int(round(stamp_height * member_scale)) | 1
			stamp_resized = stamp.resize((member_width, member_height), Image.Resampling.BOX if member_scale < 1.0 else Image.Resampling.BILINEAR)
			canvas_size = max(DSKC_CanvasDimMin, member_width, member_height)
			canvas = Image.new("L", (canvas_size, canvas_size), 0)
			canvas.paste(stamp_resized, ((canvas_size - member_width) // 2, (canvas_size - member_height) // 2))
			kernel = np.asarray(canvas, dtype=np.float32) / 255.0
			kernel[kernel < DSKC_SupportFloor * kernel.max()] = 0.0
			kernels.append((member_param, kernel / float(kernel.sum())))

	#~ Dedrick: Set up taps.
	previous_taps = np.empty((arguments.layers, arguments.samples, 3), dtype=np.float32)
	first_kernel = kernels[0][1]
	first_kernel_rows, first_kernel_cols = first_kernel.shape
	first_center_row, first_center_col = first_kernel_rows // 2, first_kernel_cols // 2
	first_support_rows, first_support_cols = np.nonzero(first_kernel)
	first_support_extent = max(first_support_rows.max() - first_support_rows.min() + 1, first_support_cols.max() - first_support_cols.min() + 1) * 0.5
	first_ring_step = 2.0 * first_support_extent / (arguments.layers * (arguments.layers + 1))

	#~ Dedrick: Pass 1 greedy rejection sampling.
	support_count = first_support_rows.size
	rejection_radius_squared = support_count / (arguments.samples * math.pi)
	scan_order = np.random.default_rng(0).permutation(support_count)
	chosen_indices = []
	squared_distance_min = np.full(support_count, np.inf)
	for scan_index in scan_order:
		if len(chosen_indices) >= arguments.samples:
			break
		if squared_distance_min[scan_index] >= rejection_radius_squared:
			chosen_indices.append(scan_index)
			np.minimum(squared_distance_min, (first_support_rows - first_support_rows[scan_index]) ** 2 + (first_support_cols - first_support_cols[scan_index]) ** 2, out=squared_distance_min)
	for fill_slot in range(arguments.samples - len(chosen_indices)):
		chosen_indices.append(fill_slot % support_count)
	chosen_indices = np.asarray(chosen_indices)
	slot_order = np.lexsort((first_support_cols[chosen_indices], first_support_rows[chosen_indices]))
	previous_taps[0, :, 0] = first_center_col - first_support_cols[chosen_indices[slot_order]]
	previous_taps[0, :, 1] = first_center_row - first_support_rows[chosen_indices[slot_order]]
	previous_taps[0, :, 2] = 1.0 / arguments.samples

	#~ Dedrick: Pass 2..L ring sampling.
	ring_angles = np.arange(arguments.samples, dtype=np.float32) * (2.0 * math.pi / arguments.samples)
	ring_cos, ring_sin = np.cos(ring_angles), np.sin(ring_angles)
	for layer_index in range(1, arguments.layers):
		ring_radius = (layer_index + 1) * first_ring_step
		previous_taps[layer_index, :, 0] = ring_radius * ring_cos
		previous_taps[layer_index, :, 1] = ring_radius * ring_sin
		previous_taps[layer_index, :, 2] = 1.0 / arguments.samples

	#~ Dedrick: Train LUT.
	charbonnier_epsilon = 1e-4
	members = []
	learning_rate_delta = (arguments.lr * 0.1 - arguments.lr) / max(1, arguments.steps - 1)
	for member_index, (member_param, member_kernel) in enumerate(kernels):
		warm_start_scale = 1.0 if member_index == 0 else member_param / kernels[member_index - 1][0]
		kernel_rows, kernel_cols = member_kernel.shape
		support_rows, support_cols = np.nonzero(member_kernel)
		member_support_extent = max(support_rows.max() - support_rows.min() + 1, support_cols.max() - support_cols.min() + 1) * 0.5
		member_ring_step = 2.0 * member_support_extent / (arguments.layers * (arguments.layers + 1))
		reach = member_support_extent + member_ring_step * (arguments.layers * (arguments.layers + 1) // 2 - 1)
		canvas_size = max(DSKC_CanvasDimMin, 2 * math.ceil(reach) + 1)
		center = canvas_size // 2
		impulse = torch.zeros((canvas_size, canvas_size), device=device)
		impulse[center, center] = 1.0
		target = torch.zeros((canvas_size, canvas_size), device=device)
		paste_row, paste_col = center - kernel_rows // 2, center - kernel_cols // 2
		target[paste_row:paste_row + kernel_rows, paste_col:paste_col + kernel_cols] = torch.from_numpy(member_kernel).to(device)

		#~ Dedrick: Base grid and coordinate scale.
		axis = torch.linspace(-1.0, 1.0, canvas_size, device=device)
		mesh_row, mesh_col = torch.meshgrid(axis, axis, indexing="ij")
		base_grid = torch.stack((mesh_col, mesh_row), dim=-1)
		coordinate_scale = torch.tensor([2.0 / (canvas_size - 1.0), 2.0 / (canvas_size - 1.0)], device=device)

		#~ Dedrick: Trainable layer parameters.
		layer_params = []
		for layer_index in range(arguments.layers):
			offsets = (torch.tensor(previous_taps[layer_index, :, :2], device=device) * warm_start_scale).requires_grad_(True)
			weights = torch.tensor(previous_taps[layer_index, :, 2], device=device, requires_grad=True)
			layer_params.append((offsets, weights))
		optimizer = torch.optim.Adam([parameter for offsets, weights in layer_params for parameter in (offsets, weights)], lr=arguments.lr)

		#~ Dedrick: Optimization steps.
		for step in range(arguments.steps):
			optimizer.param_groups[0]["lr"] = arguments.lr + step * learning_rate_delta
			optimizer.zero_grad(set_to_none=True)
			synthesized = impulse
			for offsets, weights in layer_params:
				gated_weights = dk_gated_weights(weights)
				sample_grid = base_grid + (offsets * coordinate_scale).view(arguments.samples, 1, 1, 2)
				sampled = F.grid_sample(
					synthesized[None].expand(arguments.samples, 1, canvas_size, canvas_size),
					sample_grid,
					mode="bilinear",
					padding_mode="border",
					align_corners=True
				)
				synthesized = (sampled.view(arguments.samples, canvas_size, canvas_size) * gated_weights.view(-1, 1, 1)).sum(dim=0)
			kernel_error = synthesized - target
			loss = ((kernel_error * kernel_error + charbonnier_epsilon * charbonnier_epsilon).sqrt() - charbonnier_epsilon).mean()
			loss.backward()
			optimizer.step()
			for offsets, weights in layer_params:
				offsets.data.clamp_(min=-member_support_extent, max=member_support_extent)
			if step % 250 == 249 or step + 1 == arguments.steps:
				print(f"  member {member_index + 1}/{len(kernels)} param {member_param:.2f} step {step + 1}/{arguments.steps} loss {loss.item():.3e}", flush=True)

		#~ Dedrick: Extract trained member taps.
		member_taps = np.empty((arguments.layers, arguments.samples, 3), dtype=np.float32)
		for layer_index, (offsets, weights) in enumerate(layer_params):
			member_taps[layer_index, :, :2] = offsets.detach().cpu().numpy()
			member_taps[layer_index, :, 2] = dk_gated_weights(weights.detach()).cpu().numpy()
		members.append((member_param, member_taps))
		previous_taps = member_taps
		print(f"trained member {member_index + 1}/{len(kernels)} param {member_param:.4f} loss {loss.item():.3e}")

	#~ Dedrick: Export LUT.
	header_format = "<ffIIII"
	entry_format = "<fII"
	tap_format = "<fff"
	header_size = struct.calcsize(header_format)
	entry_size = struct.calcsize(entry_format)
	tap_count = arguments.layers * arguments.samples
	lut_bytes = bytearray(header_size)
	directory_offset = len(lut_bytes)
	lut_bytes += bytearray(len(members) * entry_size)
	header_param_min, header_param_max = float(members[0][0]), float(members[-1][0])
	struct.pack_into(header_format, lut_bytes, 0, header_param_min, header_param_max, arguments.layers, arguments.samples, directory_offset, len(members))
	for member_index, (member_param, member_taps) in enumerate(members):
		tap_offset = len(lut_bytes)
		for layer_index in range(arguments.layers):
			for tap_index in range(arguments.samples):
				lut_bytes += struct.pack(tap_format, float(member_taps[layer_index, tap_index, 0]), float(member_taps[layer_index, tap_index, 1]), float(member_taps[layer_index, tap_index, 2]))
		struct.pack_into(entry_format, lut_bytes, directory_offset + member_index * entry_size, float(member_param), tap_offset, tap_count)
	out_directory = os.path.dirname(arguments.out)
	if out_directory:
		os.makedirs(out_directory, exist_ok=True)
	with open(arguments.out, "wb") as lut_file:
		lut_file.write(lut_bytes)
	print(f"Exported runtime LUT to '{arguments.out}' ({len(members)} members, {os.path.getsize(arguments.out)} bytes)")


if __name__ == "__main__":
	main()
