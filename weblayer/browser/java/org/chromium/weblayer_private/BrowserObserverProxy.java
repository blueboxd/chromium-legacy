// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.os.RemoteException;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.weblayer_private.aidl.APICallException;
import org.chromium.weblayer_private.aidl.IBrowserControllerClient;

/**
 * Owns the c++ BrowserObserverProxy class, which is responsible for forwarding all
 * BrowserObserver calls to this class, which in turn forwards to the BrowserControllerClient.
 * To avoid unnecessary IPC only one BrowserObserverProxy is created per BrowserController.
 */
@JNINamespace("weblayer")
public final class BrowserObserverProxy {
    private long mNativeBrowserObserverProxy;
    private IBrowserControllerClient mClient;

    BrowserObserverProxy(long browserController, IBrowserControllerClient client) {
        mClient = client;
        mNativeBrowserObserverProxy =
                BrowserObserverProxyJni.get().createBrowserObserverProxy(this, browserController);
    }

    public void destroy() {
        BrowserObserverProxyJni.get().deleteBrowserObserverProxy(mNativeBrowserObserverProxy);
        mNativeBrowserObserverProxy = 0;
    }

    @CalledByNative
    private void displayURLChanged(String string) {
        try {
            mClient.displayURLChanged(string);
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @NativeMethods
    interface Natives {
        long createBrowserObserverProxy(BrowserObserverProxy proxy, long browserController);
        void deleteBrowserObserverProxy(long proxy);
    }
}
