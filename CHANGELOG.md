# Changelog

## 0.9.0 - 2026-06-12

### Added

- Multi-agent support with profile-based installations (`--codex`, `--claude`, `--antigravity`).
- Support for installing to alternative LLM agent home paths (using `AGENT_HOME` environment variable).

### Changed

- Renamed codex references to agent references in tools packaging.
- Updated release, install, and uninstall scripts to support multiple agents concurrently.

## 0.8.0 - 2026-05-28

### Added

- Phase 8 release packaging for plugin, tools, and source packages.
- Local install, update, and uninstall scripts for project plugin files and Codex-side tools/specs.
- Release package install, update, uninstall, source package, tools package, and Windows wrapper validation smoke tests.
- Windows host limitation report for `.bat` wrapper validation when running on non-Windows hosts.
- README install/update/uninstall guide and Phase 8 release install eval report.

### Changed

- Release package verification now requires strict manifest/file-list/checksum agreement and fail-closed external checksum matching.
- Source package output excludes generated build output and internal planning/memory docs.
- Phase 8 release version promoted to `0.8.0`.

### Known Limits

- Windows `.bat` wrapper execution still requires validation on a Windows host.
- UE commandlet post-install validation requires a local Unreal Engine execution environment.
