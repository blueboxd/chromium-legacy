// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Rect;
import android.view.View;
import android.view.ViewParent;
import android.widget.FrameLayout;

import org.chromium.base.MathUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.EventOffsetHandler;
import org.chromium.ui.resources.dynamics.ViewResourceAdapter;

/**
 * BrowserControlsContainerView is responsible for positioning and sizing a view from the client
 * that is anchored to the top or bottom of a Browser. BrowserControlsContainerView, uses
 * ViewResourceAdapter that is kept in sync with the contents of the view. ViewResourceAdapter is
 * used to keep a bitmap in sync with the contents of the view. The bitmap is placed in a cc::Layer
 * and the layer is shown while scrolling. ViewResourceAdapter is always kept in sync, as to do
 * otherwise results in a noticeable delay between when the scroll starts the content is available.
 *
 * There are many parts involved in orchestrating scrolling. The key things to know are:
 * . BrowserControlsContainerView (in native code) keeps a cc::Layer that shows a bitmap rendered by
 *   the view. The bitmap is updated anytime the view changes. This is done as otherwise there is a
 *   noticeable delay between when the scroll starts and the bitmap is available.
 * . When scrolling, the cc::Layer for the WebContents and BrowserControlsContainerView is moved.
 * . The size of the WebContents is only changed after the user releases a touch point. Otherwise
 *   the scrollbar bounces around.
 * . WebContentsDelegate::DoBrowserControlsShrinkRendererSize() only changes when the WebContents
 *   size change.
 * . WebContentsGestureStateTracker is responsible for determining when a scroll/touch is underway.
 * . ContentViewRenderView.Delegate is used to adjust the size of the webcontents when the
 *   controls are fully visible (and a scroll is not underway).
 *
 * The flow of this code is roughly:
 * . WebContentsGestureStateTracker generally detects a touch first
 * . TabImpl is notified and caches state.
 * . onTop/BottomControlsChanged() is called. This triggers hiding the real view and calling to
 *   native code to move the cc::Layers.
 * . the move continues.
 * . when the move completes and both WebContentsGestureStateTracker and
 * BrowserControlsContainerView no longer believe a move/gesture/scroll is underway the size of the
 * WebContents is adjusted (if necessary).
 */
@JNINamespace("weblayer")
class BrowserControlsContainerView extends FrameLayout {
    // ID used with ViewResourceAdapter.
    private static final int TOP_CONTROLS_ID = 1001;
    private static final int BOTTOM_CONTROLS_ID = 1002;

    private static final long SYSTEM_UI_VIEWPORT_UPDATE_DELAY_MS = 500;

    private final boolean mIsTop;

    private long mNativeBrowserControlsContainerView;

    private ViewResourceAdapter mViewResourceAdapter;

    // Last width/height of mView as sent to the native side.
    private int mLastWidth;
    private int mLastHeight;

    // view from the client.
    private View mView;

    private ContentViewRenderView mContentViewRenderView;
    private WebContents mWebContents;
    // Only created when hosting top-controls.
    private EventOffsetHandler mEventOffsetHandler;

    // Amount page content is offset along the y-axis. This is always 0 for bottom controls (because
    // bottom controls don't offset content). For top controls, a value of 0 means no offset, and
    // positive values indicate a portion of the top-control is shown. This value never goes
    // negative.
    private int mContentOffset;

    // Amount the control is offset along the y-axis from it's fully shown position. For
    // top-controls, the value ranges from 0 (completely shown) to -height (completely hidden). For
    // bottom-controls, the value ranges from 0 (completely shown) to height (completely hidden).
    private int mControlsOffset;

    // Set to true if |mView| is hidden because the user has scrolled or triggered some action such
    // that mView is not visible. While |mView| is not visible if this is true, the bitmap from
    // |mView| may be partially visible.
    private boolean mInScroll;

    private boolean mIsFullscreen;

    // Used to delay processing fullscreen requests.
    private Runnable mSystemUiFullscreenResizeRunnable;

    private final Listener mListener;

    public interface Listener {
        /**
         * Called when the browser-controls are either completely showing, or completely hiding.
         */
        public void onBrowserControlsCompletelyShownOrHidden();
    }

    // Used to  delay updating the image for the layer.
    private final Runnable mRefreshResourceIdRunnable = () -> {
        if (mView == null) return;
        BrowserControlsContainerViewJni.get().updateControlsResource(
                mNativeBrowserControlsContainerView);
    };

    BrowserControlsContainerView(Context context, ContentViewRenderView contentViewRenderView,
            Listener listener, boolean isTop) {
        super(context);
        mIsTop = isTop;
        mContentViewRenderView = contentViewRenderView;
        mNativeBrowserControlsContainerView =
                BrowserControlsContainerViewJni.get().createBrowserControlsContainerView(
                        this, contentViewRenderView.getNativeHandle(), isTop);
        mListener = listener;
    }

    public void setWebContents(WebContents webContents) {
        mWebContents = webContents;
        BrowserControlsContainerViewJni.get().setWebContents(
                mNativeBrowserControlsContainerView, webContents);

        cancelDelayedFullscreenRunnable();
        if (mWebContents == null) return;
        processFullscreenChanged(mWebContents.isFullscreenForCurrentTab());
    }

    public void destroy() {
        clearViewAndDestroyResources();
        BrowserControlsContainerViewJni.get().deleteBrowserControlsContainerView(
                mNativeBrowserControlsContainerView);
        cancelDelayedFullscreenRunnable();
    }

    public long getNativeHandle() {
        return mNativeBrowserControlsContainerView;
    }

    public EventOffsetHandler getEventOffsetHandler() {
        assert mIsTop;
        if (mEventOffsetHandler == null) {
            mEventOffsetHandler =
                    new EventOffsetHandler(new EventOffsetHandler.EventOffsetHandlerDelegate() {
                        @Override
                        public float getTop() {
                            return mContentOffset;
                        }

                        @Override
                        public void setCurrentTouchEventOffsets(float top) {
                            if (mWebContents != null) {
                                mWebContents.getEventForwarder().setCurrentTouchEventOffsets(
                                        0, top);
                            }
                        }
                    });
        }
        return mEventOffsetHandler;
    }

    /**
     * Returns the amount of vertical space to take away from the contents.
     */
    public int getContentHeightDelta() {
        if (mView == null) return 0;
        return mIsTop ? mContentOffset : getHeight() - mControlsOffset;
    }

    /**
     * Returns true if the browser control is visible to the user.
     */
    public boolean isControlVisible() {
        // Don't check the visibility of the View itself as it's hidden while scrolling.
        return mView != null && Math.abs(mControlsOffset) != getHeight();
    }

    /**
     * Returns true if the controls are completely shown or completely hidden. A return value
     * of false indicates the controls are being moved.
     */
    public boolean isCompletelyShownOrHidden() {
        return mControlsOffset == 0 || Math.abs(mControlsOffset) == getHeight();
    }

    /**
     * Sets the view from the client.
     */
    public void setView(View view) {
        if (mView == view) return;
        clearViewAndDestroyResources();
        mView = view;
        if (mView == null) {
            setControlsOffset(0, 0);
            return;
        }
        addView(view,
                new FrameLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT,
                        FrameLayout.LayoutParams.UNSPECIFIED_GRAVITY));
        if (getWidth() > 0 && getHeight() > 0) {
            view.layout(0, 0, getWidth(), getHeight());
            createAdapterAndLayer();
        }
        if (mIsFullscreen) hideControls();
    }

    /**
     * Does cleanup necessary when mView is to be removed.
     * In general prefer calling setView(). This is really an implementation detail of setView()
     * and destroy().
     */
    private void clearViewAndDestroyResources() {
        if (mView == null) return;
        if (mView.getParent() == this) removeView(mView);
        // TODO: need some sort of destroy to drop reference.
        mViewResourceAdapter = null;
        BrowserControlsContainerViewJni.get().deleteControlsLayer(
                mNativeBrowserControlsContainerView);
        mContentViewRenderView.getResourceManager().getDynamicResourceLoader().unregisterResource(
                getResourceId());
        mView = null;
        mLastWidth = mLastHeight = 0;
    }

    public View getView() {
        return mView;
    }

    /**
     * Called from ViewAndroidDelegate, see it for details.
     */
    public void onOffsetsChanged(int controlsOffsetY, int contentOffsetY) {
        if (mView == null) return;
        if (mIsFullscreen) return;
        if (controlsOffsetY == 0) {
            finishScroll(contentOffsetY);
            return;
        }
        if (!mInScroll) prepareForScroll();
        setControlsOffset(controlsOffsetY, contentOffsetY);
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
                    BrowserControlsContainerViewJni.get().setControlsSize(
                            mNativeBrowserControlsContainerView, mLastWidth, mLastHeight);
                }
            }
        }
    }

    @Override
    public void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        // Cancel the runnable when detached as calls to removeCallback() after this completes will
        // attempt to remove from the wrong handler.
        cancelDelayedFullscreenRunnable();
    }

    private void cancelDelayedFullscreenRunnable() {
        if (mSystemUiFullscreenResizeRunnable == null) return;
        removeCallbacks(mSystemUiFullscreenResizeRunnable);
        mSystemUiFullscreenResizeRunnable = null;
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
                getResourceId(), mViewResourceAdapter);
        // It's important that the layer is created immediately and always kept in sync with the
        // View. Creating the layer only when needed results in a noticeable delay between when
        // the layer is created and actually shown. Chrome for Android does the same thing.
        BrowserControlsContainerViewJni.get().createControlsLayer(
                mNativeBrowserControlsContainerView, getResourceId());
        mLastWidth = getWidth();
        mLastHeight = getHeight();
        BrowserControlsContainerViewJni.get().setControlsSize(
                mNativeBrowserControlsContainerView, mLastWidth, mLastHeight);
        if (mIsFullscreen) {
            setFullscreenControlsOffset();
        } else {
            setControlsOffset(0, mLastHeight);
        }
    }

    private void finishScroll(int contentOffsetY) {
        mInScroll = false;
        setControlsOffset(0, contentOffsetY);
        mContentViewRenderView.postOnAnimation(() -> showControls());
    }

    private void setControlsOffset(int controlsOffsetY, int contentOffsetY) {
        // This function is called asynchronously from the gpu, and may be out of sync with the
        // current values.
        if (mIsTop) {
            mControlsOffset = MathUtils.clamp(controlsOffsetY, -getHeight(), 0);
        } else {
            mControlsOffset = MathUtils.clamp(controlsOffsetY, 0, getHeight());
        }
        mContentOffset = MathUtils.clamp(contentOffsetY, 0, getHeight());
        if (isCompletelyShownOrHidden()) {
            mListener.onBrowserControlsCompletelyShownOrHidden();
        }
        if (mIsTop) {
            BrowserControlsContainerViewJni.get().setTopControlsOffset(
                    mNativeBrowserControlsContainerView, mControlsOffset, mContentOffset);
        } else {
            BrowserControlsContainerViewJni.get().setBottomControlsOffset(
                    mNativeBrowserControlsContainerView, mControlsOffset);
        }
    }

    private void prepareForScroll() {
        mInScroll = true;
        mContentViewRenderView.postOnAnimation(() -> hideControls());
    }

    private void hideControls() {
        if (mView != null) mView.setVisibility(View.INVISIBLE);
    }

    private void showControls() {
        if (mView != null) mView.setVisibility(View.VISIBLE);
    }

    @CalledByNative
    private void didToggleFullscreenModeForTab(final boolean isFullscreen) {
        // Delay hiding until after the animation. This comes from Chrome code.
        if (mSystemUiFullscreenResizeRunnable != null) {
            removeCallbacks(mSystemUiFullscreenResizeRunnable);
        }
        mSystemUiFullscreenResizeRunnable = () -> processFullscreenChanged(isFullscreen);
        long delay = isFullscreen ? SYSTEM_UI_VIEWPORT_UPDATE_DELAY_MS : 0;
        postDelayed(mSystemUiFullscreenResizeRunnable, delay);
    }

    private void processFullscreenChanged(boolean isFullscreen) {
        mSystemUiFullscreenResizeRunnable = null;
        if (mIsFullscreen == isFullscreen) return;
        mIsFullscreen = isFullscreen;
        if (mIsFullscreen) {
            hideControls();
            setFullscreenControlsOffset();
        } else {
            showControls();
            setControlsOffset(0, mIsTop ? mLastHeight : 0);
        }
    }

    private void setFullscreenControlsOffset() {
        assert mIsFullscreen;
        setControlsOffset(mIsTop ? -mLastHeight : mLastHeight, 0);
    }

    private int getResourceId() {
        return mIsTop ? TOP_CONTROLS_ID : BOTTOM_CONTROLS_ID;
    }

    @NativeMethods
    interface Natives {
        long createBrowserControlsContainerView(
                BrowserControlsContainerView view, long nativeContentViewRenderView, boolean isTop);
        void deleteBrowserControlsContainerView(long nativeBrowserControlsContainerView);
        void createControlsLayer(long nativeBrowserControlsContainerView, int id);
        void deleteControlsLayer(long nativeBrowserControlsContainerView);
        void setTopControlsOffset(
                long nativeBrowserControlsContainerView, int controlsOffsetY, int contentOffsetY);
        void setBottomControlsOffset(long nativeBrowserControlsContainerView, int controlsOffsetY);
        void setControlsSize(long nativeBrowserControlsContainerView, int width, int height);
        void updateControlsResource(long nativeBrowserControlsContainerView);
        void setWebContents(long nativeBrowserControlsContainerView, WebContents webContents);
    }
}
