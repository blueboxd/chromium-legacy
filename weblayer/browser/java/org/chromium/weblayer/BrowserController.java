// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.ViewGroup;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.content_public.browser.ViewEventSink;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;

@JNINamespace("weblayer")
public class BrowserController {
    private long mNativeBrowserController;
    private WebContents mWebContents;

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

    public static BrowserController create(Profile profile) {
        return new BrowserController(nativeInit(profile.getNativeProfile()));
    }

    private BrowserController(long nativeBrowserController) {
        mNativeBrowserController = nativeBrowserController;
        mWebContents = nativeGetWebContents(mNativeBrowserController);
    }

    public void initialize(ViewGroup containerView, WindowAndroid windowAndroid) {
        mWebContents.initialize("", ViewAndroidDelegate.createBasicDelegate(containerView),
                new InternalAccessDelegateImpl(), windowAndroid,
                WebContents.createDefaultInternalsHolder());
    }

    public WebContents getWebContents() {
        return mWebContents;
    }

    public void show() {
        mWebContents.onShow();
    }

    public void navigate(String url) {
        nativeNavigate(mNativeBrowserController, url);
    }

    public void destroy() {}

    private static native long nativeInit(long profile);
    private native WebContents nativeGetWebContents(long nativeBrowserControllerImpl);
    private native void nativeNavigate(long nativeBrowserControllerImpl, String url);
}
