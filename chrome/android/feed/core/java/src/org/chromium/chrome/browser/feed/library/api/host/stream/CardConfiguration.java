// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.host.stream;

import android.graphics.drawable.Drawable;

/** Class which is able to provide host configuration for default card look and feel. */
// TODO: Look into allowing server configuration of this.
public interface CardConfiguration {
    int getDefaultCornerRadius();

    Drawable getCardBackground();

    /** Returns the amount of margin (in px) at the end of a card in the Stream */
    int getCardBottomMargin();

    /** Returns the amount of margin (in px) to the left (in LTR locales) of cards. */
    int getCardStartMargin();

    /** Returns the amount of margin (in px) to the right (in LTR locales) of cards. */
    int getCardEndMargin();
}
