// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.os.RemoteException;
import android.util.AndroidRuntimeException;

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.weblayer_private.aidl.IBrowserControllerClient;

@JNINamespace("weblayer")
public final class BrowserObserverProxy {
    private static final String TAG = "WL_BObserverProxy";

    private long mNativeBrowserObserverProxy;
    private IBrowserControllerClient mClient;

    BrowserObserverProxy(long browserController, IBrowserControllerClient client) {
        mClient = client;
        mNativeBrowserObserverProxy = nativeCreateBrowserObsererProxy(this, browserController);
    }

    public void destroy() {
        nativeDeleteBrowserObserverProxy(mNativeBrowserObserverProxy);
    }

    @CalledByNative
    private void displayURLChanged(String string) {
        try {
            mClient.displayURLChanged(string);
        } catch (RemoteException e) {
            Log.e(TAG, "Failed to call displayURLChanged.", e);
            throw new AndroidRuntimeException(e);
        }
    }

    private static native long nativeCreateBrowserObsererProxy(
            BrowserObserverProxy proxy, long browserController);
    private static native void nativeDeleteBrowserObserverProxy(long proxy);
}
