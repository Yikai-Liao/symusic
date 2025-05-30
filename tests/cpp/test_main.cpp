//
// Created by lyk on 24-5-23.
// The main entry of the test program using Catch2
// Include unit test in different files into one file
//

// Define the Catch2 main entry point
#define CATCH_CONFIG_MAIN
#include <catch2/catch_all.hpp>

// Include all test header files
#include "test_io.hpp"
#include "test_common_io.hpp"
#include "test_time_events.hpp"
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
