// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.os.RemoteException;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.weblayer_private.aidl.NewBrowserType;

/**
 * Owns the c++ NewBrowserCallback class, which is responsible for forwarding all
 * NewBrowserCallback calls to this class, which in turn forwards to the
 * IBrowserFragmentControllerClient.
 */
@JNINamespace("weblayer")
public final class NewBrowserCallbackProxy {
    private long mNativeNewBrowserCallbackProxy;
    private final BrowserControllerImpl mBrowser;

    public NewBrowserCallbackProxy(BrowserControllerImpl browser) {
        mBrowser = browser;
        mNativeNewBrowserCallbackProxy =
                NewBrowserCallbackProxyJni.get().createNewBrowserCallbackProxy(
                        this, browser.getNativeBrowserController());
    }

    public void destroy() {
        NewBrowserCallbackProxyJni.get().deleteNewBrowserCallbackProxy(
                mNativeNewBrowserCallbackProxy);
        mNativeNewBrowserCallbackProxy = 0;
    }

    @NewBrowserType
    private static int implTypeToJavaType(@ImplNewBrowserType int type) {
        switch (type) {
            case ImplNewBrowserType.FOREGROUND_TAB:
                return NewBrowserType.FOREGROUND_TAB;
            case ImplNewBrowserType.BACKGROUND_TAB:
                return NewBrowserType.BACKGROUND_TAB;
            case ImplNewBrowserType.NEW_POPUP:
                return NewBrowserType.NEW_POPUP;
            case ImplNewBrowserType.NEW_WINDOW:
                return NewBrowserType.NEW_WINDOW;
        }
        assert false;
        return NewBrowserType.FOREGROUND_TAB;
    }

    @CalledByNative
    public void onNewBrowser(long nativeBrowser, @ImplNewBrowserType int mode)
            throws RemoteException {
        // This class should only be created while the browser is attached to a fragment.
        assert mBrowser.getFragment() != null;
        BrowserControllerImpl browser = new BrowserControllerImpl(
                mBrowser.getProfile(), mBrowser.getFragment().getWindowAndroid(), nativeBrowser);
        mBrowser.getFragment().attachBrowserController(browser);
        mBrowser.getClient().onNewBrowser(browser, implTypeToJavaType(mode));
    }

    @NativeMethods
    interface Natives {
        long createNewBrowserCallbackProxy(NewBrowserCallbackProxy proxy, long browserController);
        void deleteNewBrowserCallbackProxy(long proxy);
    }
}
