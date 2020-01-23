// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import android.content.Context;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.chromium.chrome.browser.compositor.CompositorViewHolder.TouchEventObserver;
import org.chromium.content_public.browser.WebContents;

/**
 * FrameLayout that supports side-wise slide gesture for history navigation.
 */
public class HistoryNavigationLayout
        extends FrameLayout implements TouchEventObserver, ViewGroup.OnHierarchyChangeListener {
    private GestureDetector mDetector;
    private NavigationHandler mNavigationHandler;
    private HistoryNavigationDelegate mDelegate;
    private WebContents mWebContents;
    private boolean mIsNativePage;
    private NavigationGlow mJavaGlowEffect;
    private NavigationGlow mCompositorGlowEffect;

    public HistoryNavigationLayout(Context context) {
        super(context);
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

    @Override
    public void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        if (mNavigationHandler != null) mNavigationHandler.reset();
    }

    @Override
    public boolean shouldInterceptTouchEvent(MotionEvent e) {
        // Forward gesture events only for native pages. Rendered pages receive events
        // from SwipeRefreshHandler.
        if (!mIsNativePage) return false;
        return mNavigationHandler != null && mNavigationHandler.isActive();
    }

    @Override
    public void handleTouchEvent(MotionEvent e) {
        if (mNavigationHandler != null && mIsNativePage) {
            mDetector.onTouchEvent(e);
            mNavigationHandler.onTouchEvent(e.getAction());
        }
    }

    /**
     * Initialize {@link NavigationHandler} object.
     * @param delegate {@link HistoryNavigationDelegate} providing info and a factory method.
     * @param webContents A new WebContents object.
     * @param isNativePage {@code true} if the content is a native page.
     */
    public void initNavigationHandler(
            HistoryNavigationDelegate delegate, WebContents webContents, boolean isNativePage) {
        if (mNavigationHandler == null) {
            mDetector = new GestureDetector(getContext(), new SideNavGestureListener());
            mNavigationHandler = new NavigationHandler(this, getContext(), this::getGlowEffect);
        }
        if (mDelegate != delegate) {
            mNavigationHandler.setDelegate(delegate);
            mDelegate = delegate;
        }

        if (isNativePage == mIsNativePage && mWebContents == webContents) return;
        if (mWebContents != webContents) resetCompositorGlow();
        mWebContents = webContents;
        mIsNativePage = isNativePage;
    }

    /**
     * Create {@link NavigationGlow} object lazily.
     */
    private NavigationGlow getGlowEffect() {
        if (mIsNativePage) {
            if (mJavaGlowEffect == null) mJavaGlowEffect = new AndroidUiNavigationGlow(this);
            return mJavaGlowEffect;
        } else {
            if (mCompositorGlowEffect == null) {
                mCompositorGlowEffect = new CompositorNavigationGlow(this, mWebContents);
            }
            return mCompositorGlowEffect;
        }
    }

    /**
     * Reset CompositorGlowEffect for new a WebContents. Destroy the current one
     * (for its native object) so it can be created again lazily.
     */
    private void resetCompositorGlow() {
        if (mCompositorGlowEffect != null) {
            mCompositorGlowEffect.destroy();
            mCompositorGlowEffect = null;
        }
    }

    /** @return Current {@link HistoryNavigationDelegate} object. */
    public HistoryNavigationDelegate getDelegate() {
        return mDelegate;
    }

    /** @return Current {@link NavigationHandler} object. */
    public NavigationHandler getNavigationHandler() {
        return mNavigationHandler;
    }

    private class SideNavGestureListener extends GestureDetector.SimpleOnGestureListener {
        @Override
        public boolean onDown(MotionEvent event) {
            return mNavigationHandler.onDown();
        }

        @Override
        public boolean onScroll(MotionEvent e1, MotionEvent e2, float distanceX, float distanceY) {
            // |onScroll| needs handling only after the state moved away from |NONE|.
            if (mNavigationHandler.isStopped()) return true;

            return mNavigationHandler.onScroll(
                    e1.getX(), distanceX, distanceY, e2.getX(), e2.getY());
        }
    }

    /**
     * Cancel navigation UI with animation effect.
     */
    public void release() {
        if (mNavigationHandler != null) mNavigationHandler.release(false);
    }

    /**
     * Performs cleanup upon destruction.
     */
    void destroy() {
        resetCompositorGlow();
        mDelegate = HistoryNavigationDelegate.DEFAULT;
        if (mNavigationHandler != null) {
            mNavigationHandler.setDelegate(mDelegate);
            mNavigationHandler = null;
        }
        mDetector = null;
        mWebContents = null;
    }
}
