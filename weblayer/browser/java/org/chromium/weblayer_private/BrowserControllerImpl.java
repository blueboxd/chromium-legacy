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
import android.widget.FrameLayout;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.content_public.browser.ViewEventSink;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.weblayer_private.aidl.IBrowserController;
import org.chromium.weblayer_private.aidl.IBrowserControllerClient;
import org.chromium.weblayer_private.aidl.INavigationControllerClient;
import org.chromium.weblayer_private.aidl.IObjectWrapper;
import org.chromium.weblayer_private.aidl.ObjectWrapper;

@JNINamespace("weblayer")
public final class BrowserControllerImpl extends IBrowserController.Stub {
    private long mNativeBrowserController;

    private ActivityWindowAndroid mWindowAndroid;
    // This view is the main view (returned from OnCreateView()).
    private ContentViewRenderView mContentViewRenderView;
    // One of these is needed per WebContents.
    private ContentView mContentView;
    // Child of mContentViewRenderView, holds top-view from client.
    private TopControlsContainerView mTopControlsContainerView;
    private ProfileImpl mProfile;
    private WebContents mWebContents;
    private BrowserObserverProxy mBrowserObserverProxy;
    private NavigationControllerImpl mNavigationController;

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

    public BrowserControllerImpl(Context context, ProfileImpl profile) {
        mProfile = profile;

        mWindowAndroid = new ActivityWindowAndroid(context);
        mContentViewRenderView = new ContentViewRenderView(context);
        mWindowAndroid.setAnimationPlaceholderView(mContentViewRenderView.getSurfaceView());

        mContentViewRenderView.onNativeLibraryLoaded(mWindowAndroid);

        mNativeBrowserController = nativeCreateBrowserController(profile.getNativeProfile());
        mWebContents = nativeGetWebContents(mNativeBrowserController);
        mTopControlsContainerView =
                new TopControlsContainerView(context, mWebContents, mContentViewRenderView);
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
                mWindowAndroid, WebContents.createDefaultInternalsHolder());

        mContentViewRenderView.setCurrentWebContents(mWebContents);

        mContentViewRenderView.addView(mContentView,
                new FrameLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT,
                        FrameLayout.LayoutParams.UNSPECIFIED_GRAVITY));

        nativeSetTopControlsContainerView(
                mNativeBrowserController, mTopControlsContainerView.getNativeHandle());
        mContentView.addView(mTopControlsContainerView,
                new FrameLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT,
                        Gravity.FILL_HORIZONTAL | Gravity.TOP));

        mWebContents.onShow();
        mContentView.requestFocus();
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
    public void setClient(IBrowserControllerClient client) {
        mBrowserObserverProxy = new BrowserObserverProxy(mNativeBrowserController, client);
    }

    @Override
    public void destroy() {
        nativeSetTopControlsContainerView(mNativeBrowserController, 0);
        mContentViewRenderView.destroy();
        mTopControlsContainerView.destroy();
        if (mBrowserObserverProxy != null) mBrowserObserverProxy.destroy();
        mBrowserObserverProxy = null;
        mNavigationController = null;
        nativeDeleteBrowserController(mNativeBrowserController);
        mNativeBrowserController = 0;
    }

    @Override
    public void setTopView(IObjectWrapper viewWrapper) {
        View view = ObjectWrapper.unwrap(viewWrapper, View.class);
        mTopControlsContainerView.setView(view);
    }

    @Override
    public IObjectWrapper onCreateView() {
        return ObjectWrapper.wrap(mContentViewRenderView);
    }

    private static native long nativeCreateBrowserController(long profile);
    private native void nativeSetTopControlsContainerView(
            long nativeBrowserControllerImpl, long nativeTopControlsContainerView);
    private static native void nativeDeleteBrowserController(long browserController);
    private native WebContents nativeGetWebContents(long nativeBrowserControllerImpl);
}
