// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.host.logging;

import androidx.annotation.IntDef;

/**
 * IntDef representing the different results of requesting sessions.
 *
 * <p>When adding new values, the value of {@link SessionEvent#NEXT_VALUE} should be used and
 * incremented. When removing values, {@link SessionEvent#NEXT_VALUE} should not be changed, and
 * those values should not be reused.
 */
@IntDef({
        SessionEvent.STARTED,
        SessionEvent.FINISHED_IMMEDIATELY,
        SessionEvent.ERROR,
        SessionEvent.USER_ABANDONED,
        SessionEvent.NEXT_VALUE,
})
public @interface SessionEvent {
    // Indicates that a session was successfully started when requested.
    int STARTED = 0;

    // Indicates that a session was immediately finished when requested.
    int FINISHED_IMMEDIATELY = 1;

    // Indicates that a session failed when requested.
    int ERROR = 2;

    // Indicates the session requested was abandoned by the user taking action.
    // Note: some of these events will be dropped, as they will be reported in
    // onDestroy(), which may not be called in all instances.
    int USER_ABANDONED = 3;

    // The next value that should be used when adding additional values to the IntDef.
    int NEXT_VALUE = 4;
}
