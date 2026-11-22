// The loader resolves the entire API even when a test renders no notes.
#include <atomic>
#include <chrono>
#include <thread>

namespace {

struct FluidSettings {};
struct FluidSynth {};

std::atomic_flag initializing = ATOMIC_FLAG_INIT;
std::atomic<int> sample_cache_operations = 0;

}   // namespace

extern "C" {

void fluid_version(int* major, int* minor, int* patch) {
    *major = 2;
    *minor = 6;
    *patch = 2;
}

FluidSettings* new_fluid_settings() { return new FluidSettings; }
void           delete_fluid_settings(FluidSettings* settings) { delete settings; }
int            fluid_settings_setnum(FluidSettings*, const char*, double) { return 0; }
int            fluid_settings_setint(FluidSettings*, const char*, int) { return 0; }

FluidSynth* new_fluid_synth(FluidSettings*) {
    if (initializing.test_and_set()) { return nullptr; }
    // Give another render a chance to enter while the global tables are being built.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    initializing.clear();
    return new FluidSynth;
}

void delete_fluid_synth(FluidSynth* synth) {
    ++sample_cache_operations;
    // Widen the window in which a load can overlap sample-cache teardown.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    delete synth;
    --sample_cache_operations;
}

int fluid_synth_sfload(FluidSynth*, const char*, int) {
    const bool overlaps_teardown = ++sample_cache_operations > 1;
    --sample_cache_operations;
    return overlaps_teardown ? -1 : 0;
}
int fluid_synth_set_channel_type(FluidSynth*, int, int) { return 0; }
int fluid_synth_program_select(FluidSynth*, int, int, int, int) { return 0; }
int fluid_synth_noteon(FluidSynth*, int, int, int) { return 0; }
int fluid_synth_noteoff(FluidSynth*, int, int) { return 0; }
int fluid_synth_cc(FluidSynth*, int, int, int) { return 0; }
int fluid_synth_pitch_bend(FluidSynth*, int, int) { return 0; }
int fluid_synth_all_notes_off(FluidSynth*, int) { return 0; }
int fluid_synth_get_internal_bufsize(FluidSynth*) { return 64; }
int fluid_synth_set_interp_method(FluidSynth*, int, int) { return 0; }
int fluid_synth_write_float(FluidSynth*, int, void*, int, int, void*, int, int) { return 0; }

}   // extern "C"
