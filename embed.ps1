# --- Discovery ----------------------------------------------------------------
$src_dir = [System.IO.Path]::Combine($PSScriptRoot, "src")
$manifests = [System.IO.Directory]::GetFiles($src_dir, "*.embed", [System.IO.SearchOption]::AllDirectories)

# --- Process Manifests --------------------------------------------------------
foreach ($manifest in $manifests) {
	$manifest_dir  = [System.IO.Path]::GetDirectoryName($manifest)
	$manifest_name = [System.IO.Path]::GetFileNameWithoutExtension($manifest)
	$gen_dir       = [System.IO.Path]::Combine($manifest_dir, "generated")
	$out_path      = [System.IO.Path]::Combine($gen_dir, "$manifest_name.embed.hpp")

	[System.IO.Directory]::CreateDirectory($gen_dir) | Out-Null

	$stream = [System.IO.File]::Create($out_path)
	$writer = [System.IO.StreamWriter]::new($stream, [System.Text.Encoding]::ASCII, 65536)

	# --- File Header ----------------------------------------------------------
	$writer.WriteLine("// Copyright (C) 2026 Koh Swee Teck Dedrick.")
	$writer.WriteLine("// Licensed under the MIT license (https://opensource.org/license/mit/).`n")
	$writer.WriteLine("//~ GENERATED CODE`n")
	$manifest_file_name = "$manifest_name.embed.hpp"
	$guard = ($manifest_file_name.ToUpper() -replace '\.', '_') + '__'
	$writer.WriteLine("#ifndef $guard")
	$writer.WriteLine("#define $guard`n")
	$writer.WriteLine("namespace dk {")

	# --- Parse Directives & Emit ----------------------------------------------
	$lines = [System.IO.File]::ReadAllLines($manifest)
	foreach ($line in $lines) {
		if ($line -match '^\s*@embed_file\s+([a-zA-Z0-9_]+)\s*:\s*"([^"]+)"') {
			$identifier    = $Matches[1]
			$relative_path = $Matches[2]
			$absolute_path = [System.IO.Path]::GetFullPath([System.IO.Path]::Combine($manifest_dir, $relative_path))

			$bytes = [System.IO.File]::ReadAllBytes($absolute_path)
			$len   = $bytes.Length

			$writer.WriteLine("u8 const ${identifier}__data[] = {")
			for ($i = 0; $i -lt $len; $i += 64) {
				$chunk = [Math]::Min(64, $len - $i)
				$hex   = [System.BitConverter]::ToString($bytes, $i, $chunk).Replace('-', ', 0x')
				if ($i + $chunk -lt $len) {
					$writer.WriteLine("  0x$hex,")
				} else {
					$writer.WriteLine("  0x$hex")
				}
			}
			$writer.WriteLine("};")
			$writer.WriteLine("String8 const ${identifier} = { ${identifier}__data, sizeof(${identifier}__data) };`n")
		}
	}

	$writer.WriteLine("}")
	$writer.WriteLine("`n#endif // $guard")
	$writer.Dispose()
	$stream.Dispose()
}
