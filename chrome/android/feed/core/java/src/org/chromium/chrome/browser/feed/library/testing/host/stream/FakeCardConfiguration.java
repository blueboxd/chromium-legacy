// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.testing.host.stream;

import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;

import org.chromium.chrome.browser.feed.library.api.host.stream.CardConfiguration;

/** Fake for {@link CardConfiguration}. */
public class FakeCardConfiguration implements CardConfiguration {
    private static ColorDrawable sColorDrawable;

    private int mBottomMargin = 1;
    private int mStartMargin = 2;
    private int mEndMargin = 3;

    @Override
    public int getDefaultCornerRadius() {
        return 0;
    }

    @Override
    public Drawable getCardBackground() {
        if (sColorDrawable == null) {
            sColorDrawable = new ColorDrawable(Color.RED);
        }
        return sColorDrawable;
    }

    @Override
    public int getCardBottomMargin() {
        return mBottomMargin;
    }

    @Override
    public int getCardStartMargin() {
        return mStartMargin;
    }

    @Override
    public int getCardEndMargin() {
        return mEndMargin;
    }

    public void setCardStartMargin(int cardStartMargin) {
        this.mStartMargin = cardStartMargin;
    }

    public void setCardEndMargin(int cardEndMargin) {
        this.mEndMargin = cardEndMargin;
    }

    public void setCardBottomMargin(int cardBottomMargin) {
        this.mBottomMargin = cardBottomMargin;
    }
}
