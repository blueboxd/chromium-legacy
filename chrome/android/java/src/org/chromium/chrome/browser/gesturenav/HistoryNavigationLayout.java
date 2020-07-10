// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.chromium.base.supplier.Supplier;
import org.chromium.content_public.browser.WebContents;

/**
 * FrameLayout that supports side-wise slide gesture for history navigation.
 */
public class HistoryNavigationLayout
        extends FrameLayout implements ViewGroup.OnHierarchyChangeListener {
    private final Supplier<Boolean> mIsNativePage;
    private WebContents mWebContents;
    private NavigationGlow mJavaGlowEffect;
    private NavigationGlow mCompositorGlowEffect;

    public HistoryNavigationLayout(Context context, Supplier<Boolean> isNativePage) {
        super(context);
        mIsNativePage = isNativePage;
        setOnHierarchyChangeListener(this);
    }

    @Override
    public void onChildViewAdded(View parent, View child) {
        if (getVisibility() != View.VISIBLE) setVisibility(View.VISIBLE);
    }

    @Override
    public void onChildViewRemoved(View parent, View child) {
        // TODO(jinsukkim): Replace INVISIBLE with GONE to avoid performing layout/measurements.
        if (getChildCount() == 0) setVisibility(View.INVISIBLE);
    }

    /**
     * Create {@link NavigationGlow} object lazily.
     */
    NavigationGlow getGlowEffect() {
        if (mIsNativePage.get()) {
            if (mJavaGlowEffect == null) mJavaGlowEffect = new AndroidUiNavigationGlow(this);
            return mJavaGlowEffect;
        } else {
            // TODO(crbug.com/1102275): Investigate when this is called with nulled mWebContents.
            if (mCompositorGlowEffect == null && mWebContents != null) {
                mCompositorGlowEffect = new CompositorNavigationGlow(this, mWebContents);
            }
            return mCompositorGlowEffect;
        }
    }

    /**
     * Reset CompositorGlowEffect for new a WebContents. Destroy the current one
     * (for its native object) so it can be created again lazily.
     */
    void resetCompositorGlow(WebContents webContents) {
        if (mWebContents == webContents) return;
        if (mCompositorGlowEffect != null) {
            mCompositorGlowEffect.destroy();
            mCompositorGlowEffect = null;
        }
        mWebContents = webContents;
    }

    /**
     * Performs cleanup upon destruction.
     */
    void destroy() {
        resetCompositorGlow(null);
        mWebContents = null;
    }
}
