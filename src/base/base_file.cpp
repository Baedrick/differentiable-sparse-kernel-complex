// Copyright (C) 2026 Koh Swee Teck Dedrick.
// Licensed under the MIT license (https://opensource.org/license/mit/).

auto dk::operator==(File a, File b) noexcept -> b8 {
	return a.v == b.v;
}

auto dk::operator!=(File a, File b) noexcept -> b8 {
	return !(a == b);
}

auto dk::is_valid(File file) noexcept -> b8 {
	return file != File{};
}

auto dk::read_bytes_from_file_path(Arena *arena, String8 path) noexcept -> String8 {
	File const file = file_open(path, FileAccessFlag_Read | FileAccessFlag_ShareRead);
	FileAttributes const attr = attributes_from_file(file);
	String8 result = {};
	result.size = attr.size;
	result.data = arena_push_array<u8>(arena, result.size);
	u64 const actual_read_size = file_read(file, 0, result.size, const_cast<u8 *>(result.data));
	if (actual_read_size < result.size) {
		arena_pop(arena, result.size - actual_read_size);
		result.size = actual_read_size;
	}
	file_close(file);
	return result;
}

auto dk::write_bytes_to_file_path(String8 path, String8 bytes) noexcept -> b8 {
	b8 good = false;
	File const file = file_open(path, FileAccessFlag_Write);
	if (is_valid(file)) {
		u64 const bytes_written = file_write(file, 0, bytes.size, bytes.data);
		good = bytes_written == bytes.size;
		file_close(file);
	}
	return good;
}

auto dk::write_bytes_list_to_file_path(String8 path, String8List const *list) noexcept -> b8 {
	b8 good = false;
	File const file = file_open(path, FileAccessFlag_Write);
	if (is_valid(file)) {
		dk_defer(file_close(file));
		TempArena const scratch = scratch_begin(nullptr, 0);
		dk_defer(scratch_end(scratch));
		RingBuffer write_buffer = {};
		write_buffer.size = kilo_bytes(64);
		write_buffer.base = arena_push_array<u8>(scratch.arena, write_buffer.size);
		good = true;
		u64 file_offset = 0;
		for (String8Node const *node = list->first; node != nullptr; node = node->next) {
			for (u64 node_offset = 0; node_offset < node->string.size;) {
				u64 const unconsumed_size = write_buffer.write_pos - write_buffer.read_pos;
				u64 const available_size = write_buffer.size - unconsumed_size;
				if (available_size == 0) {
					u64 const bytes_written = file_write(file, file_offset, file_offset + write_buffer.size, write_buffer.base);
					if (bytes_written != write_buffer.size) {
						good = false;
						break;
					}
					file_offset += write_buffer.size;
					write_buffer.read_pos += write_buffer.size;
				}
				else {
					u64 const bytes_to_copy = min(available_size, node->string.size - node_offset);
					ring_write(&write_buffer, node->string.data + node_offset, bytes_to_copy);
					node_offset += bytes_to_copy;
				}
			}
		}
		if (good && write_buffer.write_pos > write_buffer.read_pos) {
			u64 const unconsumed_size = write_buffer.write_pos - write_buffer.read_pos;
			u64 const bytes_written = file_write(file, file_offset, file_offset + unconsumed_size, write_buffer.base);
			file_offset += bytes_written;
		}
		good = file_offset == list->total_size;
	}
	return good;
}

auto dk::append_bytes_to_file_path(String8 path, String8 bytes) noexcept -> b8 {
	b8 good = false;
	if (bytes.size != 0) {
		File const file = file_open(path, FileAccessFlag_Append);
		if (is_valid(file)) {
			u64 const file_pos = attributes_from_file(file).size;
			u64 const bytes_written = file_write(file, file_pos, file_pos + bytes.size, bytes.data);
			good = bytes_written == bytes.size;
			file_close(file);
		}
	}
	return good;
}
