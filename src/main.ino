#include "AudioTools.h"
#include "AudioTools/AudioLibs/AudioBoardStream.h"
#include "AudioTools/AudioLibs/AudioRealFFT.h" // or AudioKissFFT
#include "AudioTools/Disk/AudioSourceSDMMC.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"

#include "audio_file_manager.h"
#include "audio_file_player.h"
#include "wifi_manager.h"
#include "logging.h"
#include <SD.h>
#include <SD_MMC.h>
#include <Preferences.h>

// Firmware version tracking for cache reset on new uploads
#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "1.0.0"  // Update this when you want to force cache clear
#endif

// ESP32 Audio Kit typically has buttons 2-6 available (button 1 may be reserved)
#define PLAYER_1_YES 6  // Left YES button
#define PLAYER_1_NO 3   // Left NO button
#define PLAYER_2_YES 4  // Right YES button
#define PLAYER_2_NO 5   // Right NO button
#define RESET_GAME 2    // Reset/Mode button

#ifndef GAME_TIMEOUT_MS
#define GAME_TIMEOUT_MS 60000  // 60 seconds default timeout
#endif

#ifndef YES_SOUND_KEY
#define YES_SOUND_KEY "yes"
#endif

#ifndef NO_SOUND_KEY
#define NO_SOUND_KEY "no"
#endif

#ifndef LOCKED_IN_SOUND_KEY
#define LOCKED_IN_SOUND_KEY "locked_in"
#endif

#ifndef RESET_GAME_SOUND_KEY
#define RESET_GAME_SOUND_KEY "reset_game"
#endif

AudioBoardStream kit(AudioKitEs8388V1); // Audio source

#ifndef AUDIO_START_PATH
#define AUDIO_START_PATH "/"
#endif

// Audio components
AudioSourceSDMMC source(AUDIO_START_PATH);
MP3DecoderHelix decoder;

// Button press tracking
struct ButtonPress {
    unsigned long timestamp;
    bool value; // true for YES, false for NO
    bool hasPressed; // true if this player has pressed a button this round
};

ButtonPress player1LastPress = {0, false, false};
ButtonPress player2LastPress = {0, false, false};

// Game state
enum GameState {
    WAITING_FOR_PLAYERS,
    GAME_COMPLETE,
    PLAYING_SOUND
};

GameState gameState = WAITING_FOR_PLAYERS;
unsigned long firstPressTime = 0;
unsigned long lastLedToggleTime = 0;
bool ledState = false;
bool warningActive = false;

// D1, D2, D3 LED pins (common on ESP32 Audio Kit v2.2 A436)
// Trying alternate GPIO pins that are typically available
#define LED_D1 12  // Green LED (alternate pin)
#define LED_D2 13  // Red LED (alternate pin)
#define LED_D3 14  // Blue LED (alternate pin)

// Firmware version tracker
Preferences preferences;

/**
 * @brief Check if firmware version has changed and reset cache if needed
 * 
 * Compares stored firmware version with current version. If different,
 * clears audio cache to ensure fresh download with new firmware.
 */
void checkFirmwareVersionAndResetCache() {
    preferences.begin("firmware", false);
    
    String storedVersion = preferences.getString("version", "");
    String currentVersion = String(FIRMWARE_VERSION);
    
    if (storedVersion != currentVersion) {
        Logger.printf("🔄 Firmware version changed: %s -> %s\n", 
                     storedVersion.c_str(), currentVersion.c_str());
        Logger.println("🗑️ Clearing audio cache due to firmware update...");
        
        // Clear the audio cache
        clearAudioKeys();
        
        // Store new version
        preferences.putString("version", currentVersion);
        Logger.printf("✅ Cache cleared and version updated to %s\n", currentVersion.c_str());
    } else {
        Logger.printf("ℹ️ Firmware version unchanged: %s\n", currentVersion.c_str());
    }
    
    preferences.end();
}

// WiFi connected callback - downloads audio sequences when WiFi connects
void onWiFiConnected()
{
    Logger.println("🌐 WiFi connected - downloading audio sequences...");
    
    // Download sequences from server (if cache is stale)
    if (downloadAudio())
    {
        Logger.println("✅ Sequences loaded successfully");
        listAudioKeys();
    }
    else
    {
        Logger.println("⚠️ Failed to download sequences (using cached data if available)");
    }
}

void setup()
{
    Serial.begin(115200);
    delay(2000); // Give serial time to initialize

    // Initialize logging system first
    Logger.addLogger(Serial);
    
    Logger.printf("=== Starting ===\n");
    AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

    // Check firmware version and reset cache if changed
    checkFirmwareVersionAndResetCache();

    // Initialize SD_MMC in 1-bit mode (more reliable on some boards)
    Logger.println("🔧 Initializing SD_MMC (1-bit mode)...");
    if (!SD_MMC.begin("/sdcard", true)) {  // true = 1-bit mode
        Logger.println("❌ Failed to initialize SD_MMC");
    } else if (SD_MMC.cardType() == CARD_NONE) {
        Logger.println("❌ No SD card detected");
    } else {
        uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
        Logger.printf("✅ SD_MMC initialized (1-bit mode, %lluMB)\n", cardSize);
    }

    // Add more startup delay for system stabilization
    Logger.println("🔧 Allowing system to stabilize...");
    delay(3000);
    
    // Initialize external LEDs (D1, D2, D3) - turn them off initially
    pinMode(LED_D1, OUTPUT);
    pinMode(LED_D2, OUTPUT);
    pinMode(LED_D3, OUTPUT);
    digitalWrite(LED_D1, LOW);
    digitalWrite(LED_D2, LOW);
    digitalWrite(LED_D3, LOW);
    Logger.println("✅ External LEDs initialized (off)");
    
    auto cfg = kit.defaultConfig(TX_MODE);
    cfg.sd_active = false;  // Don't let AudioKit re-initialize SD (we're using SD_MMC)
    if (!kit.begin(cfg))
    {
        Logger.println("❌ Failed to initialize AudioKit");
    }
    else {
        Logger.println("✅ AudioKit initialized successfully");
    }
    
    // Initialize with SD_MMC support enabled (true)
    initAudioFilePlayer(source, kit, decoder, 0, true);  // CS pin=0 (unused for SD_MMC)

    Logger.println("🎤 Audio system ready!");

    // Initialize WiFi in background (non-blocking) with callback
    Logger.println("🔧 Starting WiFi initialization in background...");
    initWiFi(onWiFiConnected);    // Configure OTA updates (will start when WiFi is ready)
    Logger.println("🔄 Configuring OTA updates");
    initOTA();
    kit.addAction(kit.getKey(PLAYER_1_YES), buttonPressed);
    kit.addAction(kit.getKey(PLAYER_2_YES), buttonPressed);
    kit.addAction(kit.getKey(PLAYER_1_NO), buttonPressed);
    kit.addAction(kit.getKey(PLAYER_2_NO), buttonPressed);
    kit.addAction(kit.getKey(RESET_GAME), [](bool active, int pin, void *ptr) {
        if (gameState == WAITING_FOR_PLAYERS && firstPressTime > 0) {
            // Calculate elapsed time
            unsigned long elapsed = millis() - firstPressTime;
            unsigned long remaining = 0;
            
            // Only extend if less than 119 seconds have elapsed
            if (elapsed < 119000) {  // 119 seconds max
                // Calculate how much time we can add (max 60 seconds, but cap at 119 total)
                unsigned long maxExtension = 119000 - elapsed;
                unsigned long extension = min(60000UL, maxExtension);
                
                // Move the start time back by the extension amount
                firstPressTime -= extension;
                remaining = (119000 - (millis() - firstPressTime)) / 1000;
                
                Logger.printf("🔄 Reset button pressed - extending game by %lu seconds (max %lu seconds remaining)\n", 
                             extension / 1000, remaining);
                playAudioByKey(RESET_GAME_SOUND_KEY);
            } else {
                Logger.println("⏰ Cannot extend - maximum 119 seconds already reached");
            }
        } else {
            Logger.println("⚠️ Reset button pressed but no active game to extend");
        }
    });
    Logger.println("✅ Startup complete!"); 
}

void buttonPressed(bool active, int pin, void *ptr) {
    // Ignore button presses while playing sound
    if (gameState == PLAYING_SOUND) {
        return;
    }
    
    unsigned long timestamp = millis();
    bool wasFirstPress = false;
    
    // Check if this is the first button press of the round
    if (!player1LastPress.hasPressed && !player2LastPress.hasPressed) {
        wasFirstPress = true;
        firstPressTime = timestamp;
    }
    
    if(kit.getKey(PLAYER_1_YES) == pin) {
        Logger.println("YES 1 button pressed");
        player1LastPress.timestamp = timestamp;
        player1LastPress.value = true;
        player1LastPress.hasPressed = true;
    } else if(kit.getKey(PLAYER_2_YES) == pin) {
        Logger.println("YES 2 button pressed");
        player2LastPress.timestamp = timestamp;
        player2LastPress.value = true;
        player2LastPress.hasPressed = true;
    } else if(kit.getKey(PLAYER_1_NO) == pin) {
        Logger.println("NO 1 button pressed");
        player1LastPress.timestamp = timestamp;
        player1LastPress.value = false;
        player1LastPress.hasPressed = true;
    } else if(kit.getKey(PLAYER_2_NO) == pin) {
        Logger.println("NO 2 button pressed");
        player2LastPress.timestamp = timestamp;
        player2LastPress.value = false;
        player2LastPress.hasPressed = true;
    }
    
    // Play "locked in" sound if this was the first press and the other player hasn't pressed yet
    if (wasFirstPress && !(player1LastPress.hasPressed && player2LastPress.hasPressed)) {
        Logger.printf("🔒 First player locked in! Waiting for other player... (%d seconds remaining)\n", GAME_TIMEOUT_MS / 1000);
        playAudioByKey(LOCKED_IN_SOUND_KEY);
    }
}

void loop()
{
    // Handle WiFi management (config portal and OTA)
    handleWiFiLoop();
    processAudioDownloadQueue();
    processAudioFile();
    kit.processActions();
    processGame();
    processLedWarning();
}

void processLedWarning() {
    // Only flash LEDs when waiting for players and game is active
    if (gameState == WAITING_FOR_PLAYERS && firstPressTime > 0) {
        unsigned long elapsed = millis() - firstPressTime;
        unsigned long remaining = GAME_TIMEOUT_MS - elapsed;
        
        // Start flashing when 10 seconds or less remain
        if (remaining <= 10000) {
            if (!warningActive) {
                warningActive = true;
                Logger.println("⚠️ WARNING: 10 seconds remaining - LEDs flashing!");
            }
            
            // Flash LEDs every 500ms
            if (millis() - lastLedToggleTime >= 500) {
                ledState = !ledState;
                lastLedToggleTime = millis();
                
                // Set all available board LEDs to the same state
                for (int i = 0; i < 10; i++) {  // Try up to 10 LEDs
                    int ledPin = kit.pinLed(i);
                    if (ledPin >= 0) {  // Valid pin exists
                        digitalWrite(ledPin, ledState ? HIGH : LOW);
                    }
                }
                
                // Also flash external D1, D2, D3 LEDs
                digitalWrite(LED_D1, ledState ? HIGH : LOW);
                digitalWrite(LED_D2, ledState ? HIGH : LOW);
                digitalWrite(LED_D3, ledState ? HIGH : LOW);
            }
        } else if (warningActive) {
            // Turn off warning when time is extended
            warningActive = false;
            ledState = false;
            for (int i = 0; i < 10; i++) {
                int ledPin = kit.pinLed(i);
                if (ledPin >= 0) {
                    digitalWrite(ledPin, LOW);
                }
            }
            // Turn off external LEDs too
            digitalWrite(LED_D1, LOW);
            digitalWrite(LED_D2, LOW);
            digitalWrite(LED_D3, LOW);
        }
    } else if (warningActive) {
        // Game ended or reset - turn off LEDs
        warningActive = false;
        ledState = false;
        for (int i = 0; i < 10; i++) {
            int ledPin = kit.pinLed(i);
            if (ledPin >= 0) {
                digitalWrite(ledPin, LOW);
            }
        }
        // Turn off external LEDs
        digitalWrite(LED_D1, LOW);
        digitalWrite(LED_D2, LOW);
        digitalWrite(LED_D3, LOW);
    }
}


void processGame() {
    if (gameState == WAITING_FOR_PLAYERS) {
        // Check if both players have pressed
        if (player1LastPress.hasPressed && player2LastPress.hasPressed) {
            // Both players have answered
            Logger.println("🎮 Both players have answered!");
            
            // Check if either player pressed NO
            if (!player1LastPress.value || !player2LastPress.value) {
                Logger.println("❌ At least one player said NO - playing NO sound");
                playAudioByKey(NO_SOUND_KEY);
            } else {
                // Both said YES - check if within timeout
                unsigned long timeDiff = abs((long)(player1LastPress.timestamp - player2LastPress.timestamp));
                if (timeDiff <= GAME_TIMEOUT_MS) {
                    Logger.printf("✅ Both players said YES within %d seconds - playing YES sound!\n", GAME_TIMEOUT_MS / 1000);
                    playAudioByKey(YES_SOUND_KEY);
                } else {
                    Logger.printf("⏰ Both said YES but took too long (%lu ms) - playing NO sound\n", timeDiff);
                    playAudioByKey(NO_SOUND_KEY);
                }
            }
            
            gameState = PLAYING_SOUND;
        }
        // Check for timeout if at least one player has pressed
        else if ((player1LastPress.hasPressed || player2LastPress.hasPressed) && firstPressTime > 0) {
            unsigned long elapsed = millis() - firstPressTime;
            if (elapsed > GAME_TIMEOUT_MS) {
                Logger.printf("⏰ Timeout! Only one player answered within %d seconds - playing NO sound\n", GAME_TIMEOUT_MS / 1000);
                playAudioByKey(NO_SOUND_KEY);
                gameState = PLAYING_SOUND;
            }
        }
    }
    else if (gameState == PLAYING_SOUND) {
        // Wait for sound to finish playing
        if (!isAudioPlaying()) {
            Logger.println("🔄 Sound finished - resetting game");
            resetGame();
        }
    }
}

void resetGame() {
    player1LastPress.timestamp = 0;
    player1LastPress.value = false;
    player1LastPress.hasPressed = false;
    
    player2LastPress.timestamp = 0;
    player2LastPress.value = false;
    player2LastPress.hasPressed = false;
    
    firstPressTime = 0;
    gameState = WAITING_FOR_PLAYERS;
    
    Logger.println("🎮 Game reset - ready for next round!");
}