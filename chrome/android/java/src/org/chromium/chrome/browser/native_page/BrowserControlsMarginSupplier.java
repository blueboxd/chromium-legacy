// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.native_page;

import android.graphics.Rect;

import org.chromium.base.supplier.DestroyableObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.fullscreen.BrowserControlsStateProvider;

/**
 * An implementation of {@link DestroyableObservableSupplier} that monitors changes to browser
 * controls and updates top/bottom margins as needed.
 */
class BrowserControlsMarginSupplier extends ObservableSupplierImpl<Rect>
        implements BrowserControlsStateProvider.Observer, DestroyableObservableSupplier<Rect> {
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;

    BrowserControlsMarginSupplier(BrowserControlsStateProvider browserControlsStateProvider) {
        mBrowserControlsStateProvider = browserControlsStateProvider;
        mBrowserControlsStateProvider.addObserver(this);
        updateMargins();
    }

    @Override
    public void destroy() {
        mBrowserControlsStateProvider.removeObserver(this);
    }

    @Override
    public void onControlsOffsetChanged(int topOffset, int topControlsMinHeightOffset,
            int bottomOffset, int bottomControlsMinHeightOffset, boolean needsAnimate) {
        updateMargins();
    }

    @Override
    public void onTopControlsHeightChanged(int topControlsHeight, int topControlsMinHeight) {
        updateMargins();
    }

    @Override
    public void onBottomControlsHeightChanged(
            int bottomControlsHeight, int bottomControlsMinHeight) {
        updateMargins();
    }

    private void updateMargins() {
        int topMargin = mBrowserControlsStateProvider.getTopControlsHeight()
                + mBrowserControlsStateProvider.getTopControlOffset();
        int bottomMargin = mBrowserControlsStateProvider.getBottomControlsHeight()
                - mBrowserControlsStateProvider.getBottomControlOffset();
        super.set(new Rect(0, topMargin, 0, bottomMargin));
    }
}
