// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import org.chromium.base.metrics.RecordHistogram;

import java.util.concurrent.TimeUnit;

/**
 * This class collects a variety of different Suggestions related metrics.
 */
class SuggestionsMetrics {
    // The expected range for measure and layout durations.
    // If the combination of (N*Create)+Measure+Layout, where N is a maximum number of suggestions
    // exceeds:
    // - 16'000us (16ms), the suggestion list will take more than a single frame on modern devices,
    // - 33'000us (33ms), the suggestion list will take more than a single frame on all devices.
    // We expect the reported values to be within 2milliseconds range.
    // The upper range here is deliberately smaller than the limits mentioned above; any stage
    // taking more time than the limit chosen here is an indication of a problem.
    private static final long MIN_HISTOGRAM_DURATION_US = 100;
    private static final long MAX_HISTOGRAM_DURATION_US = TimeUnit.MILLISECONDS.toMicros(10);
    private static final int NUM_DURATION_BUCKETS = 100;

    /**
     * Measure duration between two timestamps in hundreds of microseconds.
     *
     * @param startTimeNs Operation start timestamp (nanoseconds).
     * @param endTimeNs Operation end time (nanoseconds).
     * @return Duration in hundreds of microseconds or -1, if timestamps were invalid.
     */
    private static final long getDurationInHundredsOfMicroseconds(
            long startTimeNs, long endTimeNs) {
        if (startTimeNs == -1 || endTimeNs == -1) return -1;
        return TimeUnit.NANOSECONDS.toMicros(endTimeNs - startTimeNs);
    }

    /**
     * Record how long the Suggestion List needed to layout its content and children.
     *
     * @param startTimeNs Operation start timestamp (nanoseconds).
     * @param endTimeNs Operation end timestamp (nanoseconds).
     */
    static final void recordSuggestionListLayoutTime(long startTimeNs, long endTimeNs) {
        final long duration = getDurationInHundredsOfMicroseconds(startTimeNs, endTimeNs);
        if (duration < 0) return;
        RecordHistogram.recordCustomTimesHistogram("Android.Omnibox.SuggestionList.LayoutTime",
                duration, MIN_HISTOGRAM_DURATION_US, MAX_HISTOGRAM_DURATION_US,
                NUM_DURATION_BUCKETS);
    }

    /**
     * Record how long the Suggestion List needed to measure its content and children.
     *
     * @param startTimeNs Operation start timestamp (nanoseconds).
     * @param endTimeNs Operation end timestamp (nanoseconds).
     */
    static final void recordSuggestionListMeasureTime(long startTimeNs, long endTimeNs) {
        final long duration = getDurationInHundredsOfMicroseconds(startTimeNs, endTimeNs);
        if (duration < 0) return;
        RecordHistogram.recordCustomTimesHistogram("Android.Omnibox.SuggestionList.MeasureTime",
                duration, MIN_HISTOGRAM_DURATION_US, MAX_HISTOGRAM_DURATION_US,
                NUM_DURATION_BUCKETS);
    }

    /**
     * Record the amount of time needed to create a new suggestion view.
     * The type of view is intentionally ignored for this call.
     *
     * @param startTimeNs Operation start timestamp (nanoseconds).
     * @param endTimeNs Operation end timestamp (nanoseconds).
     */
    static final void recordSuggestionViewCreateTime(long startTimeNs, long endTimeNs) {
        final long duration = getDurationInHundredsOfMicroseconds(startTimeNs, endTimeNs);
        if (duration < 0) return;
        RecordHistogram.recordCustomTimesHistogram("Android.Omnibox.SuggestionView.CreateTime",
                duration, MIN_HISTOGRAM_DURATION_US, MAX_HISTOGRAM_DURATION_US,
                NUM_DURATION_BUCKETS);
    }

    /**
     * Record whether suggestion view was successfully reused.
     *
     * @param reused Whether suggestion view was reused.
     */
    static final void recordSuggestionViewReused(boolean reused) {
        RecordHistogram.recordBooleanHistogram("Android.Omnibox.SuggestionView.Reused", reused);
    }
}
