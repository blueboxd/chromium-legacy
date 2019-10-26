// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.content.Context;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.webkit.ValueCallback;
import android.widget.FrameLayout;

import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.content_public.browser.ViewEventSink;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.weblayer_private.aidl.IBrowserController;
import org.chromium.weblayer_private.aidl.IBrowserControllerClient;
import org.chromium.weblayer_private.aidl.IDownloadDelegateClient;
import org.chromium.weblayer_private.aidl.IFullscreenDelegateClient;
import org.chromium.weblayer_private.aidl.INavigationControllerClient;
import org.chromium.weblayer_private.aidl.IObjectWrapper;
import org.chromium.weblayer_private.aidl.ObjectWrapper;

/**
 * Implementation of IBrowserController.
 */
@JNINamespace("weblayer")
public final class BrowserControllerImpl extends IBrowserController.Stub
        implements TopControlsContainerView.Listener,
                   WebContentsGestureStateTracker.OnGestureStateChangedListener {
    private long mNativeBrowserController;

    // TODO: move mContentViewRenderView, mContentView, mTopControlsContainerView to
    // BrowserFragmentControllerImpl.
    // This view is the main view (returned from the fragment's onCreateView()).
    private ContentViewRenderView mContentViewRenderView;
    // One of these is needed per WebContents.
    private ContentView mContentView;
    // Child of mContentViewRenderView, holds top-view from client.
    private TopControlsContainerView mTopControlsContainerView;
    private ProfileImpl mProfile;
    private WebContents mWebContents;
    private BrowserObserverProxy mBrowserObserverProxy;
    private NavigationControllerImpl mNavigationController;
    private DownloadDelegateProxy mDownloadDelegateProxy;
    private FullscreenDelegateProxy mFullscreenDelegateProxy;
    private WebContentsGestureStateTracker mGestureStateTracker;
    /**
     * The value of mCachedDoBrowserControlsShrinkRendererSize is set when
     * WebContentsGestureStateTracker begins a gesture. This is necessary as the values should only
     * change once a gesture is no longer under way.
     */
    private boolean mCachedDoBrowserControlsShrinkRendererSize;

    private static class InternalAccessDelegateImpl
            implements ViewEventSink.InternalAccessDelegate {
        @Override
        public boolean super_onKeyUp(int keyCode, KeyEvent event) {
            return false;
        }

        @Override
        public boolean super_dispatchKeyEvent(KeyEvent event) {
            return false;
        }

        @Override
        public boolean super_onGenericMotionEvent(MotionEvent event) {
            return false;
        }

        @Override
        public void onScrollChanged(int lPix, int tPix, int oldlPix, int oldtPix) {}
    }

    public BrowserControllerImpl(
            Context context, ProfileImpl profile, WindowAndroid windowAndroid) {
        mProfile = profile;

        mContentViewRenderView = new ContentViewRenderView(context);

        mContentViewRenderView.onNativeLibraryLoaded(
                windowAndroid, ContentViewRenderView.MODE_SURFACE_VIEW);

        mNativeBrowserController = BrowserControllerImplJni.get().createBrowserController(
                profile.getNativeProfile(), this);
        mWebContents = BrowserControllerImplJni.get().getWebContents(
                mNativeBrowserController, BrowserControllerImpl.this);
        mTopControlsContainerView =
                new TopControlsContainerView(context, mWebContents, mContentViewRenderView, this);
        mContentView = ContentView.createContentView(
                context, mWebContents, mTopControlsContainerView.getEventOffsetHandler());
        ViewAndroidDelegate viewAndroidDelegate = new ViewAndroidDelegate(mContentView) {
            @Override
            public void onTopControlsChanged(int topControlsOffsetY, int topContentOffsetY) {
                mTopControlsContainerView.onTopControlsChanged(
                        topControlsOffsetY, topContentOffsetY);
            }
        };
        mWebContents.initialize("", viewAndroidDelegate, new InternalAccessDelegateImpl(),
                windowAndroid, WebContents.createDefaultInternalsHolder());

        mContentViewRenderView.setCurrentWebContents(mWebContents);

        mContentViewRenderView.addView(mContentView,
                new FrameLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT,
                        FrameLayout.LayoutParams.UNSPECIFIED_GRAVITY));

        BrowserControllerImplJni.get().setTopControlsContainerView(mNativeBrowserController,
                BrowserControllerImpl.this, mTopControlsContainerView.getNativeHandle());
        mContentView.addView(mTopControlsContainerView,
                new FrameLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT,
                        Gravity.FILL_HORIZONTAL | Gravity.TOP));

        mWebContents.onShow();
        mContentView.requestFocus();

        mGestureStateTracker = new WebContentsGestureStateTracker(mContentView, mWebContents, this);
    }

    long getNativeBrowserController() {
        return mNativeBrowserController;
    }

    @Override
    public NavigationControllerImpl createNavigationController(INavigationControllerClient client) {
        // This should only be called once.
        assert mNavigationController == null;
        mNavigationController = new NavigationControllerImpl(this, client);
        return mNavigationController;
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

    @Override
    public void setClient(IBrowserControllerClient client) {
        mBrowserObserverProxy = new BrowserObserverProxy(mNativeBrowserController, client);
    }

    @Override
    public void setDownloadDelegateClient(IDownloadDelegateClient client) {
        if (client != null) {
            if (mDownloadDelegateProxy == null) {
                mDownloadDelegateProxy =
                        new DownloadDelegateProxy(mNativeBrowserController, client);
            } else {
                mDownloadDelegateProxy.setClient(client);
            }
        } else if (mDownloadDelegateProxy != null) {
            mDownloadDelegateProxy.destroy();
            mDownloadDelegateProxy = null;
        }
    }

    @Override
    public void setFullscreenDelegateClient(IFullscreenDelegateClient client) {
        if (client != null) {
            if (mFullscreenDelegateProxy == null) {
                mFullscreenDelegateProxy =
                        new FullscreenDelegateProxy(mNativeBrowserController, client);
            } else {
                mFullscreenDelegateProxy.setClient(client);
            }
        } else if (mFullscreenDelegateProxy != null) {
            mFullscreenDelegateProxy.destroy();
            mFullscreenDelegateProxy = null;
        }
    }

    @Override
    public void executeScript(String script, IObjectWrapper callback) {
        Callback<String> nativeCallback = new Callback<String>() {
            @Override
            public void onResult(String result) {
                ValueCallback<String> unwrappedCallback =
                        (ValueCallback<String>) ObjectWrapper.unwrap(callback, ValueCallback.class);
                if (unwrappedCallback != null) {
                    unwrappedCallback.onReceiveValue(result);
                }
            }
        };
        BrowserControllerImplJni.get().executeScript(
                mNativeBrowserController, script, nativeCallback);
    }

    public void destroy() {
        BrowserControllerImplJni.get().setTopControlsContainerView(
                mNativeBrowserController, BrowserControllerImpl.this, 0);
        mTopControlsContainerView.destroy();
        mContentViewRenderView.destroy();
        if (mBrowserObserverProxy != null) {
            mBrowserObserverProxy.destroy();
            mBrowserObserverProxy = null;
        }
        if (mDownloadDelegateProxy != null) {
            mDownloadDelegateProxy.destroy();
            mDownloadDelegateProxy = null;
        }
        if (mFullscreenDelegateProxy != null) {
            mFullscreenDelegateProxy.destroy();
            mFullscreenDelegateProxy = null;
        }
        mGestureStateTracker.destroy();
        mNavigationController = null;
        BrowserControllerImplJni.get().deleteBrowserController(mNativeBrowserController);
        mNativeBrowserController = 0;
    }

    public void setTopView(IObjectWrapper viewWrapper) {
        View view = ObjectWrapper.unwrap(viewWrapper, View.class);
        mTopControlsContainerView.setView(view);
    }

    /** Returns top-level View this Controller works with */
    public View getView() {
        return mContentViewRenderView;
    }

    public void setSupportsEmbedding(boolean enable, IObjectWrapper callback) {
        mContentViewRenderView.requestMode(enable ? ContentViewRenderView.MODE_TEXTURE_VIEW
                                                  : ContentViewRenderView.MODE_SURFACE_VIEW,
                (ValueCallback<Boolean>) ObjectWrapper.unwrap(callback, ValueCallback.class));
    }

    @CalledByNative
    private boolean doBrowserControlsShrinkRendererSize() {
        return (mGestureStateTracker.isInGestureOrScroll())
                ? mCachedDoBrowserControlsShrinkRendererSize
                : mTopControlsContainerView.isTopControlVisible();
    }

    @NativeMethods
    interface Natives {
        long createBrowserController(long profile, BrowserControllerImpl caller);
        void setTopControlsContainerView(long nativeBrowserControllerImpl,
                BrowserControllerImpl caller, long nativeTopControlsContainerView);
        void deleteBrowserController(long browserController);
        WebContents getWebContents(long nativeBrowserControllerImpl, BrowserControllerImpl caller);
        void executeScript(
                long nativeBrowserControllerImpl, String script, Callback<String> callback);
    }
}
