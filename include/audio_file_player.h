/**
 * @file audio_file_player.h
 * @brief Audio File Player Header
 * 
 * This library handles audio playback using the AudioTools library.
 * It consumes the audio_file_manager for file management.
 * 
 * @date 2025
 */

#ifndef AUDIO_FILE_PLAYER_H
#define AUDIO_FILE_PLAYER_H

// ============================================================================
// INCLUDES
// ============================================================================
#include <Arduino.h>
#include "AudioTools/AudioLibs/AudioBoardStream.h"
#include "AudioTools.h"

// ============================================================================
// CONSTANTS AND CONFIGURATION
// ============================================================================

#ifndef DEFAULT_AUDIO_VOLUME
#define DEFAULT_AUDIO_VOLUME 0.7  ///< Default audio volume (0.0 to 1.0)
#endif

#ifndef AUDIO_VOLUME_EEPROM_ADDRESS
#define AUDIO_VOLUME_EEPROM_ADDRESS 100  ///< EEPROM address for volume storage
#endif

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================

/**
 * @brief Initialize the audio player system
 * @param kit Reference to the AudioBoardStream
 * @param source Reference to the audio source
 * @param decoder Reference to the audio decoder
 * 
 * Sets up the audio player with the provided source and decoder.
 * Call this after initializeAudioFileManager().
 */
void initAudioFilePlayer(AudioSource &source, AudioStream &output, AudioDecoder &decoder);

/**
 * @brief Start playing an audio file
 * @param filePath Path to the audio file to play
 * @return true if playback started, false otherwise
 * 
 * Non-blocking call. Use copyAudioData() in loop to continue playback.
 */
bool startAudioPlayback(const char* filePath);

/**
 * @brief Stop current audio playback
 */
void stopAudioPlayback();

/**
 * @brief Check if audio is currently playing
 * @return true if playing, false otherwise
 */
bool isAudioPlaying();

/**
 * @brief Set the audio volume
 * @param volume Volume level (0.0 to 1.0)
 * 
 * Sets the volume and saves it to EEPROM for persistence.
 */
void setVolume(float volume);

/**
 * @brief Get the current audio volume
 * @return Current volume level (0.0 to 1.0)
 */
float getVolume();

/**
 * @brief Copy audio data (call this in main loop during playback)
 * @return true if still playing, false if finished
 */
bool processAudioFile();

/**
 * @brief Play an audio file by key
 * @param key Audio key to look up and play
 * @return true if playback started, false if key not found or error
 * 
 * Looks up the key in audio_file_manager and plays the associated file.
 */
bool playAudioByKey(const char* key);

#endif // AUDIO_FILE_PLAYER_H
