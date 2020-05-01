// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

/**
 * Exposes methods to report suggestions related events, for UMA or Fetch scheduling purposes.
 */
public interface SuggestionsEventReporter {
    /**
     * Notifies about new suggestions surfaces being opened: the bottom sheet opening or a NTP being
     * created.
     */
    void onSurfaceOpened();

    /**
     * Tracks per-page-load metrics for content suggestions.
     * @param categories The categories of content suggestions.
     * @param suggestionsPerCategory The number of content suggestions in each category.
     * @param isCategoryVisible A boolean array representing which categories (possibly empty) are
     *                          visible in the UI.
     */
    void onPageShown(int[] categories, int[] suggestionsPerCategory, boolean[] isCategoryVisible);
}
