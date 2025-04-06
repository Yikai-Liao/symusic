#pragma once
#ifndef SYMUSIC_TEST_CONVERSION_HELPER_HPP
#define SYMUSIC_TEST_CONVERSION_HELPER_HPP

#include "symusic.h"
#include <vector>

namespace symusic {

/**
 * Simple utility functions for time unit conversions.
 * These wrap the more complex conversion mechanisms of the library into simple functions
 * that can be easily tested.
 */

// Tick to Quarter conversion
inline Quarter::unit tick_to_quarter(Tick::unit tick_val, int tpq) {
    return static_cast<Quarter::unit>(tick_val) / static_cast<Quarter::unit>(tpq);
}

// Quarter to Tick conversion
inline Tick::unit quarter_to_tick(Quarter::unit quarter_val, int tpq) {
    return static_cast<Tick::unit>(std::round(quarter_val * static_cast<Quarter::unit>(tpq)));
}

// Quarter to Second conversion with constant tempo
inline Second::unit quarter_to_second(Quarter::unit quarter_val, int mspq) {
    return static_cast<Second::unit>(quarter_val * static_cast<Second::unit>(mspq) / 1000000.0);
}

// Second to Quarter conversion with constant tempo
inline Quarter::unit second_to_quarter(Second::unit second_val, int mspq) {
    return static_cast<Quarter::unit>(second_val * 1000000.0 / static_cast<Quarter::unit>(mspq));
}

// Tick to Second conversion with constant tempo
inline Second::unit tick_to_second(Tick::unit tick_val, int tpq, int mspq) {
    Quarter::unit quarter_val = tick_to_quarter(tick_val, tpq);
    return quarter_to_second(quarter_val, mspq);
}

// Second to Tick conversion with constant tempo
inline Tick::unit second_to_tick(Second::unit second_val, int tpq, int mspq) {
    Quarter::unit quarter_val = second_to_quarter(second_val, mspq);
    return quarter_to_tick(quarter_val, tpq);
}

// Quarter to Second conversion with variable tempo
inline Second::unit quarter_to_second(Quarter::unit quarter_val, const std::vector<Tempo<Quarter>>& tempos) {
    // Find the applicable tempo
    size_t i = 0;
    while (i < tempos.size() && tempos[i].time <= quarter_val) {
        i++;
    }
    i = (i == 0) ? 0 : i - 1;

    // Calculate time difference in quarters
    Quarter::unit quarter_diff = quarter_val - tempos[i].time;

    // Convert to seconds
    Second::unit base_time = 0;
    // If not the first tempo segment, calculate the base time
    if (i > 0) {
        for (size_t j = 0; j < i; j++) {
            Quarter::unit segment_duration =
                (j+1 < tempos.size()) ? (tempos[j+1].time - tempos[j].time) : 0;
            base_time += quarter_to_second(segment_duration, tempos[j].mspq);
        }
    }

    // Add the time from the current tempo segment
    return base_time + quarter_to_second(quarter_diff, tempos[i].mspq);
}

// Second to Quarter conversion with variable tempo
inline Quarter::unit second_to_quarter(Second::unit second_val, const std::vector<Tempo<Quarter>>& tempos) {
    // This is more complex since we need to determine which tempo segment the second falls into
    // First, calculate all the tempo segment boundaries in seconds
    std::vector<Second::unit> tempo_boundaries_seconds;
    tempo_boundaries_seconds.push_back(0);

    for (size_t i = 1; i < tempos.size(); i++) {
        Quarter::unit quarter_diff = tempos[i].time - tempos[i-1].time;
        Second::unit segment_duration = quarter_to_second(quarter_diff, tempos[i-1].mspq);
        tempo_boundaries_seconds.push_back(tempo_boundaries_seconds.back() + segment_duration);
    }

    // Find the applicable tempo segment
    size_t i = 0;
    while (i < tempo_boundaries_seconds.size() && tempo_boundaries_seconds[i] <= second_val) {
        i++;
    }
    i = (i == 0) ? 0 : i - 1;

    // Calculate time in quarters for this segment
    Second::unit second_diff = second_val - tempo_boundaries_seconds[i];
    Quarter::unit quarter_diff = second_to_quarter(second_diff, tempos[i].mspq);

    return tempos[i].time + quarter_diff;
}

// Tick to Second conversion with variable tempo
inline Second::unit tick_to_second(Tick::unit tick_val, int tpq, const std::vector<Tempo<Tick>>& tempos) {
    // Convert tempos from Tick to Quarter
    std::vector<Tempo<Quarter>> quarter_tempos;
    for (const auto& tempo : tempos) {
        Quarter::unit quarter_time = tick_to_quarter(tempo.time, tpq);
        quarter_tempos.push_back(Tempo<Quarter>(quarter_time, tempo.mspq));
    }

    // Convert tick to quarter
    Quarter::unit quarter_val = tick_to_quarter(tick_val, tpq);

    // Convert quarter to second with variable tempo
    return quarter_to_second(quarter_val, quarter_tempos);
}

} // namespace symusic

#endif // SYMUSIC_TEST_CONVERSION_HELPER_HPP
