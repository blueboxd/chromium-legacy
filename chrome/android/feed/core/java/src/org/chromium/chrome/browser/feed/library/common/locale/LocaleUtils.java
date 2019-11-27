// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.common.locale;

import android.annotation.TargetApi;
import android.content.Context;
import android.os.Build;
import android.os.Build.VERSION;
import android.support.annotation.VisibleForTesting;
import android.text.TextUtils;

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
        return getLanguageTag(getLocale(context));
    }

    /** Returns a string representation of a specified locale and region. */
    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    @VisibleForTesting
    static String getLanguageTag(Locale locale) {
        return (VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP)
                ? locale.toLanguageTag()
                : getLanguageTagPreLollipop(locale);
    }

    private static String getLanguageTagPreLollipop(Locale locale) {
        StringBuilder sb = new StringBuilder();
        String language = locale.getLanguage();
        if (TextUtils.isEmpty(language)) {
            language = "und";
        }
        sb.append(language);
        String variant = locale.getVariant();
        if (!TextUtils.isEmpty(variant)) {
            sb.append("-").append(variant);
        }
        String country = locale.getCountry();
        if (!TextUtils.isEmpty(country)) {
            sb.append("-").append(country);
        }
        return sb.toString();
    }
}
