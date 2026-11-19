// Catch2 entry point: includes every C++ test file into one executable.

// Define the Catch2 main entry point
#define CATCH_CONFIG_MAIN
#include <catch2/catch_all.hpp>

// Include all test header files
#include "test_io.hpp"
#include "test_common_io.hpp"
#include "test_time_events.hpp"
#include "test_checked_time.hpp"
#include "test_track_score.hpp"
#include "test_conversion.hpp"
#include "test_midi_io.hpp"
// Temporarily commented out problematic tests
#include "test_pianoroll.hpp"
#include "test_synth.hpp"
#include "test_repr.hpp"
#include "test_zpp.hpp"

// Include new detailed test files
#include "test_score.hpp"
#include "test_track.hpp"
#include "test_conversion_detailed.hpp"
#include "test_event_detailed.hpp"
#include "test_repr_detailed.hpp"
