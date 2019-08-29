// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.app.Activity;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.widget.LinearLayout;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.components.embedder_support.view.ContentViewRenderView;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.ViewEventSink;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.ViewAndroidDelegate;

@JNINamespace("weblayer")
public final class BrowserControllerImpl {
    private long mNativeBrowserController;

    private ActivityWindowAndroid mWindowAndroid;
    // This is set as the content view of the activity. It contains mContentView.
    private LinearLayout mLinearLayout;
    // This is parented to mLinearLayout.
    private ContentViewRenderView mContentView;
    private ProfileImpl mProfile;
    private WebContents mWebContents;
    private BrowserObserverProxy mBrowserObserverProxy;
    private View mTopView;

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
            Activity activity, ProfileImpl profile, BrowserControllerClient client) {
        mProfile = profile;

        mLinearLayout = new LinearLayout(activity);
        mLinearLayout.setOrientation(LinearLayout.VERTICAL);

        mWindowAndroid = new ActivityWindowAndroid(activity);
        mContentView = new ContentViewRenderView(activity);
        activity.setContentView(mLinearLayout);
        mWindowAndroid.setAnimationPlaceholderView(mContentView.getSurfaceView());

        mContentView.onNativeLibraryLoaded(mWindowAndroid);

        mNativeBrowserController = nativeCreateBrowserController(profile.getNativeProfile());
        mWebContents = nativeGetWebContents(mNativeBrowserController);
        mWebContents.initialize("", ViewAndroidDelegate.createBasicDelegate(mContentView),
                new InternalAccessDelegateImpl(), mWindowAndroid,
                WebContents.createDefaultInternalsHolder());

        mContentView.setCurrentWebContents(mWebContents);
        mLinearLayout.addView(mContentView,
                new LinearLayout.LayoutParams(
                        LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT, 1f));
        mWebContents.onShow();
        mBrowserObserverProxy = new BrowserObserverProxy(mNativeBrowserController, client);
    }

    public void destroy() {
        mBrowserObserverProxy.destroy();
        nativeDeleteBrowserController(mNativeBrowserController);
        mNativeBrowserController = 0;
    }

    public void setTopView(View view) {
        if (mTopView == view) return;
        if (mTopView != null) mLinearLayout.removeView(mTopView);
        mTopView = view;
        if (mTopView != null) {
            mLinearLayout.addView(mTopView, 0,
                    new LinearLayout.LayoutParams(
                            LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT, 0f));
        }
    }

    // TODO: this is temporary, move to NavigationControllerImpl.
    public void navigate(String uri) {
        mWebContents.getNavigationController().loadUrl(new LoadUrlParams(uri));
    }

    private static native long nativeCreateBrowserController(long profile);
    private static native void nativeDeleteBrowserController(long browserController);
    private native WebContents nativeGetWebContents(long nativeBrowserControllerImpl);
}
