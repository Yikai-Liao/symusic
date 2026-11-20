#pragma once

namespace symusic::details {

inline constexpr int tuning_steps_per_semitone = 1 << 14;
// The highest encoded value (7F 7F 7F) means leave the pitch unchanged.
inline constexpr float tuning_pitch_limit = 128.0f - 1.0f / tuning_steps_per_semitone;

inline bool valid_note_tuning(int key, float pitch) {
    return key >= 0 && key <= 127 && pitch >= 0 && pitch < tuning_pitch_limit;
}

// Call after checking the pitch range. MIDI tuning uses a 14-bit fraction.
inline int encode_tuning_pitch(float pitch) {
    return static_cast<int>(pitch * tuning_steps_per_semitone);
}

inline float decode_tuning_pitch(int pitch) {
    return pitch / static_cast<float>(tuning_steps_per_semitone);
}

}   // namespace symusic::details
