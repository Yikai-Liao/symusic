# Data Processing Tutorial

Symusic's high performance makes it particularly well-suited for processing large datasets of symbolic music files, a common task in Music Information Retrieval (MIR) and machine learning.

## Why Symusic for Data Processing?

-   **Speed**: Loading and basic manipulation of MIDI files are significantly faster than pure Python libraries.
-   **Efficiency**: C++ backend reduces memory overhead compared to Python object representations.
-   **Multiprocessing**: Symusic objects are efficiently serializable (pickleable), making them work seamlessly with Python's `multiprocessing` module for parallel processing.

## Common Data Processing Tasks

1.  **Loading and Filtering**: Reading a large dataset and selecting relevant files or tracks.
2.  **Data Cleaning**: Removing empty files, fixing timing issues, standardizing formats.
3.  **Feature Extraction**: Computing musical features (e.g., pitch histograms, note density, tempo).
4.  **Representation Conversion**: Converting MIDI to other formats like piano roll or custom event sequences for model input.
5.  **Data Augmentation**: Applying transformations like transposition or time stretching.

## Using `multiprocessing`

Python's `multiprocessing` library is the standard way to parallelize CPU-bound tasks across multiple cores. Symusic objects integrate well with this.

```python
import multiprocessing as mp
from pathlib import Path
from symusic import Score
import time
import pickle # Symusic uses efficient pickling

def worker_function(midi_path: Path) -> dict | None:
    """Loads a MIDI file and extracts some basic features."""
    try:
        # Load the score
        score = Score(midi_path)

        # Skip empty scores
        if score.empty():
            return None
            
        # Basic feature extraction
        num_notes = score.note_num()
        duration_seconds = score.to("second").end()
        avg_tempo = 0
        if score.tempos:
            avg_tempo = sum(t.qpm for t in score.tempos) / len(score.tempos)

        return {
            "filename": midi_path.name,
            "num_notes": num_notes,
            "duration_sec": duration_seconds,
            "avg_tempo": avg_tempo
        }

    except Exception as e:
        print(f"Error processing {midi_path.name}: {e}")
        return None

if __name__ == "__main__":
    start_time = time.time()
    
    input_dir = Path("path/to/your/midi/dataset")
    midi_files = list(input_dir.glob("**/*.mid")) # Recursively find MIDI files
    print(f"Found {len(midi_files)} MIDI files.")

    # Determine number of processes (use all available cores)
    num_processes = mp.cpu_count()
    print(f"Using {num_processes} processes.")

    # Process files in parallel
    with mp.Pool(processes=num_processes) as pool:
        # Use map to apply the worker function to each file path
        results = pool.map(worker_function, midi_files)

    # Filter out None results (errors or empty files)
    valid_results = [r for r in results if r is not None]
    
    end_time = time.time()
    print(f"\nProcessed {len(valid_results)} files in {end_time - start_time:.2f} seconds.")

    # Example: Save results to a file (e.g., JSON or CSV)
    # import json
    # with open("dataset_features.json", "w") as f:
    #     json.dump(valid_results, f, indent=2)

    # Example: Print some stats
    if valid_results:
        avg_notes = sum(r['num_notes'] for r in valid_results) / len(valid_results)
        avg_duration = sum(r['duration_sec'] for r in valid_results) / len(valid_results)
        print(f"Average notes per file: {avg_notes:.1f}")
        print(f"Average duration per file: {avg_duration:.1f} seconds")
```

**Explanation:**

1.  **`worker_function`**: This function takes a single `Path` object, loads the MIDI file using `Score`, performs some processing (feature extraction in this case), and returns the result (or `None` on error/empty score).
2.  **`if __name__ == "__main__":`**: Essential for `multiprocessing` to work correctly, especially on Windows.
3.  **`mp.Pool`**: Creates a pool of worker processes.
4.  **`pool.map(worker_function, midi_files)`**: This is the core of the parallel processing. It distributes the `midi_files` list among the worker processes, each calling `worker_function` on a subset of the files. It collects the results in order.
5.  **Result Handling**: The collected `results` list contains the return values from `worker_function` for each file.

## Tips for Large Datasets

-   **Error Handling**: Include `try...except` blocks in your worker function to gracefully handle corrupted or problematic MIDI files without crashing the entire process.
-   **Memory Management**: While Symusic is efficient, processing extremely large files or extracting complex features might still consume significant memory. Monitor memory usage. If needed, process files individually or use generators if results don't all need to be in memory at once.
-   **Logging**: For long-running jobs, use Python's `logging` module instead of `print` for better tracking of progress and errors, especially when redirecting output to files.
-   **Serialization Cost**: Symusic's pickling is fast, but transferring large processed `Score` objects back from worker processes can still have overhead. If only extracted features are needed, return just those features from the worker function, not the entire `Score` object.
-   **Intermediate Results**: For very large datasets, consider saving intermediate results periodically to avoid losing work in case of interruptions.

## Example: Filtering and Saving Piano Tracks

This example processes a dataset, keeps only the piano tracks, and saves the modified files.

```python
import multiprocessing as mp
from pathlib import Path
from symusic import Score
import time

def filter_piano_tracks(midi_path: Path, output_dir: Path):
    """Loads a MIDI, keeps only piano tracks, and saves to output_dir."""
    try:
        score = Score(midi_path)
        if score.empty():
            return f"Skipped (empty): {midi_path.name}"

        # Find piano tracks (Program 0, not drums)
        piano_tracks = [t for t in score.tracks if t.program == 0 and not t.is_drum]

        if not piano_tracks:
            return f"Skipped (no piano): {midi_path.name}"
        
        # Create a new score with only piano tracks
        # Important: Need to copy global events like tempos, time signatures
        new_score = Score(score.ticks_per_quarter)
        new_score.tracks = piano_tracks # Assign the filtered tracks
        new_score.tempos = score.tempos.copy()
        new_score.time_signatures = score.time_signatures.copy()
        new_score.key_signatures = score.key_signatures.copy()
        new_score.markers = score.markers.copy()
        # Note: Deep copies might be needed depending on subsequent modifications

        output_path = output_dir / midi_path.name
        new_score.dump_midi(output_path)
        return f"Processed: {midi_path.name}"
        
    except Exception as e:
        return f"Error processing {midi_path.name}: {e}"

if __name__ == "__main__":
    start_time = time.time()
    input_dir = Path("path/to/your/midi/dataset")
    output_dir = Path("path/to/filtered/piano_midi")
    output_dir.mkdir(parents=True, exist_ok=True)

    midi_files = list(input_dir.glob("**/*.mid"))
    print(f"Found {len(midi_files)} MIDI files.")

    num_processes = mp.cpu_count()
    print(f"Using {num_processes} processes.")

    # Prepare arguments for starmap
    args = [(mf, output_dir) for mf in midi_files]

    with mp.Pool(processes=num_processes) as pool:
        # Use starmap for functions taking multiple arguments
        results = pool.starmap(filter_piano_tracks, args)

    end_time = time.time()
    print(f"\nFinished in {end_time - start_time:.2f} seconds.")

    # Print summary/errors
    processed_count = sum(1 for r in results if r.startswith("Processed"))
    skipped_count = sum(1 for r in results if r.startswith("Skipped"))
    error_count = sum(1 for r in results if r.startswith("Error"))
    print(f"Processed: {processed_count}, Skipped: {skipped_count}, Errors: {error_count}")
    # Optionally log the specific error messages
    # for r in results:
    #     if r.startswith("Error"):
    #         print(r)
```

This tutorial demonstrates how to leverage Symusic's performance for efficient batch processing of symbolic music data using Python's standard multiprocessing tools. 