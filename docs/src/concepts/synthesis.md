# Synthesis Concept

Symusic integrates audio synthesis capabilities, allowing the direct conversion of symbolic music data (represented by `Score` objects) into audible sound waveforms. This process relies on **SoundFont** technology.

## Purpose and Role

The synthesis feature bridges the gap between symbolic representation and actual sound, enabling:

-   **Auditioning**: Listening to MIDI files or programmatically generated scores without external software.
-   **Audio Dataset Creation**: Generating waveform datasets from symbolic sources for tasks like automatic transcription or instrument recognition training.
-   **Procedural Audio**: Integrating dynamic music rendering into applications or games.

[See Synthesizer API Reference](../api_reference/synthesizer.md) for detailed usage.

## Core Components & Process

The synthesis workflow involves several key parts:

1.  **`Synthesizer` Class**: The main object that orchestrates the synthesis. It's initialized with a SoundFont and parameters.
2.  **SoundFont (`.sf2`/`.sf3`)**: A file containing instrument samples and playback rules. This defines the *sound* of the output.
3.  **Input `Score`**: The symbolic music data to be rendered.
4.  **`render()` Method**: Called on the `Synthesizer` instance with the `Score`. It performs the following steps:
    *   Ensures the Score is in `Second` time unit (converts internally if needed).
    *   Iterates through score events (notes, program changes, etc.).
    *   Uses track information (`program`, `is_drum`) to select instruments from the SoundFont.
    *   Triggers and mixes the corresponding audio samples according to note properties (pitch, velocity, timing, duration) and SoundFont rules.
5.  **Output `AudioData`**: An object holding the resulting audio samples (NumPy compatible) and metadata (sample rate, channels).
6.  **`dump_wav()` Utility**: Saves the `AudioData` to a standard WAV file.

## Key Concepts

-   **SoundFont Dependency**: The quality and character of the synthesized audio depend heavily on the chosen SoundFont file. Different SoundFonts contain different instrument samples.
-   **Time Unit Requirement**: Synthesis fundamentally operates in absolute time. Therefore, input `Score` objects using `Tick` or `Quarter` units are internally converted to `Second` during rendering.
-   **Parameters (`sample_rate`, `quality`)**: These control the technical aspects of the audio output (resolution, fidelity) and the computational trade-off during rendering.

## Relationship to Other Concepts

-   Relies on **`Score`** as the complete musical input.
-   Uses **`Track`** metadata (`program`, `is_drum`) for instrument selection.
-   Interprets **`Note`** events to trigger sounds.
-   Operates in the **`Second`** time unit domain.

By integrating these components, the synthesis feature provides a direct path from symbolic representation to audible sound within the Symusic library.
