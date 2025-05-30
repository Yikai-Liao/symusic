#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem> // For directory traversal
#include <iomanip>    // For std::setw, std::fixed, std::setprecision for progress bar
#include <algorithm>  // For std::min
#include <span>       // For std::span

#include "symusic/score.h" // For symusic::Score
#include "symusic/io/midi.h" // For symusic::io::parse_midi
// #include "CLI/CLI.hpp"   // For command line argument parsing // REMOVED

namespace fs = std::filesystem;

// Function to read entire file into a vector of char (or uint8_t)
std::vector<uint8_t> read_file_to_bytes(const fs::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + path.string());
    }
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        throw std::runtime_error("Failed to read file: " + path.string());
    }
    return buffer;
}

// Function to count notes in a score
size_t count_notes(const symusic::Score<symusic::Tick>& score) {
    size_t note_count = 0;
    if (score.tracks) { // Check if tracks pointer is valid
        for (const auto& track : *score.tracks) { // Dereference score.tracks
            if (track && track->notes) { // Check if track and track->notes are valid
                note_count += track->notes->size(); // Use -> for shared_ptr access
            }
        }
    }
    return note_count;
}

// Function for progress bar
void print_progress(size_t current, size_t total, const std::string& current_file_name) {
    if (total == 0) return;
    float percentage = 100.0f * static_cast<float>(current) / static_cast<float>(total);
    int bar_width = 40; // Adjusted for typical terminal widths
    int pos = static_cast<int>(bar_width * percentage / 100.0f);

    // Ensure progress bar doesn't exceed 100% visually due to floating point issues
    pos = std::min(pos, bar_width);
    if (current == total) { // Ensure it shows 100% and full bar at the end
        pos = bar_width;
        percentage = 100.0f;
    }

    std::cout << "\r["; // Fixed: Added \r for carriage return to overwrite previous line
    for (int i = 0; i < bar_width; ++i) {
        if (i < pos) std::cout << "=";
        else if (i == pos && current != total) std::cout << ">"; // Show '>' only if not complete
        else std::cout << " ";
    }
    std::cout << "] " << std::fixed << std::setprecision(1) << percentage << "% ("
              << current << "/" << total << ") ";
    
    int max_filename_len = 25; 
    std::string display_filename = current_file_name;
    if (current_file_name.length() > static_cast<size_t>(max_filename_len)) {
        display_filename = "..." + current_file_name.substr(current_file_name.length() - (max_filename_len - 3));
    }
    std::cout << std::left << std::setw(max_filename_len + 5) << ("Processing: " + display_filename) << std::flush; 
}

void print_usage(const char* prog_name) {
    std::cerr << "Usage: " << prog_name << " <input_directory> [-o <output_csv_file>]" << std::endl;
    std::cerr << "  <input_directory> : Directory to scan for MIDI files (required)." << std::endl;
    std::cerr << "  -o <output_csv_file> : Output CSV file path (optional, default: ./midi_note_counts.csv)." << std::endl;
    std::cerr << "  --output <output_csv_file> : Same as -o." << std::endl;
}

int main(int argc, char** argv) {
    // CLI::App app{"Process MIDI files in a directory, count notes, and save to CSV"}; // REMOVED

    std::string input_dir_path;
    std::string output_csv_path = "midi_note_counts.csv";

    // Manual argument parsing
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    input_dir_path = argv[1];

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "-o" || arg == "--output")) {
            if (i + 1 < argc) { // Make sure we have a value after -o/--output
                output_csv_path = argv[++i]; // Increment i and use next argument as path
            } else {
                std::cerr << "Error: " << arg << " option requires one argument." << std::endl;
                print_usage(argv[0]);
                return 1;
            }
        } else {
            std::cerr << "Error: Unknown option " << arg << std::endl;
            print_usage(argv[0]);
            return 1;
        }
    }

    // Check if input directory exists
    if (!fs::exists(input_dir_path) || !fs::is_directory(input_dir_path)) {
        std::cerr << "Error: Input directory \'" << input_dir_path << "\' does not exist or is not a directory." << std::endl;
        return 1;
    }


    // app.add_option("input_directory", input_dir_path, "Directory to scan for MIDI files") // REMOVED
    //     ->required()                                                                      // REMOVED
    //     ->check(CLI::ExistingDirectory);                                                  // REMOVED

    // app.add_option("-o,--output", output_csv_path, "Output CSV file path (default: ./midi_note_counts.csv)"); // REMOVED
    // CLI11_PARSE(app, argc, argv); // REMOVED

    fs::path root_dir(input_dir_path);
    std::vector<fs::path> midi_files;

    std::cout << "Scanning for MIDI files in \'" << fs::absolute(root_dir).string() << "\'..." << std::endl;
    try {
        for (const auto& entry : fs::recursive_directory_iterator(root_dir, fs::directory_options::skip_permission_denied)) {
            if (entry.is_regular_file()) {
                fs::path current_path = entry.path();
                std::string extension = current_path.extension().string();
                std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
                if (extension == ".mid" || extension == ".midi") {
                    midi_files.push_back(current_path);
                }
            }
        }
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Filesystem error while scanning directory: " << e.what() << std::endl;
        return 1;
    }

    if (midi_files.empty()) {
        std::cout << "No MIDI files found in the specified directory." << std::endl;
        return 0;
    }

    std::cout << "Found " << midi_files.size() << " MIDI files. Processing..." << std::endl;

    std::ofstream csv_file(output_csv_path);
    if (!csv_file.is_open()) {
        std::cerr << "Error: Could not open output CSV file: " << output_csv_path << std::endl;
        return 1;
    }

    csv_file << "AbsolutePath,RelativePath,FileName,NoteCount\n";

    size_t processed_count = 0;
    for (const auto& midi_path : midi_files) {
        processed_count++;
        std::string absolute_path_str = fs::absolute(midi_path).string();
        std::string relative_path_str;
        std::string filename_str = midi_path.filename().string();

        try {
            // Make relative path robust
            fs::path rel_path_obj = fs::relative(midi_path, root_dir);
            relative_path_str = rel_path_obj.empty() ? midi_path.filename().string() : rel_path_obj.string();
        } catch (const fs::filesystem_error& e) {
            // If relative path fails (e.g. different drives on windows for fs::relative, or other issues),
            // use filename as a fallback for relative, or just leave it based on absolute path logic.
            // For simplicity, let's use filename if relative computation fails.
            // std::cerr << "Warning: Could not compute relative path for " << absolute_path_str << ": " << e.what() << std::endl;
            relative_path_str = filename_str; 
        }
        
        // Normalize paths for CSV (replace backslashes with forward slashes for consistency)
        std::replace(absolute_path_str.begin(), absolute_path_str.end(), '\\', '/');
        std::replace(relative_path_str.begin(), relative_path_str.end(), '\\', '/');


        print_progress(processed_count, midi_files.size(), filename_str);

        try {
            // Read file into a byte vector
            std::vector<uint8_t> file_bytes = read_file_to_bytes(midi_path);
            std::span<const uint8_t> data_span(file_bytes.data(), file_bytes.size());

            // Use the generic parse function
            // Assuming DataFormat is symusic::DataFormat or symusic::io::DataFormat
            // Assuming parse is symusic::parse or symusic::io::parse
            const symusic::Score<symusic::Tick> score = 
                symusic::parse<symusic::DataFormat::MIDI, symusic::Score<symusic::Tick>>(
                    data_span, 
                    true // strict_mode
                );
            
            size_t notes = count_notes(score);
            csv_file << "\"" << absolute_path_str << "\",\""
                     << relative_path_str << "\",\""
                     << filename_str << "\","
                     << notes << "\n";
        } catch (const std::exception& e) {
            // Ensure progress bar is not overwritten by error message
            std::cout << std::endl; // Move to next line before printing error
            std::cerr << "Error processing file " << absolute_path_str << ": " << e.what() << std::endl;
            csv_file << "\"" << absolute_path_str << "\",\""
                     << relative_path_str << "\",\""
                     << filename_str << "\","
                     << "ERROR" << "\n";
            // Reprint progress bar if needed, or accept it's temporarily misaligned
            if (processed_count < midi_files.size()) { // if not the last file
                 print_progress(processed_count, midi_files.size(), "Error occurred...");
            }
        }
    }

    std::cout << std::endl << "Processing complete. " << processed_count << " files processed." << std::endl;
    std::cout << "Results saved to " << fs::absolute(fs::path(output_csv_path)).string() << std::endl;
    csv_file.close();

    return 0;
} 