// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.host.stream;

/** Interface which is able to provide host configuration for default stream look and feel. */
public interface StreamConfiguration {
    /**
     * Returns the padding (in px) that appears to the start (left in LtR)) of each view in the
     * Stream.
     */
    int getPaddingStart();

    /**
     * Returns the padding (in px) that appears to the end (right in LtR)) of each view in the
     * Stream.
     */
    int getPaddingEnd();

    /** Returns the padding (in px) that appears before the first view in the Stream. */
    int getPaddingTop();

    /** Returns the padding (in px) that appears after the last view in the Stream. */
    int getPaddingBottom();
}
