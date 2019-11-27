// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet.host;

/** Lets Piet ask the host to format strings. */
public interface StringFormatter {
    /** Return a relative elapsed time string such as "8 minutes ago" or "1 day ago". */
    String getRelativeElapsedString(long elapsedTimeMillis);
}
