# Changelog

All notable changes to `awg-manager-alpha` are documented in this file.
The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

### Added
- Keenetic-style dashboard mock page after successful login (`/app`).
- External web resources directory:
  - `web/login.html`, `web/app.html`,
  - `web/assets/*.css`, `web/assets/*.js`.
- AmneziaWG module lifecycle management in service script:
  - load with `insmod` on start if missing,
  - unload with `rmmod` on stop,
  - module state in `status` output.
- Dashboard top-right kernel indicator:
  - label `AmneziaWG Kernel`,
  - green LED when loaded,
  - red LED when not loaded.
- API endpoint `GET /api/kernel-status` (session-protected).
- Auto-update worker script (`scripts/updater.sh`) with:
  - manifest check,
  - package download,
  - optional SHA-256 verification,
  - `opkg install` apply flow,
  - state/log files.
- Update API endpoints (session-protected):
  - `GET /api/update/status`,
  - `POST /api/update/check`,
  - `POST /api/update/apply`.
- Bottom-right dashboard update block:
  - current package version,
  - disabled update button when no updates,
  - highlighted active button when update is available.
- Public update manifest file:
  - `update/latest-aarch64-3.10.txt`.

### Changed
- Login page redesign aligned with Keenetic visual language:
  - dark-cyan palette,
  - split-screen authentication layout,
  - updated typography and form styling,
  - responsive behavior for desktop/mobile.
- Improved login request UX:
  - explicit button loading state,
  - clearer connectivity/auth feedback.
- Server now serves UI from filesystem resources instead of large inline C literals.
- IPK package now includes `amneziawg.ko` at `/opt/lib/modules/amneziawg.ko`.
- Main post-login interface simplified to two icon-based sections:
  - `Туннели`,
  - `Маршрутизация`,
  with hover labels and hash-based view switching.
- IPK package now includes updater runtime script at
  `/opt/libexec/awg-manager-alpha/updater.sh`.

## [0.1.0-6] - 2026-02-11

### Added
- Host unit tests (`make test`) for:
  - MD5/SHA-256 known vectors,
  - session lifecycle, expiry, and capacity,
  - runtime environment override handling.
- Checkpoint backup script `scripts/create_backup.sh`.

### Changed
- `build_aarch64_wsl.sh` now runs unit tests before cross-build/packaging.
- Init service output improved for start/stop/restart/status diagnostics.

### Fixed
- Hardened init script PID validation (checks process command line).
- Sanitized service env values for IP/port fields.
- Added client socket I/O timeouts to avoid long request stalls.
- Added frontend login fetch timeout/error handling to avoid endless "Checking..." state.
- Added missing output buffer size guard in session token creation.

## [0.1.0-5] - 2026-02-10

### Added
- Informative init script output:
  - app version,
  - PID,
  - web URL,
  - router auth target,
  - Keenetic name probe.

### Fixed
- Reduced service command delays (`start`/`stop`/`restart`).
- Improved graceful shutdown responsiveness.

## [0.1.0-4] - 2026-02-10

### Fixed
- Service start without `nohup` installed (Entware compatibility fallback).
- Router target auto-detection now prioritizes LAN bridge interfaces (`br0`, `br-lan`, `lan`, `bridge0`).
- Added router auth network timeouts to prevent hanging login.

## [0.1.0-3] - 2026-02-10

### Changed
- Package renamed to avoid conflicts with existing `awg-manager` installs:
  - package: `awg-manager-alpha`,
  - binary: `/opt/bin/awg-manager-alpha`,
  - service: `/opt/etc/init.d/S99awg-manager-alpha`.

## [0.1.0-2] - 2026-02-10

### Fixed
- IPK packaging switched to Entware-compatible tar-gzip IPK layout:
  - `debian-binary`,
  - `control.tar.gz`,
  - `data.tar.gz`.

## [0.1.0-1] - 2026-02-10

### Added
- Initial alpha baseline:
  - C web server,
  - landing/login UI,
  - Keenetic RCI auth challenge-response,
  - in-memory session management,
  - init script and IPK packaging scaffolding.

## Milestone Checkpoints

- `backups/awg-manager-alpha_milestone-01_20260210-235048.tar.gz`
- `backups/awg-manager-alpha_milestone-02-code-audit_20260210-235207.tar.gz`
- `backups/awg-manager-alpha_milestone-03-tests_20260211-000810.tar.gz`
- `backups/awg-manager-alpha_milestone-04-docs_20260211-002505.tar.gz`
- `backups/awg-manager-alpha_milestone-05-changelog_20260211-002704.tar.gz`
- `backups/awg-manager-alpha_milestone-06-changelog-format_20260211-002831.tar.gz`
- `backups/awg-manager-alpha_milestone-07-ui-refresh_20260211-004006.tar.gz`
- `backups/awg-manager-alpha_milestone-08-build-warning-fix_20260211-004320.tar.gz`
- `backups/awg-manager-alpha_milestone-09-web-resources_20260211-005655.tar.gz`
- `backups/awg-manager-alpha_milestone-10-kernel-module-ui_20260211-011811.tar.gz`
- `backups/awg-manager-alpha_milestone-11-kernel-polish_20260211-011955.tar.gz`
- `backups/awg-manager-alpha_milestone-12-auto-update_20260215-235619.tar.gz`
