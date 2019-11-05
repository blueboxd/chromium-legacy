// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.content.Context;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.webkit.ValueCallback;
import android.widget.FrameLayout;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;

/**
 * BrowserFragmentViewController controls the set of Views needed to show the
 * WebContents.
 */
@JNINamespace("weblayer")
public final class BrowserFragmentViewController
        implements TopControlsContainerView.Listener,
                   WebContentsGestureStateTracker.OnGestureStateChangedListener {
    private final ContentViewRenderView mContentViewRenderView;
    private final ContentView mContentView;
    // Child of mContentViewRenderView, holds top-view from client.
    private final TopControlsContainerView mTopControlsContainerView;

    private BrowserControllerImpl mBrowserController;

    private WebContentsGestureStateTracker mGestureStateTracker;

    /**
     * The value of mCachedDoBrowserControlsShrinkRendererSize is set when
     * WebContentsGestureStateTracker begins a gesture. This is necessary as the values should only
     * change once a gesture is no longer under way.
     */
    private boolean mCachedDoBrowserControlsShrinkRendererSize;

    public BrowserFragmentViewController(Context context, WindowAndroid windowAndroid) {
        mContentViewRenderView = new ContentViewRenderView(context);

        mContentViewRenderView.onNativeLibraryLoaded(
                windowAndroid, ContentViewRenderView.MODE_SURFACE_VIEW);
        mTopControlsContainerView =
                new TopControlsContainerView(context, mContentViewRenderView, this);
        mContentView = ContentView.createContentView(
                context, mTopControlsContainerView.getEventOffsetHandler());
        ViewAndroidDelegate viewAndroidDelegate = new ViewAndroidDelegate(mContentView) {
            @Override
            public void onTopControlsChanged(int topControlsOffsetY, int topContentOffsetY) {
                mTopControlsContainerView.onTopControlsChanged(
                        topControlsOffsetY, topContentOffsetY);
            }
        };
        mContentViewRenderView.addView(mContentView,
                new FrameLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT,
                        FrameLayout.LayoutParams.UNSPECIFIED_GRAVITY));
        mContentView.addView(mTopControlsContainerView,
                new FrameLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT,
                        Gravity.FILL_HORIZONTAL | Gravity.TOP));
    }

    public void destroy() {
        setActiveBrowserController(null);
        mTopControlsContainerView.destroy();
        mContentViewRenderView.destroy();
    }

    /** Returns top-level View this Controller works with */
    public View getView() {
        return mContentViewRenderView;
    }

    public ViewGroup getContentView() {
        return mContentView;
    }

    public void setActiveBrowserController(BrowserControllerImpl browserController) {
        if (browserController == mBrowserController) return;

        if (mBrowserController != null) {
            mBrowserController.onDidLoseActive();
            // WebContentsGestureStateTracker is relatively cheap, easier to destroy rather than
            // update WebContents.
            mGestureStateTracker.destroy();
            mGestureStateTracker = null;
        }
        mBrowserController = browserController;
        WebContents webContents =
                mBrowserController != null ? mBrowserController.getWebContents() : null;
        mContentView.setWebContents(webContents);
        mContentViewRenderView.setWebContents(webContents);
        mTopControlsContainerView.setWebContents(webContents);
        if (mBrowserController != null) {
            mGestureStateTracker =
                    new WebContentsGestureStateTracker(mContentView, webContents, this);
            mBrowserController.onDidGainActive(mTopControlsContainerView.getNativeHandle());
            mContentView.requestFocus();
        }
    }

    public BrowserControllerImpl getBrowserController() {
        return mBrowserController;
    }

    public void setTopView(View view) {
        mTopControlsContainerView.setView(view);
    }

    @Override
    public void onTopControlsCompletelyShownOrHidden() {
        adjustWebContentsHeightIfNecessary();
    }

    @Override
    public void onGestureStateChanged() {
        if (mGestureStateTracker.isInGestureOrScroll()) {
            mCachedDoBrowserControlsShrinkRendererSize =
                    mTopControlsContainerView.isTopControlVisible();
        }
        adjustWebContentsHeightIfNecessary();
    }

    private void adjustWebContentsHeightIfNecessary() {
        if (mGestureStateTracker.isInGestureOrScroll()
                || !mTopControlsContainerView.isTopControlsCompletelyShownOrHidden()) {
            return;
        }
        mContentViewRenderView.setWebContentsHeightDelta(
                mTopControlsContainerView.getTopContentOffset());
    }

    public void setSupportsEmbedding(boolean enable, ValueCallback<Boolean> callback) {
        mContentViewRenderView.requestMode(enable ? ContentViewRenderView.MODE_TEXTURE_VIEW
                                                  : ContentViewRenderView.MODE_SURFACE_VIEW,
                callback);
    }

    public void onTopControlsChanged(int topControlsOffsetY, int topContentOffsetY) {
        mTopControlsContainerView.onTopControlsChanged(topControlsOffsetY, topContentOffsetY);
    }

    public boolean doBrowserControlsShrinkRendererSize() {
        return (mGestureStateTracker.isInGestureOrScroll())
                ? mCachedDoBrowserControlsShrinkRendererSize
                : mTopControlsContainerView.isTopControlVisible();
    }
}
