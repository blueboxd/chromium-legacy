// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.host.logging;

import androidx.annotation.IntDef;

/**
 * IntDef representing the different types of spinners.
 *
 * <p>When adding new values, the value of {@link SpinnerType#NEXT_VALUE} should be used and
 * incremented. When removing values, {@link SpinnerType#NEXT_VALUE} should not be changed, and
 * those values should not be reused.
 */
@IntDef({
        SpinnerType.INITIAL_LOAD,
        SpinnerType.ZERO_STATE_REFRESH,
        SpinnerType.MORE_BUTTON,
        SpinnerType.SYNTHETIC_TOKEN,
        SpinnerType.INFINITE_FEED,
        SpinnerType.NEXT_VALUE,
})
public @interface SpinnerType {
    // Spinner shown on initial load of the Feed.
    int INITIAL_LOAD = 1;
    // Spinner shown when Feed is refreshed.
    int ZERO_STATE_REFRESH = 2;
    // Spinner shown when more button is clicked.
    int MORE_BUTTON = 3;
    // Spinner shown when a synthetic token is consumed.
    int SYNTHETIC_TOKEN = 4;
    // Spinner shown when a spinner is shown for loading the infinite feed.
    int INFINITE_FEED = 5;
    // The next value that should be used when adding additional values to the IntDef.
    int NEXT_VALUE = 6;
}
