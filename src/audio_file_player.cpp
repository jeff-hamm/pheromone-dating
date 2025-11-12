/**
 * @file audio_file_player.cpp
 * 
 * This file implements audio playback functionality using the AudioTools library.
 * It consumes the audio_file_manager for file management.
 * 
 * @date 2025
 */

#include "audio_file_player.h"
#include "audio_file_manager.h"
#include "AudioTools.h"
#include <Preferences.h>

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

// Audio playback components
static AudioPlayer* audioPlayer = nullptr;
static bool isPlayingAudio = false;
static unsigned long audioStartTime = 0;
static float currentVolume = DEFAULT_AUDIO_VOLUME;
static Preferences volumePrefs;

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

/**
 * @brief Load volume from EEPROM/Preferences
 * @return Volume value from storage, or default if not found
 */
static float loadVolumeFromStorage()
{
    if (!volumePrefs.begin("audio", true)) // Read-only
    {
        Serial.println("⚠️ Failed to open volume preferences for reading");
        return DEFAULT_AUDIO_VOLUME;
    }
    
    float volume = volumePrefs.getFloat("volume", DEFAULT_AUDIO_VOLUME);
    volumePrefs.end();
    
    // Validate range
    if (volume < 0.0f || volume > 1.0f)
    {
        Serial.printf("⚠️ Invalid volume in storage: %.2f, using default\n", volume);
        return DEFAULT_AUDIO_VOLUME;
    }
    
    Serial.printf("📖 Loaded volume from storage: %.2f\n", volume);
    return volume;
}

/**
 * @brief Save volume to EEPROM/Preferences
 * @param volume Volume value to save
 */
static void saveVolumeToStorage(float volume)
{
    if (!volumePrefs.begin("audio", false)) // Read-write
    {
        Serial.println("❌ Failed to open volume preferences for writing");
        return;
    }
    
    volumePrefs.putFloat("volume", volume);
    volumePrefs.end();
    
    Serial.printf("💾 Saved volume to storage: %.2f\n", volume);
}

// ============================================================================
// PUBLIC FUNCTIONS
// ============================================================================

void initAudioFilePlayer(AudioSource &source, AudioStream &output, AudioDecoder &decoder)
{
    // Check if already initialized
    if (audioPlayer != nullptr)
    {
        Serial.println("⚠️ Audio player already initialized, skipping...");
        return;
    }
    
    Serial.println("🔧 Initializing audio player...");
    
    // Create audio player with provided source and decoder
    audioPlayer = new AudioPlayer(source, output, decoder);

    // Initialize audio file manager
    initializeAudioFileManager();
    
    // Load volume from storage
    currentVolume = loadVolumeFromStorage();
    
    // Set the volume on the player
    if (audioPlayer)
    {
        audioPlayer->setVolume(currentVolume);
        Serial.printf("🔊 Initial volume set to %.2f\n", currentVolume);
    }
    audioPlayer->begin();
    Serial.println("✅ Audio player initialized");
}

void setVolume(float volume)
{
    // Clamp volume to valid range
    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;
    
    currentVolume = volume;
    
    if (audioPlayer)
    {
        audioPlayer->setVolume(volume);
        Serial.printf("🔊 Volume set to %.2f\n", volume);
    }
    
    // Save to storage for persistence
    saveVolumeToStorage(volume);
}

float getVolume()
{
    return currentVolume;
}
bool startAudioPlayback(const char* filePath)
{
    if (!audioPlayer || !filePath || isPlayingAudio)
    {
        return false;
    }
    
    Serial.printf("🎵 Starting audio playback: %s\n", filePath);
    audioPlayer->playPath(filePath);
    isPlayingAudio = true;
    audioStartTime = millis();
    Serial.println("🎵 Audio playback started");
    
    return true;
}

void stopAudioPlayback()
{
    if (!audioPlayer)
    {
        return;
    }
    
    if (audioPlayer->isActive())
    {
        audioPlayer->end();
    }
    
    if (!isPlayingAudio)
    {
        return;
    }
    
    isPlayingAudio = false;
    Serial.println("🔇 Audio playback stopped");
}

bool isAudioPlaying()
{
    return isPlayingAudio;
}

bool processAudioFile()
{
    if (!audioPlayer || !isPlayingAudio)
    {
        return false;
    }
    
    audioPlayer->copy();
    
    // Check if playback finished
    if (!audioPlayer->isActive())
    {
        stopAudioPlayback();
        return false;
    }
    
    return true;
}

bool playAudioByKey(const char* key)
{
    if (!key || !hasAudioKey(key))
    {
        Serial.printf("❌ Audio key not found: %s\n", key ? key : "NULL");
        return false;
    }
    
    // Process the audio key to get the file path
    const char* filePath = processAudioKey(key);
    
    if (!filePath)
    {
        Serial.printf("⚠️ Audio file not available for key: %s\n", key);
        return false;
    }
    
    // Start playback
    return startAudioPlayback(filePath);
}
