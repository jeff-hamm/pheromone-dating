# Firmware Version & Cache Reset

## Overview

The firmware includes an automatic cache reset feature that clears all downloaded audio files when you upload a new version of the firmware. This ensures that audio files are always fresh and compatible with the current firmware version.

## How It Works

1. **Version Tracking**: The firmware version is stored in ESP32's NVS (Non-Volatile Storage) using the Preferences library
2. **Automatic Detection**: On each boot, the firmware compares the stored version with the current version
3. **Cache Reset**: If versions differ, the audio cache is automatically cleared from both RAM and SD card
4. **Fresh Download**: After cache reset, new audio files will be downloaded from the remote server

## Configuring the Version

The firmware version is set in `platformio.ini`:

```ini
-DFIRMWARE_VERSION=\"1.0.0\"
```

You can also override it in `main.ino` if needed (though platformio.ini is recommended):

```cpp
#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "1.0.0"
#endif
```

## When to Increment the Version

Increment the firmware version whenever:

- ✅ Audio file format changes
- ✅ JSON schema for audio definitions changes
- ✅ Audio file naming or path structure changes
- ✅ You want to force all devices to re-download audio files
- ✅ Major feature updates that affect audio playback

You DON'T need to increment for:

- ❌ Bug fixes that don't affect audio files
- ❌ UI changes
- ❌ Game logic updates (unless they require new audio files)

## Version Increment Examples

### Patch Release (Bug fixes)
```ini
-DFIRMWARE_VERSION=\"1.0.0\"  →  -DFIRMWARE_VERSION=\"1.0.1\"
```

### Minor Release (New features)
```ini
-DFIRMWARE_VERSION=\"1.0.1\"  →  -DFIRMWARE_VERSION=\"1.1.0\"
```

### Major Release (Breaking changes)
```ini
-DFIRMWARE_VERSION=\"1.1.0\"  →  -DFIRMWARE_VERSION=\"2.0.0\"
```

## Serial Monitor Output

When the firmware boots, you'll see one of these messages:

**Version unchanged:**
```
ℹ️ Firmware version unchanged: 1.0.0
```

**Version changed (cache cleared):**
```
🔄 Firmware version changed: 1.0.0 -> 1.1.0
🗑️ Clearing audio cache due to firmware update...
✅ Cleared SD card cache files
✅ Cache cleared and version updated to 1.1.0
```

## Manual Cache Reset

If you need to manually clear the cache without changing the version, you can call:

```cpp
clearAudioKeys();
```

Or press the RESET_GAME button if configured.

## Technical Details

- **Storage**: Version stored in NVS namespace "firmware", key "version"
- **Persistence**: Version survives power cycles and reboots
- **Cache Files Cleared**:
  - `/audio_files.json` - Cached audio definitions
  - `/known_cache_time.txt` - Cache timestamp
  - All loaded audio file entries in RAM

## Troubleshooting

**Cache not clearing:**
- Check that `FIRMWARE_VERSION` is properly defined
- Verify Preferences library is working (check Serial output)
- Ensure SD card is properly initialized

**Version not updating:**
- Make sure you uploaded the new firmware (not just compiled)
- Check that platformio.ini changes were saved before uploading
- Verify Serial output shows the version check

**Fresh install:**
On first boot, you'll see an empty stored version:
```
🔄 Firmware version changed:  -> 1.0.0
```
This is normal and indicates the firmware is setting up version tracking for the first time.
