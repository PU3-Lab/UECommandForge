#!/usr/bin/env bash

uecf_sha256() {
  local path="$1"
  local hash_path="${path//\\//}"

  if command -v shasum >/dev/null 2>&1; then
    shasum -a 256 "${hash_path}" | awk '{ print $1 }'
    return 0
  fi

  if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "${hash_path}" | awk '{ print $1 }'
    return 0
  fi

  if command -v powershell.exe >/dev/null 2>&1; then
    local windows_path
    windows_path="$(cygpath -w "${hash_path}")"
    powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \
      '& { param([string]$Path) $ErrorActionPreference = "Stop"; (Get-FileHash -Algorithm SHA256 -LiteralPath $Path).Hash.ToLowerInvariant() }' \
      "${windows_path}" | tr -d '\r'
    return 0
  fi

  echo "[release-common] SHA256 tool not found. Install shasum, sha256sum, or PowerShell." >&2
  return 2
}

uecf_create_zip() {
  local zip_path="$1"
  shift

  if [ "$#" -eq 0 ]; then
    echo "[release-common] zip input list is empty" >&2
    return 2
  fi

  if command -v zip >/dev/null 2>&1; then
    zip -qr "${zip_path}" "$@"
    return 0
  fi

  if command -v powershell.exe >/dev/null 2>&1; then
    local windows_zip_path
    windows_zip_path="$(cygpath -w "${zip_path}")"
    local windows_root_path
    windows_root_path="$(cygpath -w "$(pwd)")"
    local windows_paths=()
    local path
    for path in "$@"; do
      windows_paths+=("$(cygpath -w "${path}")")
    done

    powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \
      '& {
        $ErrorActionPreference = "Stop"
        Add-Type -AssemblyName System.IO.Compression
        Add-Type -AssemblyName System.IO.Compression.FileSystem

        $Destination = $args[0]
        $Root = [System.IO.Path]::GetFullPath($args[1])
        if (-not $Root.EndsWith([System.IO.Path]::DirectorySeparatorChar)) {
          $Root = $Root + [System.IO.Path]::DirectorySeparatorChar
        }
        $Paths = @($args | Select-Object -Skip 2)
        if ($Paths.Count -eq 0) { throw "zip input list is empty" }
        if (Test-Path -LiteralPath $Destination) { Remove-Item -LiteralPath $Destination -Force }

        $Zip = [System.IO.Compression.ZipFile]::Open($Destination, [System.IO.Compression.ZipArchiveMode]::Create)
        try {
          foreach ($Path in $Paths) {
            $FullPath = [System.IO.Path]::GetFullPath($Path)
            if (Test-Path -LiteralPath $FullPath -PathType Container) {
              Get-ChildItem -LiteralPath $FullPath -Recurse -File | ForEach-Object {
                $EntryName = $_.FullName.Substring($Root.Length).Replace("\", "/")
                [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile($Zip, $_.FullName, $EntryName, [System.IO.Compression.CompressionLevel]::Optimal) | Out-Null
              }
            } elseif (Test-Path -LiteralPath $FullPath -PathType Leaf) {
              $EntryName = $FullPath.Substring($Root.Length).Replace("\", "/")
              [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile($Zip, $FullPath, $EntryName, [System.IO.Compression.CompressionLevel]::Optimal) | Out-Null
            } else {
              throw "zip input not found: $Path"
            }
          }
        } finally {
          $Zip.Dispose()
        }
      }' \
      "${windows_zip_path}" "${windows_root_path}" "${windows_paths[@]}"
    return 0
  fi

  echo "[release-common] zip tool not found. Install zip or run on Windows with PowerShell." >&2
  return 2
}
