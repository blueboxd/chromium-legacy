// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.testing.host.stream;

import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;

import com.google.android.libraries.feed.api.host.stream.CardConfiguration;

/** Fake for {@link CardConfiguration}. */
public class FakeCardConfiguration implements CardConfiguration {
    private static final ColorDrawable COLOR_DRAWABLE = new ColorDrawable(Color.RED);

    private int bottomMargin = 1;
    private int startMargin = 2;
    private int endMargin = 3;

    @Override
    public int getDefaultCornerRadius() {
        return 0;
    }

    @Override
    public Drawable getCardBackground() {
        return COLOR_DRAWABLE;
    }

    @Override
    public int getCardBottomMargin() {
        return bottomMargin;
    }

    @Override
    public int getCardStartMargin() {
        return startMargin;
    }

    @Override
    public int getCardEndMargin() {
        return endMargin;
    }

    public void setCardStartMargin(int cardStartMargin) {
        this.startMargin = cardStartMargin;
    }

    public void setCardEndMargin(int cardEndMargin) {
        this.endMargin = cardEndMargin;
    }

    public void setCardBottomMargin(int cardBottomMargin) {
        this.bottomMargin = cardBottomMargin;
    }
}
