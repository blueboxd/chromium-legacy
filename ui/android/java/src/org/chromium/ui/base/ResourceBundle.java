// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

import java.util.Arrays;

/**
 * This class provides the resource bundle related methods for the native
 * library.
 *
 * IMPORTANT: Clients that use {@link ResourceBundle} and/or
 * {@link org.chromium.ui.resources.ResourceExtractor} MUST call either
 * {@link ResourceBundle#setAvailablePakLocales(String[], String[])} or
 * {@link ResourceBundle#setNoAvailableLocalePaks()} before calling the getters in this class.
 */
@JNINamespace("ui")
public final class ResourceBundle {
    private static String[] sCompressedLocales;
    private static String[] sUncompressedLocales;

    private ResourceBundle() {}

    /**
     * Called when there are no locale pak files available.
     */
    public static void setNoAvailableLocalePaks() {
        assert sCompressedLocales == null && sUncompressedLocales == null;
        sCompressedLocales = new String[] {};
        sUncompressedLocales = new String[] {};
    }

    /**
     * Sets the available compressed and uncompressed locale pak files.
     * @param compressed Locales that have compressed pak files.
     * @param uncompressed Locales that have uncompressed pak files.
     */
    public static void setAvailablePakLocales(String[] compressed, String[] uncompressed) {
        assert sCompressedLocales == null && sUncompressedLocales == null;
        sCompressedLocales = compressed;
        sUncompressedLocales = uncompressed;
    }

    /**
     * Return the array of locales that have compressed pak files. Do not modify the array.
     * @return The locales that have compressed pak files.
     */
    public static String[] getAvailableCompressedPakLocales() {
        assert sCompressedLocales != null;
        return sCompressedLocales;
    }

    @CalledByNative
    private static String getLocalePakResourcePath(String locale) {
        assert sUncompressedLocales != null;
        if (Arrays.binarySearch(sUncompressedLocales, locale) >= 0) {
            return "assets/stored-locales/" + locale + ".pak";
        }
        return null;
    }
}
