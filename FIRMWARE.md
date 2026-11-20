# Firmware Release Process (Copilot Workflow)

**⚠️ IMPORTANT: This is THE ONLY authoritative source for firmware releases of the web installer. Follow these steps exactly.**

This is an automated workflow guide for GitHub Copilot / Claude Code to create firmware releases for the
FOSSA web installer (`installer/`).

## ⚡ Quick Start - DO THIS FIRST

**ALWAYS start by getting the current Bitcoin block height:**
```powershell
Invoke-WebRequest -Uri "https://mempool.space/api/blocks/tip/height" -UseBasicParsing | Select-Object -ExpandProperty Content
```

This becomes your version number: `vBLOCKHEIGHT` (e.g., v976739)

## 📋 Release Strategy

- **Hardware variant:** WT32-SC01 / ESP32 with ST7796 display, portrait (rotation 2).
  PlatformIO env: `installer-ST7796-rotated` (extends `fossa-ST7796-rotated`).
- **Version format:** `vBLOCKHEIGHT` – no suffix (only one variant so far).
  If more display variants are added later, give each a suffix (e.g. `vBLOCKHEIGHTl` for landscape) and
  document it here, in the same way as ZapBox does.
- **Binary files:** Copied WITHOUT any suffix: `bootloader.bin`, `partitions.bin`, `firmware.bin`, plus `boot_app0.bin`.
- **Installer build vs. hardcoded build:** The installer build sets `-DINSTALLER_BUILD=1`, which forces
  `HARDCODED=false` in `fossa.ino`. The settings are then read from `/elements.json` (written by the web installer
  over USB serial). The `fossa-*` environments keep using `hardcoded_user_config.h`. **Never release a firmware that
  was built from a `fossa-*` env – it would contain your personal device string!**

## Automated Release Steps

### 1. Update Version in platformio.ini

In the `[env:installer-ST7796-rotated]` section update the version flag with the block height from above:
```ini
-DFIRMWARE_VERSION=\"v976739\"
```

### 2. Create Firmware Directory

```powershell
mkdir installer/firmware/v976739
```

### 3. Create manifest.json

`installer/firmware/v976739/manifest.json`:
```json
{
  "name": "Fossa ATM",
  "version": "v976739",
  "new_install_prompt_erase": true,
  "builds": [
    {
      "chipFamily": "ESP32",
      "parts": [
        { "path": "bootloader.bin", "offset": 4096 },
        { "path": "partitions.bin", "offset": 32768 },
        { "path": "boot_app0.bin", "offset": 57344 },
        { "path": "firmware.bin", "offset": 65536 }
      ]
    }
  ]
}
```

**⚠️ Offsets (decimal) for classic ESP32:** bootloader `0x1000` = 4096, partitions `0x8000` = 32768,
boot_app0 `0xE000` = 57344, firmware `0x10000` = 65536. `chipFamily` must be `ESP32` (not ESP32-S3 / C3).

### 4. Update Web Installer (`installer/index.html`)

Add the new version **at the top** of the `<select id="firmware-version">` list and mark it as latest
(remove "Latest" from the previous entry). Also update the `manifest="./firmware/vXXXXXX/manifest.json?_t=0"`
attribute of `<esp-web-install-button id="flash-button">` to the new version.

```html
<option value="./firmware/v976739/manifest.json">v976739 (Latest – short description)</option>
```

### 5. Compile Firmware (DO NOT SKIP!)

```powershell
C:\Users\Datenrettung\.platformio\penv\Scripts\platformio.exe run -e installer-ST7796-rotated
```

### 6. Copy Binary Files

**⚠️ CRITICAL: Copy files WITHOUT any suffix!**

```powershell
$src = ".pio\build\installer-ST7796-rotated"
$dst = "installer\firmware\v976739"
Copy-Item "$src\bootloader.bin" "$dst\bootloader.bin"
Copy-Item "$src\partitions.bin" "$dst\partitions.bin"
Copy-Item "$src\firmware.bin"   "$dst\firmware.bin"
Copy-Item "$env:USERPROFILE\.platformio\packages\framework-arduinoespressif32\tools\partitions\boot_app0.bin" "$dst\boot_app0.bin"
```

### 7. Generate Release Description

Use `git log` to get commits since last release:
```powershell
git log --oneline v976739..HEAD
```

Create a **SHORT and CONCISE** summary of all changes since the last firmware release, in **ENGLISH**:
- Group by type: Features, Bug Fixes, Technical
- 1–2 lines per change, focus on user-visible improvements

```markdown
## Changes Since v976739

### Features
- ...

### Bug Fixes
- ...
```

### 8. Git Commit

```bash
git add platformio.ini installer/ FIRMWARE.md
git commit -m "Release v976739: <short description in English>"
```

### 9. Inform User

Tell user:
- Which version was prepared
- Show brief changelog in English
- Next steps: Test with a real device (flash → connect → write config → restart) → Tag → Push → publish

## Quick Checklist

**⚠️ Follow in this exact order:**

- [ ] **STEP 0:** Get Bitcoin block height (ALWAYS DO THIS FIRST!)
- [ ] Update `platformio.ini` (`FIRMWARE_VERSION` in `[env:installer-ST7796-rotated]`)
- [ ] Create `installer/firmware/vBLOCKHEIGHT/`
- [ ] Create `manifest.json`
- [ ] Update `installer/index.html` (new `<option>` on top + `manifest` attribute of the flash button)
- [ ] **Compile firmware** (`pio run -e installer-ST7796-rotated`) — **DO NOT SKIP!**
- [ ] **Copy all 4 binary files** (bootloader, partitions, firmware, boot_app0) — **DO NOT SKIP!**
- [ ] Verify the firmware folder contains 5 files: `manifest.json` + 4 `.bin` files
- [ ] If the config protocol or the config fields changed: update `installer/assets/installer.js` and `installer/index.html` (see below)
- [ ] Generate release description from git log (in English)
- [ ] Git commit (DO NOT push yet)
- [ ] Inform user with English release notes

## Changing the configuration fields or serial protocol

The web installer and the firmware must stay in sync:

| Firmware | Web installer |
|---|---|
| `readFiles()` in `fossa/100_config.ino` reads `config_*` keys from `/elements.json` | `FIELDS` list + form inputs in `installer/assets/installer.js` / `installer/index.html` |
| `executeCommand()` in `fossa/100_config.ino` (`/ping`, `/file-remove`, `/file-append`, `/file-read`, `/config-done`) and the `/config` command in `fossa/fossa.ino` | `installer/assets/installer.js` (top of file documents the protocol) |
| `convertToFloatArray()` buffer = 50 bytes | `checkNumberList()` limits lists to 49 characters |

Changing a field name or command means a **new firmware release** (older firmware in the version dropdown keeps
the old behaviour, so keep the installer backwards compatible or drop old versions from the list).

## Common Mistakes to Avoid

1. ❌ **DON'T** skip compiling the firmware — the folder MUST contain all `.bin` files in addition to `manifest.json`
2. ❌ **DON'T** release a binary built from a `fossa-*` env (contains hardcoded device string) — only `installer-*`
3. ❌ **DON'T** forget `boot_app0.bin` (offset 0xE000) in the folder and in the manifest
4. ❌ **DON'T** use a chipFamily other than `ESP32` / the wrong bootloader offset (ESP32 = 4096, not 0)
5. ❌ **DON'T** add suffixes to binary filenames
6. ❌ **DON'T** forget to update the `<option>` list and the flash button `manifest` attribute in `installer/index.html`
7. ❌ **DON'T** commit `fossa/hardcoded_user_config.h` with your personal device string

## Binary File Locations

After `pio run -e installer-ST7796-rotated`:
```
.pio/build/installer-ST7796-rotated/
├── bootloader.bin
├── partitions.bin
└── firmware.bin
# boot_app0.bin: %USERPROFILE%\.platformio\packages\framework-arduinoespressif32\tools\partitions\boot_app0.bin
```

## Final Release Structure

```
installer/
├── index.html
├── assets/
│   ├── installer.js
│   ├── serial.js
│   └── style.css
└── firmware/
    └── v976739/
        ├── bootloader.bin
        ├── partitions.bin
        ├── boot_app0.bin
        ├── firmware.bin
        └── manifest.json
```

## Local testing of the installer page

Web Serial and esp-web-tools need `https://` or `http://localhost`:
```powershell
cd installer
python -m http.server 8080   # or: npx serve .
# open http://localhost:8080 in Chrome / Edge
```

## Release History

### v976739 — 2026-11-20
- First web-installer release: flash via esp-web-tools, configure all settings over USB serial.
- Config mode starts automatically (no stored config / serial `/config` / screen tap within 6 s).
- Fix: `config_max_amount` was never read from the stored config.
- Fix: empty bill/coin values fall back to the defaults instead of breaking the ATM.
