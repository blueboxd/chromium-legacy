// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.sharedstream.logging;

import android.graphics.Rect;
import android.view.View;
import android.view.ViewParent;
import android.view.ViewTreeObserver;

import androidx.annotation.Nullable;
import androidx.core.view.ViewCompat;

import org.chromium.chrome.browser.feed.library.api.host.config.Configuration;
import org.chromium.chrome.browser.feed.library.api.host.config.Configuration.ConfigKey;

/** Utility class to monitor the visibility of a view. */
public class VisibilityMonitor
        implements ViewTreeObserver.OnPreDrawListener, View.OnAttachStateChangeListener {
    public static final double DEFAULT_VIEW_LOG_THRESHOLD = .66;
    private static final String TAG = "VisibilityMonitor";

    private final View mView;
    private final double mViewLogThreshold;
    private boolean mVisible;
    @Nullable
    private VisibilityListener mVisibilityListener;

    public VisibilityMonitor(View view, Configuration configuration) {
        this.mView = view;
        this.mViewLogThreshold = configuration.getValueOrDefault(
                ConfigKey.VIEW_LOG_THRESHOLD, DEFAULT_VIEW_LOG_THRESHOLD);
    }

    public VisibilityMonitor(View view, double viewLogThreshold) {
        this.mView = view;
        this.mViewLogThreshold = viewLogThreshold;
    }

    @Override
    public boolean onPreDraw() {
        ViewParent parent = mView.getParent();
        if (parent != null) {
            Rect rect = new Rect(0, 0, mView.getWidth(), mView.getHeight());

            @SuppressWarnings("argument.type.incompatible")
            boolean childVisibleRectNotEmpty = parent.getChildVisibleRect(mView, rect, null);
            if (childVisibleRectNotEmpty
                    && rect.height() >= mViewLogThreshold * mView.getHeight()) {
                if (!mVisible) {
                    notifyListenerOnVisible();
                    mVisible = true;
                }
            } else {
                mVisible = false;
            }
        }
        return true;
    }

    @Override
    public void onViewAttachedToWindow(View v) {
        mView.getViewTreeObserver().addOnPreDrawListener(this);
    }

    @Override
    public void onViewDetachedFromWindow(View v) {
        mVisible = false;
        mView.getViewTreeObserver().removeOnPreDrawListener(this);
    }

    public void setListener(@Nullable VisibilityListener visibilityListener) {
        if (visibilityListener != null) {
            detach();
        }

        this.mVisibilityListener = visibilityListener;

        if (visibilityListener != null) {
            attach();
        }
    }

    private void attach() {
        mView.addOnAttachStateChangeListener(this);
        if (ViewCompat.isAttachedToWindow(mView)) {
            mView.getViewTreeObserver().addOnPreDrawListener(this);
        }
    }

    private void detach() {
        mView.removeOnAttachStateChangeListener(this);
        if (ViewCompat.isAttachedToWindow(mView)) {
            mView.getViewTreeObserver().removeOnPreDrawListener(this);
        }
    }

    private void notifyListenerOnVisible() {
        if (mVisibilityListener != null) {
            mVisibilityListener.onViewVisible();
        }
    }
}
