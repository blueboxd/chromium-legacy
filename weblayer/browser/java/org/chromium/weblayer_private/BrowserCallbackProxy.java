// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.os.RemoteException;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.weblayer_private.aidl.IBrowserControllerClient;

/**
 * Owns the C++ BrowserCallbackProxy class, which is responsible for forwarding all
 * BrowserObserver calls to this class, which in turn forwards to the BrowserControllerClient.
 * To avoid unnecessary IPC only one BrowserCallbackProxy is created per BrowserController.
 */
@JNINamespace("weblayer")
public final class BrowserCallbackProxy {
    private long mNativeBrowserCallbackProxy;
    private IBrowserControllerClient mClient;

    BrowserCallbackProxy(long browserController, IBrowserControllerClient client) {
        mClient = client;
        mNativeBrowserCallbackProxy =
                BrowserCallbackProxyJni.get().createBrowserCallbackProxy(this, browserController);
    }

    public void destroy() {
        BrowserCallbackProxyJni.get().deleteBrowserCallbackProxy(mNativeBrowserCallbackProxy);
        mNativeBrowserCallbackProxy = 0;
    }

    @CalledByNative
    private void visibleUrlChanged(String string) throws RemoteException {
        mClient.visibleUrlChanged(string);
    }

    @NativeMethods
    interface Natives {
        long createBrowserCallbackProxy(BrowserCallbackProxy proxy, long browserController);
        void deleteBrowserCallbackProxy(long proxy);
    }
}
