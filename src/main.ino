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

#define PLAYER_1_YES 1
#define PLAYER_2_YES 2
#define PLAYER_1_NO 3
#define PLAYER_2_NO 4
#define RESET_GAME 5

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
    AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info); // setup Audiokit

    // Add more startup delay for system stabilization
    Logger.println("🔧 Allowing system to stabilize...");
    delay(3000);
    auto cfg = kit.defaultConfig(TX_MODE);
    cfg.sd_active = false;
    if (!kit.begin(cfg))
    {
        Logger.println("❌ Failed to initialize AudioKit");
    }
    else {
        Logger.println("✅ AudioKit initialized successfully");
    }
    initAudioFilePlayer(source, kit, decoder);

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
        Logger.println("🔄 Reset button pressed - resetting game");
        resetGame();
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