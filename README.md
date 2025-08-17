# libdnf-plugin-filewatcher

**libdnf5 plugin** that watches specific files/paths during DNF5 transactions and, if any watched item is modified after a package update, **optionally overwrites/restores** it according to your policy (e.g., from a golden copy).  
Typical use-case: keep opinionated local files (branding, templates, unit files, configs) immutable across package updates without forking the upstream RPM.

> This plugin integrates at the **libdnf5** layer. It is discovered via the standard libdnf5 plugin mechanism and configured from `/etc/dnf/libdnf5-plugins/`. Default plugin/config search paths and overrides are the same as other libdnf5 plugins.

---

## Table of contents

- [How it works](#how-it-works)  
- [Requirements](#requirements)  
- [Build & install](#build--install)  
- [Configuration](#configuration)  
  - [Quick start](#quick-start)  
  - [Configuration reference](#configuration-reference)  
  - [Example: lock down systemd units](#example-lock-down-systemd-units)  
- [Usage](#usage)  
- [Logging](#logging)  
- [Troubleshooting](#troubleshooting)  
- [FAQ](#faq)  
- [Development](#development)  
- [Roadmap](#roadmap)  
- [License](#license)

---

## How it works

- The plugin is loaded by libdnf5 when DNF5 starts.  
- During a transaction that **updates or installs packages**, the plugin watches configured files/directories.  
- If a watched file changes after the package update completes (or if the RPM has replaced it), the plugin can:
  - **restore/overwrite** the file from a configured **source** (e.g., a golden copy directory), or
  - **just report** the change (dry-run/alert mode).  
- Behavior is driven entirely by `filewatcher.conf`.

---

## Requirements

- A distro running **DNF5 + libdnf5** (e.g., recent Fedora or RHEL derivatives where DNF5 is available).  
- Toolchain: **CMake ≥ 3.16**, **g++ (C++17)**.  
- libdnf5 development headers & libraries (e.g., `libdnf5-devel`), plus standard build dependencies.

---

## Build & install

You can use the included `make.sh` or run the following:

```bash
# 1) Clone
git clone https://github.com/polaz/libdnf-plugin-filewatcher.git
cd libdnf-plugin-filewatcher

# 2) Configure & build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

# 3) Install (defaults to /usr/local; set CMAKE_INSTALL_PREFIX if needed)
sudo cmake --install build
```

The plugin binary is installed into the libdir’s libdnf5 plugins folder (e.g., `/usr/lib64/libdnf5/plugins/`).

> **Testing without installing system-wide**  
> Create a temporary `dnf5.conf` that points to a staging plugins directory via `pluginpath` and a staging config via `pluginconfpath`:
>
> ```ini
> # /tmp/dnf5-test.conf
> [main]
> plugins=true
> pluginconfpath=/tmp/libdnf5-plugins
> pluginpath=/home/$USER/libdnf-plugin-filewatcher/build  # or wherever the .so lands
> ```
>
> Then run:
>
> ```bash
> sudo dnf5 -c /tmp/dnf5-test.conf check-update
> ```

---

## Configuration

### Quick start

1. Copy the sample config:
   ```bash
   sudo install -d /etc/dnf/libdnf5-plugins/
   sudo cp filewatcher.conf.dist /etc/dnf/libdnf5-plugins/filewatcher.conf
   sudo $EDITOR /etc/dnf/libdnf5-plugins/filewatcher.conf
   ```

2. Ensure DNF5 plugins are enabled (they are by default).

3. Run any DNF5 operation that updates packages; watch logs for filewatcher messages.

### Configuration reference

> ⚠️ The **authoritative list** of options is in `filewatcher.conf.dist` shipped with this repo.  
> Below is a practical reference to help you get started:

```ini
# /etc/dnf/libdnf5-plugins/filewatcher.conf
[main]
# Enable/disable the plugin globally
enabled = true

# One or more watch entries. Each entry can be a file or directory.
# Globs are allowed.
watch = /etc/myapp/*
watch = /usr/lib/systemd/system/*.service

# Directory that holds your "golden copies" used to overwrite changes.
# Use absolute path. The plugin will map relative structure from here.
source_dir = /var/lib/filewatcher/source

# Overwrite policy: none|changed|always
#  - none:   report only
#  - changed: if target differs from the source copy, overwrite it
#  - always: overwrite on every detected event after transaction
overwrite = changed

# Exclude patterns (optional, can be repeated)
exclude = *.rpmnew
exclude = *.rpmsave

# Logging verbosity: error|warn|info|debug
log_level = info

# Safety valve: if true, do not overwrite files that belong to a different package
# than the one being updated in the current transaction (best effort).
package_scope_safe = true
```

> Notes
> - Put your canonical files under `source_dir` preserving the same relative path as the target.  
>   Example: if you watch `/usr/lib/systemd/system/my.service`, place the canonical file at  
>   `/var/lib/filewatcher/source/usr/lib/systemd/system/my.service`.
> - When `overwrite=changed`, the plugin compares the target to the canonical file and overwrites only on difference.

### Example: lock down systemd units

Keep specific unit files identical to your golden copies across updates:

```ini
[main]
enabled = true
watch = /usr/lib/systemd/system/myapp*.service
source_dir = /var/lib/filewatcher/source
overwrite = changed
exclude = *.rpmnew
log_level = info
```

Place canonical files at:
```
/var/lib/filewatcher/source/usr/lib/systemd/system/myapp.service
/var/lib/filewatcher/source/usr/lib/systemd/system/myapp-worker.service
```

---

## Usage

Once configured, **no extra command** is needed. Just run `dnf5 upgrade`, `dnf5 distro-sync`, etc. The plugin executes automatically as part of the transaction lifecycle.  
You’ll see log lines confirming what was detected and whether an overwrite happened.

---

## Logging

- Messages go to DNF5/libdnf5 logging (journal and/or terminal depending on your environment).  
- Increase verbosity via `log_level = debug` in `filewatcher.conf` (plugin-side) and the usual DNF5 verbosity flags if needed.  
- Useful DNF5 knobs: `-v`, `-d <level>`.

---

## Troubleshooting

Below is a systematic approach you can follow in ops.

### 1) Potential sources (5–7)

1. **Plugin not loaded** (wrong plugin dir, plugins disabled).  
2. **Config not found** (wrong filename or wrong config directory).  
3. **No matching watch entries** (glob doesn’t match, path typo, permissions).  
4. **Missing golden copy** (file absent under `source_dir`).  
5. **Policy prevents overwrite** (`overwrite=none`, excludes, or `package_scope_safe=true` blocks it).  
6. **SELinux denies write** to target path.  
7. **Transaction didn’t actually update the file** (no file change occurred).

### 2) Extended checklist

- Confirm plugin discovery:
  - Config dir `/etc/dnf/libdnf5-plugins/`
  - Plugin dir `/usr/lib64/libdnf5/plugins/`
  - Any overrides in your `dnf5.conf` (`pluginconfpath`, `pluginpath`)
- Run `dnf5 -v check-update` and watch for filewatcher lines.
- Validate config syntax and **exact** filename: `/etc/dnf/libdnf5-plugins/filewatcher.conf`.
- Verify your globs: `printf '%s\n' /usr/lib/systemd/system/myapp*.service` should list targets.  
- Ensure canonical file exists under `source_dir` with the same relative path.
- Temporarily set `log_level=debug`.
- Test with a harmless file to observe behavior (e.g., a test file in `/etc/myapp/`).

### 3) Narrow down to 1–2 likely causes

In practice, the most common issues are:
- **Config path/filename** not correct (not under `/etc/dnf/libdnf5-plugins/` or name mismatch), or  
- **Missing `source_dir` file** (nothing to overwrite with).

### 4) What to log/collect

- `sudo dnf5 -v upgrade |& tee /tmp/dnf5-filewatcher.log` (includes plugin messages).  
- `tree /etc/dnf/libdnf5-plugins/` and `cat /etc/dnf/libdnf5-plugins/filewatcher.conf`.  
- `tree /var/lib/filewatcher/source/` (or your chosen `source_dir`).  
- `ls -lZ <target-file>` and any AVCs from `journalctl -t setroubleshoot --since -2h` (SELinux).  
- `dnf5 config-manager --dump` (to capture `pluginpath`/`pluginconfpath` if customized).

If you still get stuck, include the above logs when opening an issue.

---

## FAQ

**Q: Where does the plugin look for configs and the binary by default?**  
A: Config in `/etc/dnf/libdnf5-plugins/`, binary in `/usr/lib64/libdnf5/plugins/`. Both can be changed in `dnf5.conf` via `pluginconfpath`/`pluginpath`.

**Q: Is this a DNF5 command plugin?**  
A: No, this is a **libdnf5** plugin (lives beneath the DNF5 CLI). It doesn’t add new CLI subcommands; it reacts to transaction hooks.

**Q: How is this different from a generic “actions” plugin?**  
A: “Actions” plugins let you run external scripts on hooks. **filewatcher** focuses on watching/guarding real files with an overwrite policy—no need to maintain hook scripts.

---

## Development

- Code is C++ (single plugin `.so`) built via CMake.  
- Start with a Debug build: `-DCMAKE_BUILD_TYPE=Debug` and set `log_level=debug`.  
- For quick experiments, point `pluginpath` and `pluginconfpath` at your build tree with a temp `dnf5.conf`.

---

## Roadmap

- SHA256 comparison mode to avoid needless overwrites.  
- Per-watch entry policies (different `overwrite` and `source_dir` per entry).  
- Optional post-overwrite handler (e.g., `systemctl daemon-reload` for unit files).  
- SELinux-friendly relabel after overwrite.

---

## License

See repository license headers (if present). Upstream libdnf5 is LGPLv2+.

---

## Acknowledgments

- Inspired by common workflows around guarding local modifications during RPM updates.  
- Thanks to the DNF5/libdnf5 documentation and existing plugin packaging for clear examples of plugin and config paths.
