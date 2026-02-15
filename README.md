# awg-manager-alpha

Minimal C web application for Entware/Keenetic (ARM64) with:

- local login page (landing + auth form),
- authentication against Keenetic `/auth` challenge-response API,
- local app session cookie,
- zero dynamic global state leaks,
- clean baseline for later `.ipk` packaging.

## Current stage

This is stage 1 + packaging baseline:

- project skeleton,
- secure HTTP server baseline,
- RCI auth handshake module,
- runtime auto-configuration (no credentials in config files),
- Entware init script (`S99awg-manager-alpha`),
- IPK template + build script,
- host unit tests and milestone backup tooling,
- Keenetic-style login redesign + post-login dashboard mock,
- automatic AmneziaWG kernel module load/unload from service lifecycle,
- dashboard kernel indicator (red/green LED).

Changelog:

`CHANGELOG.md`

## Build

```sh
make
```

Run unit tests:

```sh
make test
```

Build + run tests in one step:

```sh
make test && make
```

Result binary:

`build/awg-manager-alpha`

Optional binary compression:

```sh
upx --best --lzma build/awg-manager-alpha
```

## Build for router (aarch64) on Win11 + WSL

Install cross-toolchain once:

```sh
sudo apt update
sudo apt install -y build-essential binutils gcc-aarch64-linux-gnu \
  libc6-dev-arm64-cross file upx-ucl
```

Recommended (for best Entware compatibility): install Zig and let build script
produce static `aarch64-linux-musl` binary:

```sh
sudo snap install --beta zig --classic
export PATH=$PATH:/snap/bin
```

If you previously added foreign architecture and got `404 binary-arm64` errors,
revert it first:

```sh
sudo dpkg --remove-architecture arm64
sudo apt update
```

Then build + package in one command:

```sh
./scripts/build_aarch64_wsl.sh aarch64-3.10 <version>
```

This script builds ARM64 binary, verifies architecture, optionally compresses with UPX,
creates final `.ipk` in `dist/`, and fails fast if unit tests fail.

## Run

```sh
./build/awg-manager-alpha
```

For local run from project directory (without IPK install), use:

```sh
AWG_WEB_ROOT=./web ./build/awg-manager-alpha
```

By default:

- listen: `0.0.0.0:8088`
- router: detected from LAN bridge (`br0`, `br-lan`, `lan`, `bridge0`) and then
  from default gateway in `/proc/net/route` (fallback `192.168.1.1:80`)
- session ttl: `1800` seconds

## Runtime overrides

Optional environment variables:

- `AWG_LISTEN_ADDR`
- `AWG_LISTEN_PORT`
- `AWG_ROUTER_ADDR`
- `AWG_ROUTER_PORT`
- `AWG_WEB_ROOT` (default `/opt/share/awg-manager-alpha/www`)
- `AWG_MODULE_PATH` (default `/opt/lib/modules/amneziawg.ko`)
- `AWG_SESSION_TTL`

## Web resources

UI files are stored as regular resources in the project:

- `web/login.html`
- `web/app.html`
- `web/assets/auth.css`
- `web/assets/auth.js`
- `web/assets/app.css`
- `web/assets/app.js`

On router they are installed to:

`/opt/share/awg-manager-alpha/www`

## Kernel module lifecycle

This project expects kernel module file in project root:

`amneziawg.ko`

During package build/install it is deployed to:

`/opt/lib/modules/amneziawg.ko`

Init script behavior:

- on `start`: checks `/proc/modules`, loads module with `insmod` if not loaded;
- on `stop`: unloads module with `rmmod`;
- on `status`: reports both app process and kernel module state.

Dashboard behavior after login:

- top-right `AmneziaWG Kernel` indicator is green when module is loaded;
- red when module is not loaded;
- state is refreshed via session-protected endpoint `GET /api/kernel-status`.

## Entware service script

Service script path in repository:

`init.d/S99awg-manager-alpha`

Target path on device:

`/opt/etc/init.d/S99awg-manager-alpha`

Supported commands:

```sh
/opt/etc/init.d/S99awg-manager-alpha start
/opt/etc/init.d/S99awg-manager-alpha stop
/opt/etc/init.d/S99awg-manager-alpha restart
/opt/etc/init.d/S99awg-manager-alpha status
```

The daemon runs detached (`nohup`, stdin from `/dev/null`) and writes output to:

`/opt/var/log/awg-manager-alpha.log`

If `nohup` is missing on the router, script automatically falls back to background start.

`status` returns exit code `0` only when app is running and module is loaded.
Otherwise it returns `1`.

Optional runtime env file for service:

`/opt/etc/awg-manager-alpha.env`

Example:

```sh
AWG_LISTEN_PORT=18088
AWG_ROUTER_ADDR=192.168.10.1
AWG_ROUTER_PORT=80
AWG_WEB_ROOT=/opt/share/awg-manager-alpha/www
AWG_MODULE_PATH=/opt/lib/modules/amneziawg.ko
AWG_SESSION_TTL=1800
```

Credentials must not be stored in this file.

## IPK structure

Packaging template:

- `packaging/ipk/CONTROL/control.in`
- `packaging/ipk/CONTROL/postinst`
- `packaging/ipk/CONTROL/prerm`
- `scripts/build_ipk.sh`
- `web/` (deployed to `/opt/share/awg-manager-alpha/www`)
- `amneziawg.ko` (deployed to `/opt/lib/modules/amneziawg.ko`)

## Build .ipk

1) Build binary:

```sh
make clean
make
```

For router package, binary must be `aarch64`. Recommended path is:

```sh
./scripts/build_aarch64_wsl.sh aarch64-3.10 <version>
```

2) Optional UPX compression:

```sh
upx --best --lzma build/awg-manager-alpha
```

3) Build package (`version`, `arch` optional):

```sh
./scripts/build_ipk.sh <version> aarch64-3.10
```

`build_ipk.sh` validates that binary architecture matches package architecture.
Example: you cannot pack an `amd64` binary as `aarch64-3.10`.

For Entware compatibility, package is created in tar-gzip IPK layout:
`debian-binary`, `control.tar.gz`, `data.tar.gz`.

Output:

`dist/awg-manager-alpha_<version>_aarch64-3.10.ipk`

4) Install on router:

```sh
opkg install /path/to/awg-manager-alpha_<version>_aarch64-3.10.ipk
```

## Troubleshooting

- `Not downgrading package awg-manager ...`:
  you are installing a different package name/version over existing package.
  This project uses package name `awg-manager-alpha`.
- `Malformed package file`:
  rebuild package with `scripts/build_ipk.sh` from this repository (tar-gzip IPK layout).
- Login stuck on "Checking..." / "Проверка...":
  update to latest package and verify `AWG_ROUTER_ADDR`/`AWG_ROUTER_PORT`
  in `/opt/etc/awg-manager-alpha.env`.
- `insmod`/`rmmod` errors:
  verify module path `/opt/lib/modules/amneziawg.ko`, kernel compatibility,
  and permissions to load/unload modules.
- Service diagnostics:
  check `/opt/var/log/awg-manager-alpha.log` and run
  `/opt/etc/init.d/S99awg-manager-alpha status`.

## Create checkpoint backup

Create source/artifact backup archive after milestone:

```sh
./scripts/create_backup.sh milestone-01
```

Output archive:

`backups/awg-manager-alpha_milestone-01_YYYYMMDD-HHMMSS.tar.gz`

## Security notes

- Router credentials are never stored in files.
- Login password is processed in-memory only for current request.
- App uses random session tokens from `/dev/urandom`.
- Cookies are set with `HttpOnly; SameSite=Strict; Path=/`.
- HTTP parser enforces request and body limits.
