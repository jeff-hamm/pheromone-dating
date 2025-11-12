# Audio File Manager - Usage Documentation

## Overview

The Audio File Manager is a library for ESP32/Arduino projects that handles downloading, caching, and processing audio files based on DTMF sequence keys. It manages remote audio sequences from a server, caches them on an SD card for offline use, and provides a download queue system for background file downloads.

## Features

- 🌐 **Remote Sequence Download**: Fetch audio file definitions from a remote JSON server
- 💾 **SD Card Caching**: Cache sequences and audio files locally for offline operation
- 📥 **Background Download Queue**: Non-blocking download system for audio files
- ⏰ **Cache Management**: Automatic cache validation with configurable expiration
- 🔄 **Automatic Recovery**: Handles WiFi disconnections and SD card errors gracefully

## Hardware Requirements

- ESP32 or compatible Arduino board
- SD card module connected to SPI
- WiFi connectivity
- SD card chip select pin (default: GPIO 5)

## Dependencies

```cpp
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SD.h>
#include <FS.h>
```

Install these libraries via Arduino Library Manager:
- `ArduinoJson` by Benoit Blanchon

## Configuration

Configure the following constants before including the header (optional):

```cpp
// SD Card Configuration
#define SD_CS_PIN 5                          // SD card chip select pin

// Cache Settings
#define AUDIO_FILES_DIR "/audio"             // Directory for cached audio files
#define AUDIO_JSON_FILE "/audio_files.json"  // Cache file for sequences
#define CACHE_TIMESTAMP_FILE "/known_cache_time.txt"
#define CACHE_VALIDITY_HOURS (24*7)          // Cache expires after 7 days

// Limits
#define MAX_KNOWN_SEQUENCES 50               // Maximum sequences to store
#define MAX_DOWNLOAD_QUEUE 20                // Maximum download queue size
#define MAX_HTTP_RESPONSE_SIZE 8192          // Maximum JSON response size
#define MAX_FILENAME_LENGTH 64               // Maximum filename length

// Remote Server
#define KNOWN_FILES_URL "https://your-server.com/sequences.json"
#define USER_AGENT_HEADER "AudioFileManager/1.0"

#include "audio_file_manager.h"
```

## JSON Format

The remote server should return JSON in this format:

```json
{
  "123": {
    "description": "Welcome Message",
    "type": "audio",
    "path": "https://example.com/audio/welcome.mp3"
  },
  "456": {
    "description": "Goodbye Message",
    "type": "audio",
    "path": "/local/goodbye.mp3"
  },
  "789": {
    "description": "External Link",
    "type": "url",
    "path": "https://example.com"
  },
  "999": {
    "description": "Custom Service",
    "type": "service",
    "path": ""
  }
}
```

### Supported Types

- **`audio`**: Audio file to play (URL or local path)
- **`service`**: Custom service integration (TODO: requires implementation)
- **`shortcut`**: Quick action shortcut (TODO: requires implementation)
- **`url`**: Web URL to open (TODO: requires implementation)

## Basic Usage

### Setup

```cpp
#include "audio_file_manager.h"

void setup() {
    Serial.begin(115200);
    
    // Connect to WiFi first
    WiFi.begin("YourSSID", "YourPassword");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi Connected");
    
    // Initialize the audio file manager
    initializeAudioFileManager();
    
    // Download sequences from server (if cache is stale)
    if (downloadAudio()) {
        Serial.println("Sequences loaded successfully");
    }
    
    // List available sequences
    listAudioKeys();
}
```

### Main Loop

```cpp
void loop() {
    // Process download queue in background
    processAudioDownloadQueue();
    
    // Your other code here...
    
    delay(10);
}
```

### Processing Audio Keys

```cpp
void handleDTMFSequence(const char* sequence) {
    // Check if sequence is known
    if (!hasAudioKey(sequence)) {
        Serial.printf("Unknown sequence: %s\n", sequence);
        return;
    }
    
    // Process the sequence
    const char* audioPath = processAudioKey(sequence);
    
    if (audioPath) {
        // Audio file is available locally
        Serial.printf("Playing audio: %s\n", audioPath);
        // TODO: Play audio file using your audio library
        playAudioFile(audioPath);
    } else {
        // Audio not available yet (added to download queue)
        Serial.println("Audio will download in background");
    }
}
```

## API Reference

### Initialization Functions

#### `void initializeAudioFileManager()`
Initialize the audio file manager system.
- Loads cached sequences from SD card if available
- Initializes internal data structures
- **Call this in `setup()` before using other functions**

### Sequence Management

#### `bool downloadAudio()`
Download audio sequence definitions from remote server.

**Returns:** `true` if successful, `false` on error

**Behavior:**
- Only downloads if cache is stale or empty
- Requires active WiFi connection
- Automatically saves to SD card cache
- Frees existing sequences before loading new ones

**Example:**
```cpp
if (WiFi.status() == WL_CONNECTED) {
    if (downloadAudio()) {
        Serial.printf("Loaded %d sequences\n", getAudioKeyCount());
    }
}
```

#### `bool hasAudioKey(const char* sequence)`
Check if a sequence exists in the loaded sequences.

**Parameters:**
- `sequence`: The audio key to check

**Returns:** `true` if sequence exists, `false` otherwise

**Example:**
```cpp
if (hasAudioKey("123")) {
    Serial.println("Sequence 123 is available");
}
```

#### `const char* processAudioKey(const char* sequence)`
Process an audio key and get the local file path.

**Parameters:**
- `sequence`: The audio key to process

**Returns:** 
- Local file path (string) if audio is ready to play
- `nullptr` if not available or wrong type

**Behavior:**
- For audio URLs: Checks local cache or queues download
- For local paths: Returns the path directly
- For other types: Logs info but returns `nullptr`

**Example:**
```cpp
const char* path = processAudioKey("123");
if (path) {
    // Play audio immediately
    player.play(path);
} else {
    // Audio queued for download or not audio type
    Serial.println("Audio not ready");
}
```

### Information Functions

#### `void listAudioKeys()`
Print all loaded sequences to Serial output.

**Example output:**
```
📋 Known Sequences (3 total):
============================================================
 1. 123
    Description: Welcome Message
    Type: audio
    Path: https://example.com/welcome.mp3

 2. 456
    Description: Goodbye Message
    Type: audio
    Path: /local/goodbye.mp3
```

#### `int getAudioKeyCount()`
Get the number of currently loaded sequences.

**Returns:** Number of sequences

**Example:**
```cpp
Serial.printf("Total sequences: %d\n", getAudioKeyCount());
```

### Cache Management

#### `void clearAudioKeys()`
Clear all sequences from memory and SD card cache.

**Behavior:**
- Frees all allocated memory
- Deletes cache files from SD card
- Resets sequence count to 0

**Example:**
```cpp
clearAudioKeys();
Serial.println("Cache cleared, ready for fresh download");
```

### Download Queue Functions

#### `bool processAudioDownloadQueue()`
Process the next item in the download queue (non-blocking).

**Returns:** `true` if item processed, `false` if queue empty or error

**Usage:** Call this periodically in `loop()` to download files in background

**Example:**
```cpp
void loop() {
    if (processAudioDownloadQueue()) {
        Serial.println("Downloaded an audio file");
    }
    delay(10);
}
```

#### `int getDownloadQueueCount()`
Get number of remaining items in download queue.

**Returns:** Number of unprocessed items

#### `int getTotalDownloadQueueSize()`
Get total number of items ever added to queue.

**Returns:** Total queue size (including processed items)

#### `bool isDownloadQueueEmpty()`
Check if download queue is empty.

**Returns:** `true` if no items remain

**Example:**
```cpp
if (isDownloadQueueEmpty()) {
    Serial.println("All downloads complete");
}
```

#### `void listDownloadQueue()`
Print download queue status to Serial.

**Example output:**
```
📥 Audio Download Queue (3 items, 1 processed):
========================================================
 1. ✅ Downloaded Welcome Message
    URL: https://example.com/welcome.mp3
    Local: /audio/welcome.mp3

 2. 🔄 In Progress Goodbye Message
    URL: https://example.com/goodbye.mp3
    Local: /audio/goodbye.mp3

 3. ⏳ Pending Help Audio
    URL: https://example.com/help.mp3
    Local: /audio/help.mp3
```

#### `void clearDownloadQueue()`
Clear all items from download queue.

**Note:** Does not delete already downloaded files

## Complete Example

```cpp
#include <WiFi.h>
#include "audio_file_manager.h"

const char* ssid = "YourSSID";
const char* password = "YourPassword";

void setup() {
    Serial.begin(115200);
    
    // Connect WiFi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nConnected to WiFi");
    
    // Initialize audio file manager
    initializeAudioFileManager();
    
    // Load sequences
    if (downloadAudio()) {
        Serial.printf("Loaded %d audio sequences\n", getAudioKeyCount());
        listAudioKeys();
    }
}

void loop() {
    // Process downloads in background
    static unsigned long lastCheck = 0;
    if (millis() - lastCheck > 100) {
        processAudioDownloadQueue();
        lastCheck = millis();
    }
    
    // Example: Handle DTMF input
    if (Serial.available()) {
        String input = Serial.readStringUntil('\n');
        input.trim();
        
        if (hasAudioKey(input.c_str())) {
            const char* audioPath = processAudioKey(input.c_str());
            if (audioPath) {
                Serial.printf("Ready to play: %s\n", audioPath);
                // TODO: Integrate with your audio player
            }
        } else {
            Serial.println("Unknown sequence");
        }
    }
    
    delay(10);
}
```

## Troubleshooting

### SD Card Issues

**Problem:** SD card initialization fails
```
❌ SD card initialization failed
```

**Solutions:**
- Check wiring (MISO, MOSI, SCK, CS pins)
- Verify `SD_CS_PIN` matches your hardware
- Try a different SD card (some cards are incompatible)
- Format card as FAT32

### WiFi Issues

**Problem:** Cannot download sequences
```
❌ WiFi not connected, cannot download sequences
```

**Solutions:**
- Verify WiFi credentials
- Check WiFi signal strength
- Ensure internet connectivity
- Check firewall settings for HTTPS access

### Cache Issues

**Problem:** Cache not loading
```
ℹ️ No cached sequences found on SD card
```

**Solutions:**
- This is normal on first run
- Check SD card has write permissions
- Verify `/audio_files.json` exists after first download
- Run `downloadAudio()` to fetch sequences

### Memory Issues

**Problem:** Too many sequences
```
⚠️ Maximum known sequences limit reached
```

**Solutions:**
- Increase `MAX_KNOWN_SEQUENCES` (default: 50)
- Reduce number of sequences on server
- Call `clearAudioKeys()` to free memory

### Download Queue Full

**Problem:** Cannot add to queue
```
⚠️ Download queue is full, cannot add more items
```

**Solutions:**
- Increase `MAX_DOWNLOAD_QUEUE` (default: 20)
- Process queue faster in `loop()`
- Call `clearDownloadQueue()` after downloads complete

## Performance Tips

1. **Call `processAudioDownloadQueue()` frequently** (every 10-100ms) for faster downloads
2. **Pre-download audio files** during idle time
3. **Monitor queue size** to avoid overflow
4. **Use local paths** when possible to avoid downloads
5. **Set appropriate cache validity** based on your update frequency

## Security Considerations

- Use HTTPS URLs for production
- Validate JSON structure before use
- Implement authentication if needed (modify `USER_AGENT_HEADER`)
- Sanitize filenames to prevent path traversal attacks

## License

See project license file for details.

## Support

For issues and questions, please refer to the main project repository.
