// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.content.Context;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.widget.LinearLayout;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.components.embedder_support.view.ContentViewRenderView;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.ViewEventSink;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.weblayer_private.aidl.IBrowserController;
import org.chromium.weblayer_private.aidl.IBrowserControllerClient;
import org.chromium.weblayer_private.aidl.IObjectWrapper;
import org.chromium.weblayer_private.aidl.ObjectWrapper;

@JNINamespace("weblayer")
public final class BrowserControllerImpl extends IBrowserController.Stub {
    private long mNativeBrowserController;

    private ActivityWindowAndroid mWindowAndroid;
    // This is set as the content view of the activity. It contains mContentViewRenderView.
    private LinearLayout mLinearLayout;
    // This is parented to mLinearLayout.
    private ContentViewRenderView mContentViewRenderView;
    // One of these is needed per WebContents.
    private ContentView mContentView;
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

    public BrowserControllerImpl(Context context, ProfileImpl profile) {
        mProfile = profile;

        mLinearLayout = new LinearLayout(context);
        mLinearLayout.setOrientation(LinearLayout.VERTICAL);

        mWindowAndroid = new ActivityWindowAndroid(context);
        mContentViewRenderView = new ContentViewRenderView(context);
        mWindowAndroid.setAnimationPlaceholderView(mContentViewRenderView.getSurfaceView());

        mContentViewRenderView.onNativeLibraryLoaded(mWindowAndroid);

        mNativeBrowserController = nativeCreateBrowserController(profile.getNativeProfile());
        mWebContents = nativeGetWebContents(mNativeBrowserController);
        mWebContents.initialize("", ViewAndroidDelegate.createBasicDelegate(mContentViewRenderView),
                new InternalAccessDelegateImpl(), mWindowAndroid,
                WebContents.createDefaultInternalsHolder());

        mContentViewRenderView.setCurrentWebContents(mWebContents);
        mLinearLayout.addView(mContentViewRenderView,
                new LinearLayout.LayoutParams(
                        LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT, 1f));

        mContentView = ContentView.createContentView(context, mWebContents);
        mContentViewRenderView.addView(mContentView,
                new LinearLayout.LayoutParams(
                        LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT, 1f));

        mWebContents.onShow();
    }

    @Override
    public void setClient(IBrowserControllerClient client) {
        mBrowserObserverProxy = new BrowserObserverProxy(mNativeBrowserController, client);
    }

    @Override
    public void destroy() {
        if (mBrowserObserverProxy != null) mBrowserObserverProxy.destroy();
        nativeDeleteBrowserController(mNativeBrowserController);
        mNativeBrowserController = 0;
    }

    @Override
    public void setTopView(IObjectWrapper viewWrapper) {
        View view = ObjectWrapper.unwrap(viewWrapper, View.class);
        if (mTopView == view) return;
        if (mTopView != null) mLinearLayout.removeView(mTopView);
        mTopView = view;
        if (mTopView != null) {
            mLinearLayout.addView(mTopView, 0,
                    new LinearLayout.LayoutParams(
                            LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT, 0f));
        }
    }

    @Override
    public IObjectWrapper onCreateView() {
        return ObjectWrapper.wrap(mLinearLayout);
    }

    // TODO: this is temporary, move to NavigationControllerImpl.
    @Override
    public void navigate(String uri) {
        mWebContents.getNavigationController().loadUrl(new LoadUrlParams(uri));
    }

    private static native long nativeCreateBrowserController(long profile);
    private static native void nativeDeleteBrowserController(long browserController);
    private native WebContents nativeGetWebContents(long nativeBrowserControllerImpl);
}
