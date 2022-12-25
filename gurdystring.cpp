#include "gurdystring.h"

/// @brief Constructor.  GurdyString manages turning "strings" on and off, determining its note, and interacting with the MIDI layer.
/// @param my_channel The MIDI channel to communicate over
/// @param my_note The base MIDI note of this string (0-127)
/// @param my_name A text label for this string (e.g. "Drone")
/// @param my_vol The volume of this string (0-127)
GurdyString::GurdyString(int my_channel, int my_note, String my_name, int my_vol) {
  midi_channel = my_channel;
  name = my_name;
  open_note = my_note;
  midi_volume = my_vol;
  trigger_volume = int((my_vol)/128.0 * 80 - 70);
  note_being_played = open_note;
};

// soundOn() sends sound on this string's channel at its notes
// optionally with an additional offset (e.g. a key being pressed)
//
// I DO NOT KNOW why I can just access usbMIDI here, but even when
// creating MIDI_obj globally, I could not access MIDI_obj the same way.
// Bringing in a pointer and working with MIDI_obj in this manner is
// because of that.

/// @brief Turns on sound over this string's MIDI channel at its current volume
/// @param my_offset The offset from the string's base note to make sound
/// @param my_modulation The amount of optional modulation (0-127) to apply to the sound.  This is MIDI CC1.  0 == no modulation.
/// @warning The way this is currently written, only one note may be playing per string object.  Don't call this twice in a row without calling soundOff() first.
void GurdyString::soundOn(int my_offset, int my_modulation) {
  note_being_played = open_note + my_offset;
  if (!mute_on) {
    usbMIDI.sendNoteOn(note_being_played, midi_volume, midi_channel);
#if !defined(USE_TRIGGER) && !defined(USE_TSUNAMI)
    MIDI.sendNoteOn(note_being_played, midi_volume, midi_channel);
#elif defined(USE_TRIGGER)
    trigger_obj.trackGain(note_being_played + (128 * (midi_channel - 1)), trigger_volume);
    trigger_obj.trackPlayPoly(note_being_played + (128 * (midi_channel - 1)), true);
    trigger_obj.trackLoop(note_being_played + (128 * (midi_channel - 1)), true);
#elif defined(USE_TSUNAMI)
    trigger_obj.trackGain(note_being_played + (128 * (midi_channel - 1)), trigger_volume);
    trigger_obj.trackPlayPoly(note_being_played + (128 * (midi_channel - 1)), TSUNAMI_OUT, true);
    trigger_obj.trackLoop(note_being_played + (128 * (midi_channel - 1)), true);
#endif

    // If modulation isn't zero, send that as a MIDI CC for this channel
    // This is meant to be configured to create a gentle vibrato.
    if (my_modulation > 0) {
      usbMIDI.sendControlChange(1, my_modulation, midi_channel);
#if !defined(USE_TRIGGER) && !defined(USE_TSUNAMI)
      MIDI.sendControlChange(1, my_modulation, midi_channel);
#endif
    }
  }
  is_playing = true;
};

/// @brief  Turns off the sound currently playing for this string, nicely.
void GurdyString::soundOff() {
  usbMIDI.sendNoteOff(note_being_played, midi_volume, midi_channel);
#if !defined(USE_TRIGGER) && !defined(USE_TSUNAMI)
  MIDI.sendNoteOff(note_being_played, midi_volume, midi_channel);
#else
  if (trigger_volume > -60) {
    trigger_obj.trackFade(note_being_played + (128 * (midi_channel - 1)), trigger_volume - 10, 200, true);
  } else {
    trigger_obj.trackFade(note_being_played + (128 * (midi_channel - 1)), -70, 200, true);
  }
  //trigger_obj.trackStop(note_being_played + (128 * (midi_channel - 1)));
#endif
  is_playing = false;
};

// soundKill is a nuclear version of soundOff() that kills sound on the channel.
// It does not need to know the note being played as it kills all of them.

/// @brief Issues a MIDI CC123 to the string's MIDI channel, killing all sound on it.
/// @note On Tsunami/Trigger units, this kills *all* tracks playing.  This is not meant be the regular way to turn off sound, see soundOff() which does it more gently.
void GurdyString::soundKill() {
  usbMIDI.sendControlChange(123, 0, midi_channel);
#if !defined(USE_TRIGGER) && !defined(USE_TSUNAMI)
  MIDI.sendControlChange(123, 0, midi_channel);
#else
  trigger_obj.stopAllTracks();
#endif
  is_playing = false;
};

/// @brief Returns the string's open (base) note.
/// @return The string's base note as a MIDI note number 0-127
int GurdyString::getOpenNote() {
  return open_note;
};

/// @brief Sets a new base note for this string.
/// @param new_note The new base MIDI note (0-127) for this string
void GurdyString::setOpenNote(int new_note) {
  open_note = new_note;
};

// MIDI volume is an integer between 0 (off) and 127 (full volume).

/// @brief Sets a new volume for this string.
/// @param vol The new MIDI volume for this string.  0 = silent, 127 = full volume.
/// @note On Tsunami/Trigger units, this is translated to the -70 to +10 scale used on those internally.
/// * MIDI volume 112 = line-out volume level on Tsunami/Trigger units.
void GurdyString::setVolume(int vol) {
  midi_volume = vol;
  trigger_volume = int((vol)/128.0 * 80 - 70);
};

/// @brief Returns the string's MIDI volume.
/// @return The string's volume, 0-127
int GurdyString::getVolume() {
  return midi_volume;
};

/// @brief Mutes/unmutes the string.
/// @param mute True = mute, false = unmute
/// @note While muted, soundOn/Off events do nothing.  This exists for programming ease (calling sounOn() on all strings without checking if they are muted).
void GurdyString::setMute(bool mute) {
  mute_on = mute;
};

/// @brief Returns the string's mute/unmute status.
/// @return True if muted, false if unmuted
bool GurdyString::getMute() {
  return mute_on;
};

/// @brief Reports if the string is currently playing a note.
/// @return True if playing a note, false otherwise.
bool GurdyString::isPlaying() {
  return is_playing;
};

/// @brief Send a MIDI Program Change to this string's MIDI channel.
/// @param program The program change value, 0-127.
/// @note This has no effect on Tsunami/Trigger units.
void GurdyString::setProgram(uint8_t program) {
  usbMIDI.sendProgramChange(program, midi_channel);
#if !defined(USE_TRIGGER) && !defined(USE_TSUNAMI)
  MIDI.sendProgramChange(program, midi_channel);
#endif
};

/// @brief Sends a MIDI CC11 (Expression) value to this string's MIDI channel.
/// @param exp The expression value, 0-127.
/// @note This has no effect on Tsunami/Trigger units.
void GurdyString::setExpression(int exp) {
      usbMIDI.sendControlChange(11, exp, midi_channel);
#if !defined(USE_TRIGGER) && !defined(USE_TSUNAMI)
      MIDI.sendControlChange(11, exp, midi_channel);
#endif
};

/// @brief Bends this string's sound to the specified amount.
/// @param bend The amount of pitch bend.  0 to 16383, where 8192 = no bend.
/// @note This has no effect on Tsunami/Trigger units.
void GurdyString::setPitchBend(int bend) {
  usbMIDI.sendPitchBend(bend, midi_channel);
  #if !defined(USE_TRIGGER) && !defined(USE_TSUNAMI)
    MIDI.sendPitchBend(bend, midi_channel);
  #endif
};

/// @brief Sets the amount of modulation (vibrato) on this string.
/// @param vib The amount of modulation, 0-127.
/// @note This is MIDI CC1, the "mod wheel".  Intended to used for a vibrato effect.
void GurdyString::setVibrato(int vib) {
  usbMIDI.sendControlChange(1, vib, midi_channel);
#if !defined(USE_TRIGGER) && !defined(USE_TSUNAMI)
  MIDI.sendControlChange(1, vib, midi_channel);
#endif
};

/// @brief Returns the text name of this string.
/// @return The string's display name
String GurdyString::getName() {
  return name;
};
