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

uecf_is_windows_bash() {
  case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) return 0 ;;
    *) return 1 ;;
  esac
}

uecf_require_windows_reparse_tools() {
  if ! command -v cygpath >/dev/null 2>&1 || ! command -v powershell.exe >/dev/null 2>&1; then
    echo "[release-common] Windows reparse-point checks require cygpath and powershell.exe" >&2
    return 2
  fi
}

uecf_to_windows_path() {
  local path="$1"
  local windows_path

  if windows_path="$(cygpath -w -a "${path}" 2>/dev/null)"; then
    printf '%s\n' "${windows_path}"
    return 0
  fi

  case "${path}" in
    /[A-Za-z]/*)
      windows_path="${path:1:1}:/${path:3}"
      windows_path="${windows_path//\//\\}"
      ;;
    [A-Za-z]:/*|[A-Za-z]:\\*)
      windows_path="${path//\//\\}"
      ;;
    *)
      local windows_pwd
      windows_pwd="$(pwd -W)" || return 2
      windows_path="${windows_pwd}\\${path//\//\\}"
      ;;
  esac

  powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \
    '& { param([string]$Path) [System.IO.Path]::GetFullPath($Path) }' \
    "${windows_path}" | tr -d '\r'
}

uecf_path_is_reparse_point() {
  local path="$1"

  if [ -L "${path}" ]; then
    return 0
  fi

  if ! uecf_is_windows_bash; then
    return 1
  fi

  uecf_require_windows_reparse_tools || return 2

  local windows_path
  windows_path="$(uecf_to_windows_path "${path}")" || return 2
  powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \
    '& {
      param([string]$Path)
      $ErrorActionPreference = "Stop"
      try {
        $Item = Get-Item -LiteralPath $Path -Force -ErrorAction Stop
      } catch [System.Management.Automation.ItemNotFoundException] {
        exit 1
      } catch {
        Write-Error $_
        exit 2
      }
      if (($Item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) { exit 0 }
      exit 1
    }' \
    "${windows_path}" >/dev/null 2>&1
}

uecf_reject_link_path() {
  local path="$1"
  local owner="$2"

  if uecf_path_is_reparse_point "${path}"; then
    echo "[${owner}] refusing to operate on symlink path: ${path}" >&2
    return 2
  else
    local reparse_status=$?
    if [ "${reparse_status}" -eq 2 ]; then
      return 2
    fi
  fi
  return 0
}

uecf_reject_link_ancestors() {
  local path="$1"
  local owner="$2"
  local original_pwd
  original_pwd="$(pwd)"

  uecf_reject_link_path "${path}" "${owner}" || return 2

  local current="${path}"
  while [ -n "${current}" ] && [ "${current}" != "." ] && [ "${current}" != "/" ]; do
    if [ -L "${current}" ]; then
      echo "[${owner}] refusing to operate below symlink path: ${path}" >&2
      return 2
    fi
    current="$(dirname "${current}")"
  done

  if [ "${path#/}" = "${path}" ]; then
    current="${original_pwd}"
    while [ -n "${current}" ] && [ "${current}" != "." ] && [ "${current}" != "/" ]; do
      if [ -L "${current}" ]; then
        echo "[${owner}] refusing to operate below symlink path: ${path}" >&2
        return 2
      fi
      current="$(dirname "${current}")"
    done
  fi

  if uecf_is_windows_bash; then
    uecf_require_windows_reparse_tools || return 2

    local windows_path
    windows_path="$(uecf_to_windows_path "${path}")" || return 2
    if powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \
      '& {
        param([string]$Path)
        $ErrorActionPreference = "Stop"
        $Current = [System.IO.Path]::GetFullPath($Path)

        while (-not (Test-Path -LiteralPath $Current)) {
          $Parent = [System.IO.Directory]::GetParent($Current)
          if ($null -eq $Parent) { exit 0 }
          $Current = $Parent.FullName
        }

        while ($null -ne $Current) {
          $Item = Get-Item -LiteralPath $Current -Force -ErrorAction Stop
          if (($Item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
            Write-Error ("reparse point is not allowed: " + $Item.FullName)
            exit 2
          }
          $Parent = [System.IO.Directory]::GetParent($Current)
          if ($null -eq $Parent) { break }
          $Current = $Parent.FullName
        }
        exit 0
      }' \
      "${windows_path}" >/dev/null; then
      return 0
    fi

    echo "[${owner}] refusing to operate below symlink path: ${path}" >&2
    return 2
  fi
  return 0
}

uecf_reject_link_tree() {
  local root="$1"
  local owner="$2"

  if [ ! -e "${root}" ]; then
    return 0
  fi

  local first_symlink
  if ! first_symlink="$(find "${root}" -type l -print -quit)"; then
    echo "[${owner}] could not inspect release package tree: ${root}" >&2
    return 2
  fi

  if [ -n "${first_symlink}" ]; then
    echo "[${owner}] symlinks are not allowed in release packages: ${root}" >&2
    return 2
  fi

  if ! uecf_is_windows_bash; then
    return 0
  fi

  uecf_require_windows_reparse_tools || return 2

  local windows_root
  windows_root="$(uecf_to_windows_path "${root}")" || return 2
  if powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \
    '& {
      param([string]$Root)
      $ErrorActionPreference = "Stop"
      $RootItem = Get-Item -LiteralPath $Root -Force
      if (($RootItem.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
        Write-Error ("reparse point is not allowed: " + $RootItem.FullName)
        exit 2
      }
      $ReparseItem = Get-ChildItem -LiteralPath $Root -Recurse -Force -Attributes ReparsePoint |
        Select-Object -First 1
      if ($null -ne $ReparseItem) {
        Write-Error ("reparse point is not allowed: " + $ReparseItem.FullName)
        exit 2
      }
      exit 0
    }' \
    "${windows_root}" >/dev/null; then
    return 0
  fi

  echo "[${owner}] symlinks are not allowed in release packages: ${root}" >&2
  return 2
}

uecf_reject_output_file_path() {
  local path="$1"
  local owner="$2"

  uecf_reject_link_ancestors "${path}" "${owner}" || return 2

  if [ ! -e "${path}" ]; then
    return 0
  fi

  if [ -d "${path}" ]; then
    uecf_reject_link_tree "${path}" "${owner}" || return 2
    echo "[${owner}] output file path is a directory: ${path}" >&2
    return 2
  fi

  if [ ! -f "${path}" ]; then
    echo "[${owner}] output file path is not a regular file: ${path}" >&2
    return 2
  fi

  return 0
}

uecf_write_file_from_stdin() {
  local output_path="$1"
  local owner="$2"
  local output_dir
  local temp_file

  output_dir="$(dirname "${output_path}")"
  uecf_reject_link_ancestors "${output_dir}" "${owner}" || return 2
  uecf_reject_output_file_path "${output_path}" "${owner}" || return 2
  temp_file="$(mktemp "${output_dir}/.$(basename "${output_path}").XXXXXX")"
  if ! cat > "${temp_file}"; then
    rm -f "${temp_file}"
    return 2
  fi
  mv "${temp_file}" "${output_path}"
}

uecf_append_file_from_stdin() {
  local output_path="$1"
  local owner="$2"
  local output_dir
  local temp_file

  output_dir="$(dirname "${output_path}")"
  uecf_reject_link_ancestors "${output_dir}" "${owner}" || return 2
  uecf_reject_output_file_path "${output_path}" "${owner}" || return 2
  temp_file="$(mktemp "${output_dir}/.$(basename "${output_path}").XXXXXX")"
  if [ -f "${output_path}" ] && ! cat "${output_path}" > "${temp_file}"; then
    rm -f "${temp_file}"
    return 2
  fi
  if ! cat >> "${temp_file}"; then
    rm -f "${temp_file}"
    return 2
  fi
  mv "${temp_file}" "${output_path}"
}

uecf_write_checksums_file() {
  local zip_path="$1"
  local checksums_path="$2"
  local owner="$3"

  "$(dirname "${BASH_SOURCE[0]}")/write_checksums.sh" "${zip_path}" \
    | uecf_write_file_from_stdin "${checksums_path}" "${owner}"
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
