// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet.host;

import android.graphics.Typeface;

import androidx.annotation.StringDef;

import org.chromium.base.Consumer;

/** Allows the host to provide Typefaces to Piet. */
public interface TypefaceProvider {
    /**
     * Allows the host to load a typeface Piet would otherwise not be able to access (ex. from
     * assets), and return it via the consumer. Piet will call this when the typeface is not one
     * Piet recognizes (as a default Android typeface). If host does not specially handle the
     * specified typeface, host can accept {@code null} through the consumer, and Piet will proceed
     * through its fallback typefaces.
     *
     * <p>Piet also expects the host to provide the Google Sans typeface, and will request it using
     * the {@link GoogleSansTypeface} StringDef. Piet will report errors if Google Sans is requested
     * and not found.
     *
     * @param typeface the String that the host uses to identify which typeface to load.
     * @param isItalic specifies whether the font should be italic. This is passed to the host
     *         instead
     *     of being handled by Piet so that the host can decide whether to just set the italic bits
     *     for the typeface or, if they want, return an entirely different font for the italic
     *     version.
     * @param consumer accepts the typeface once it's loaded, via consumer.accept(Typeface). If the
     *     host does not recognize the typeface name or fails to load the typeface, it should accept
     *     {@code null}. If the host does NOT call consumer.accept(null), Piet will not load the
     *     fallback font, and will just use the platform default.
     */
    void getTypeface(String typeface, boolean isItalic, Consumer<Typeface> consumer);

    /**
     * Strings the host can expect to receive to request Google Sans fonts. These are intentionally
     * decoupled from the Piet CommonTypeface enum so we can change the proto enum names without
     * breaking older hosts.
     */
    @StringDef({GoogleSansTypeface.UNDEFINED, GoogleSansTypeface.GOOGLE_SANS_REGULAR,
            GoogleSansTypeface.GOOGLE_SANS_MEDIUM})
    @interface GoogleSansTypeface {
        String UNDEFINED = "UNDEFINED";
        String GOOGLE_SANS_REGULAR = "GOOGLE_SANS_REGULAR";
        String GOOGLE_SANS_MEDIUM = "GOOGLE_SANS_MEDIUM";
    }
}
