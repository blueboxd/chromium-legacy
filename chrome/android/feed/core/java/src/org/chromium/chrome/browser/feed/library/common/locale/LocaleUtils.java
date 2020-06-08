// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.common.locale;

import android.annotation.TargetApi;
import android.content.Context;
import android.os.Build;

import java.util.Locale;

/** Utility methods for retrieving information about device language. */
public final class LocaleUtils {
    private LocaleUtils() {}

    /** Returns the top user specified locale. */
    // TODO: Look into allowing multiple languages to be returned.
    @TargetApi(Build.VERSION_CODES.N)
    public static Locale getLocale(Context context) {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.N
                ? context.getResources().getConfiguration().getLocales().get(0)
                : context.getResources().getConfiguration().locale;
    }

    /** Returns a string which represents the top locale and region of the device. */
    public static String getLanguageTag(Context context) {
        return getLocale(context).toLanguageTag();
    }
}
