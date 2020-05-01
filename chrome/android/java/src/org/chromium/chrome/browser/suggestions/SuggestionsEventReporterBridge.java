// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import org.chromium.base.annotations.NativeMethods;

/**
 * Exposes methods to report suggestions related events, for UMA or Fetch scheduling purposes.
 */
public class SuggestionsEventReporterBridge implements SuggestionsEventReporter {
    @Override
    public void onSurfaceOpened() {
        SuggestionsEventReporterBridgeJni.get().onSurfaceOpened();
    }

    @Override
    public void onPageShown(
            int[] categories, int[] suggestionsPerCategory, boolean[] isCategoryVisible) {
        SuggestionsEventReporterBridgeJni.get().onPageShown(
                categories, suggestionsPerCategory, isCategoryVisible);
    }

    public static void onActivityWarmResumed() {
        SuggestionsEventReporterBridgeJni.get().onActivityWarmResumed();
    }

    public static void onColdStart() {
        SuggestionsEventReporterBridgeJni.get().onColdStart();
    }

    // TODO(https://crbug.com/1069183): clean-up natives as part of C++ clean-up effort.
    @NativeMethods
    interface Natives {
        void onPageShown(
                int[] categories, int[] suggestionsPerCategory, boolean[] isCategoryVisible);
        void onSuggestionShown(int globalPosition, int category, int positionInCategory,
                long publishTimestampMs, float score, long fetchTimestampMs);
        void onSuggestionOpened(int globalPosition, int category, int categoryIndex,
                int positionInCategory, long publishTimestampMs, float score,
                int windowOpenDisposition, boolean isPrefetched);
        void onSuggestionMenuOpened(int globalPosition, int category, int positionInCategory,
                long publishTimestampMs, float score);
        void onMoreButtonShown(int category, int position);
        void onMoreButtonClicked(int category, int position);
        void onSurfaceOpened();
        void onActivityWarmResumed();
        void onColdStart();
        void onSuggestionTargetVisited(int category, long visitTimeMs);
    }
}
