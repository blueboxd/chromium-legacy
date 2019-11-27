// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.common.logging;

import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

/** Date-formatting static methods. */
public final class StringFormattingUtils {
    // Do not instantiate
    private StringFormattingUtils() {}

    private static SimpleDateFormat sLogDateFormat;

    /** Formats {@code date} in the same format as used by logcat. */
    static synchronized String formatLogDate(Date date) {
        if (sLogDateFormat == null) {
            // Getting the date format is mildly expensive, so don't do it unless we need it.
            sLogDateFormat = new SimpleDateFormat("MM-dd HH:mm:ss.SSS", Locale.US);
        }
        return sLogDateFormat.format(date);
    }

    /** Formats a long as a Date */
    public static String formatLogDate(long timeMs) {
        return formatLogDate(new Date(timeMs));
    }
}
