// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Rect;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.view.ViewParent;
import android.widget.FrameLayout;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.resources.dynamics.ViewResourceAdapter;

/**
 * TopControlsContainerView is responsible for holding the top-view from the client. Further, it
 * has a ViewResourceAdapter that is kept in sync with the contents of the top-view.
 * ViewResourceAdapter is used to keep a bitmap in sync with the contents of the top-view. The
 * bitmap is placed in a cc::Layer and the layer is shown while scrolling the top-view.
 * ViewResourceAdapter is always kept in sync, as to do otherwise results in a noticeable delay
 * between when the scroll starts the content is available.
 */
@JNINamespace("weblayer")
class TopControlsContainerView extends FrameLayout {
    // ID used with ViewResourceAdapter.
    private static final int TOP_CONTROLS_ID = 1001;

    private long mNativeTopControlsContainerView;

    private ViewResourceAdapter mViewResourceAdapter;

    // Last width/height of mView as sent to the native side.
    private int mLastWidth;
    private int mLastHeight;

    // view from the client.
    private View mView;

    private ContentViewRenderView mContentViewRenderView;
    private WebContents mWebContents;
    private EventOffsetHandler mEventOffsetHandler;
    private int mTopContentOffset;

    // True if scrolling.
    private boolean mInTopControlsScroll;

    // Used to  delay updating the image for the layer.
    private final Runnable mRefreshResourceIdRunnable = () -> {
        nativeUpdateTopControlsResource(mNativeTopControlsContainerView);
    };

    TopControlsContainerView(
            Context context, WebContents webContents, ContentViewRenderView contentViewRenderView) {
        super(context);
        mContentViewRenderView = contentViewRenderView;
        mWebContents = webContents;
        mEventOffsetHandler =
                new EventOffsetHandler(new EventOffsetHandler.EventOffsetHandlerDelegate() {
                    @Override
                    public float getTop() {
                        return mTopContentOffset;
                    }

                    @Override
                    public void setCurrentTouchEventOffsets(float top) {
                        mWebContents.getEventForwarder().setCurrentTouchEventOffsets(0, top);
                    }
                });
        mNativeTopControlsContainerView = nativeCreateTopControlsContainerView(
                webContents, contentViewRenderView.getNativeHandle());
    }

    public void destroy() {
        setView(null);
        nativeDeleteTopControlsContainerView(mNativeTopControlsContainerView);
    }

    public long getNativeHandle() {
        return mNativeTopControlsContainerView;
    }

    public EventOffsetHandler getEventOffsetHandler() {
        return mEventOffsetHandler;
    }

    /**
     * Sets the view from the client.
     */
    public void setView(View view) {
        if (mView == view) return;
        if (mView != null) {
            if (mView.getParent() == this) removeView(mView);
            // TODO: need some sort of destroy to drop reference.
            mViewResourceAdapter = null;
            nativeDeleteTopControlsLayer(mNativeTopControlsContainerView);
            mContentViewRenderView.getResourceManager()
                    .getDynamicResourceLoader()
                    .unregisterResource(TOP_CONTROLS_ID);
        }
        mView = view;
        if (mView == null) return;
        addView(view,
                new FrameLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT,
                        FrameLayout.LayoutParams.UNSPECIFIED_GRAVITY));
        if (getWidth() > 0 && getHeight() > 0) {
            view.layout(0, 0, getWidth(), getHeight());
            createAdapterAndLayer();
        }
    }

    public View getView() {
        return mView;
    }

    /**
     * Called from ViewAndroidDelegate, see it for details.
     */
    public void onTopControlsChanged(int topControlsOffsetY, int topContentOffsetY) {
        if (topContentOffsetY == getHeight()) {
            finishTopControlsScroll(topContentOffsetY);
            return;
        }
        if (!mInTopControlsScroll) prepareForTopControlsScroll();
        setTopControlsOffset(topControlsOffsetY, topContentOffsetY);
    }

    @SuppressLint("NewApi") // Used on O+, invalidateChildInParent used for previous versions.
    @Override
    public void onDescendantInvalidated(View child, View target) {
        super.onDescendantInvalidated(child, target);
        invalidateViewResourceAdapter();
    }

    @Override
    public ViewParent invalidateChildInParent(int[] location, Rect dirty) {
        invalidateViewResourceAdapter();
        return super.invalidateChildInParent(location, dirty);
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        super.onLayout(changed, left, top, right, bottom);
        if (mView == null) return;
        int width = right - left;
        int height = bottom - top;
        if (height != mLastHeight || width != mLastWidth) {
            mLastWidth = width;
            mLastHeight = height;
            if (mLastWidth > 0 && mLastHeight > 0) {
                if (mViewResourceAdapter == null) {
                    createAdapterAndLayer();
                } else {
                    nativeSetTopControlsSize(
                            mNativeTopControlsContainerView, mLastWidth, mLastHeight);
                }
            }
        }
    }

    /**
     * Triggers copying the contents of mView to the offscreen buffer.
     */
    private void invalidateViewResourceAdapter() {
        if (mViewResourceAdapter == null || mView.getVisibility() != View.VISIBLE) return;
        mViewResourceAdapter.invalidate(null);
        removeCallbacks(mRefreshResourceIdRunnable);
        postOnAnimation(mRefreshResourceIdRunnable);
    }

    /**
     * Creates mViewResourceAdapter and the layer showing a copy of mView.
     */
    private void createAdapterAndLayer() {
        assert mViewResourceAdapter == null;
        assert mView != null;
        mViewResourceAdapter = new ViewResourceAdapter(mView);
        mContentViewRenderView.getResourceManager().getDynamicResourceLoader().registerResource(
                TOP_CONTROLS_ID, mViewResourceAdapter);
        // It's important that the layer is created immediately and always kept in sync with the
        // View. Creating the layer only when needed results in a noticeable delay between when
        // the layer is created and actually shown. Chrome for Android does the same thing.
        nativeCreateTopControlsLayer(mNativeTopControlsContainerView, TOP_CONTROLS_ID);
        mLastWidth = getWidth();
        mLastHeight = getHeight();
        nativeSetTopControlsSize(mNativeTopControlsContainerView, mLastWidth, mLastHeight);
    }

    private void finishTopControlsScroll(int topContentOffsetY) {
        mInTopControlsScroll = false;
        setTopControlsOffset(0, topContentOffsetY);
        mContentViewRenderView.postOnAnimation(() -> showTopControls());
    }

    private void setTopControlsOffset(int topControlsOffsetY, int topContentOffsetY) {
        mTopContentOffset = topContentOffsetY;
        nativeSetTopControlsOffset(
                mNativeTopControlsContainerView, topControlsOffsetY, topContentOffsetY);
    }

    private void prepareForTopControlsScroll() {
        mInTopControlsScroll = true;
        mContentViewRenderView.postOnAnimation(() -> hideTopControls());
    }

    private void hideTopControls() {
        if (mView != null) mView.setVisibility(View.INVISIBLE);
    }

    private void showTopControls() {
        if (mView != null) mView.setVisibility(View.VISIBLE);
    }

    private static native long nativeCreateTopControlsContainerView(
            WebContents webContents, long nativeContentViewRenderView);
    private native void nativeDeleteTopControlsContainerView(long nativeTopControlsContainerView);
    private native void nativeCreateTopControlsLayer(long nativeTopControlsContainerView, int id);
    private native void nativeDeleteTopControlsLayer(long nativeTopControlsContainerView);
    private native void nativeSetTopControlsOffset(
            long nativeTopControlsContainerView, int topControlsOffsetY, int topContentOffsetY);
    private native void nativeSetTopControlsSize(
            long nativeTopControlsContainerView, int width, int height);
    private native void nativeUpdateTopControlsResource(long nativeTopControlsContainerView);
}
