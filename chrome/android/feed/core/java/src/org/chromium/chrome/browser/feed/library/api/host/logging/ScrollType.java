// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.host.logging;

import androidx.annotation.IntDef;

/** IntDef representing the different types of scrolls. */
@IntDef({ScrollType.UNKNOWN, ScrollType.STREAM_SCROLL, ScrollType.NEXT_VALUE})
public @interface ScrollType {
    // Type of scroll that occurs
    int UNKNOWN = 0;
    // Scroll the stream of cards
    int STREAM_SCROLL = 1;
    int NEXT_VALUE = 2;
}
