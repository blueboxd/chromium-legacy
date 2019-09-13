// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.os.RemoteException;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
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
        mNativeBrowserObserverProxy = nativeCreateBrowserObsererProxy(this, browserController);
    }

    public void destroy() {
        nativeDeleteBrowserObserverProxy(mNativeBrowserObserverProxy);
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

    private static native long nativeCreateBrowserObsererProxy(
            BrowserObserverProxy proxy, long browserController);
    private static native void nativeDeleteBrowserObserverProxy(long proxy);
}
