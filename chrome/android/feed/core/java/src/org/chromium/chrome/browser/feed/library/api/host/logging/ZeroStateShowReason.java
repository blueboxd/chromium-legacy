// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.host.logging;

import android.support.annotation.IntDef;

/**
 * The reason a zero state is shown.
 *
 * <p>When adding new values, the value of {@link ZeroStateShowReason#NEXT_VALUE} should be used and
 * incremented. When removing values, {@link ZeroStateShowReason#NEXT_VALUE} should not be changed,
 * and those values should not be reused.
 */
@IntDef({
        ZeroStateShowReason.ERROR,
        ZeroStateShowReason.NO_CONTENT,
        ZeroStateShowReason.CONTENT_DISMISSED,
        ZeroStateShowReason.NO_CONTENT_FROM_CONTINUATION_TOKEN,
        ZeroStateShowReason.NEXT_VALUE,
})
public @interface ZeroStateShowReason {
    // Indicates the zero state was shown due to an error. This would most commonly happen if a
    // request fails.
    int ERROR = 0;

    // Indicates the zero state was shown because no content is available. This can happen if the
    // host does not schedule a refresh when the app is opened.
    int NO_CONTENT = 1;

    // Indicates the zero state was shown because all content, including any tokens, were dismissed.
    int CONTENT_DISMISSED = 2;

    // Indicates that the only content showing was a continuation token, and that token completed
    // with no additional content, resulting in no content showing. This shouldn't occur, as a token
    // should at least return another token.
    int NO_CONTENT_FROM_CONTINUATION_TOKEN = 3;

    // The next value that should be used when adding additional values to the IntDef.
    int NEXT_VALUE = 4;
}
