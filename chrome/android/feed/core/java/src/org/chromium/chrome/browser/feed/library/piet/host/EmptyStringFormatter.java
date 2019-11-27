// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet.host;

/** {@link StringFormatter} that returns empty string, for hosts that do not use this capability. */
public class EmptyStringFormatter implements StringFormatter {
    @Override
    public String getRelativeElapsedString(long elapsedTimeMillis) {
        return "";
    }
}
