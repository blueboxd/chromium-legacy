// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import android.view.ViewGroup;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.content_public.browser.WebContents;

/**
 * Implements navigation glow using compositor layer for tab switcher or rendered web contents.
 */
@JNINamespace("android")
class CompositorNavigationGlow extends NavigationGlow {
    private float mAccumulatedScroll;
    private long mNativeNavigationGlow;

    /**
     * @pararm parentView Parent view where the glow view gets attached to.
     * @pararm webContents WebContents whose native view's cc layer will be used
     *        for rendering glow effect.
     * @return NavigationGlow object for rendered pages
     */
    public CompositorNavigationGlow(ViewGroup parentView, WebContents webContents) {
        super(parentView);
        mNativeNavigationGlow =
                nativeInit(parentView.getResources().getDisplayMetrics().density, webContents);
    }

    @Override
    public void prepare(float startX, float startY) {
        nativePrepare(mNativeNavigationGlow, startX, startY, mParentView.getWidth(),
                mParentView.getHeight());
    }

    @Override
    public void onScroll(float xDelta) {
        if (mNativeNavigationGlow == 0) return;
        mAccumulatedScroll += xDelta;
        nativeOnOverscroll(mNativeNavigationGlow, mAccumulatedScroll, xDelta);
    }

    @Override
    public void release() {
        if (mNativeNavigationGlow == 0) return;
        nativeOnOverscroll(mNativeNavigationGlow, 0, 0);
        mAccumulatedScroll = 0;
    }

    @Override
    public void reset() {
        if (mNativeNavigationGlow == 0) return;
        nativeOnReset(mNativeNavigationGlow);
        mAccumulatedScroll = 0;
    }

    @Override
    public void destroy() {
        if (mNativeNavigationGlow == 0) return;
        nativeDestroy(mNativeNavigationGlow);
        mNativeNavigationGlow = 0;
    }

    private native long nativeInit(float dipScale, WebContents webContents);
    private native void nativePrepare(
            long nativeNavigationGlow, float startX, float startY, int width, int height);
    private native void nativeOnOverscroll(
            long nativeNavigationGlow, float accumulatedScroll, float delta);
    private native void nativeOnReset(long nativeNavigationGlow);
    private native void nativeDestroy(long nativeNavigationGlow);
}
