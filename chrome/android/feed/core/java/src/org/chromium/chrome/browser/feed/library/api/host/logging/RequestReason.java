// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.host.logging;

import androidx.annotation.IntDef;

/**
 * The reason a request is being made.
 *
 * <p>When adding new values, the value of {@link RequestReason#NEXT_VALUE} should be used and
 * incremented. When removing values, {@link RequestReason#NEXT_VALUE} should not be changed, and
 * those values should not be reused.
 */
@IntDef({
        RequestReason.UNKNOWN,
        RequestReason.ZERO_STATE,
        RequestReason.HOST_REQUESTED,
        RequestReason.OPEN_WITH_CONTENT,
        RequestReason.MANUAL_CONTINUATION,
        RequestReason.AUTOMATIC_CONTINUATION,
        RequestReason.OPEN_WITHOUT_CONTENT,
        RequestReason.CLEAR_ALL,
        RequestReason.NEXT_VALUE,
})
public @interface RequestReason {
    // An unknown refresh reason.
    int UNKNOWN = 0;

    // Refresh triggered because the user manually hit the refresh button from the
    // zero-state.
    int ZERO_STATE = 1;

    // Refresh triggered because the host requested a refresh.
    int HOST_REQUESTED = 2;

    // Refresh triggered because there was stale content. The stale content was
    // shown while a background refresh would occur, which would be appended below
    // content the user had seen.
    int OPEN_WITH_CONTENT = 3;

    // Refresh triggered because the user tapped on a 'More' button.
    int MANUAL_CONTINUATION = 4;

    // Refresh triggered via automatically consuming a continuation token, without
    // showing the user a 'More' button.
    int AUTOMATIC_CONTINUATION = 5;

    // Refresh made when the Stream starts up and no content is showing.
    int OPEN_WITHOUT_CONTENT = 6;

    // Refresh made because the host requested a clear all.
    int CLEAR_ALL = 7;

    // The next value that should be used when adding additional values to the IntDef.
    int NEXT_VALUE = 8;
}
