// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet.host;

import android.graphics.Typeface;

import org.chromium.base.Consumer;

/** Typeface provider that does not provide any typefaces; for use as a default implementation. */
public class NullTypefaceProvider implements TypefaceProvider {
    @Override
    public void getTypeface(
            String typeface, boolean isItalic, Consumer</*@Nullable*/ Typeface> consumer) {
        consumer.accept(null);
    }
}
