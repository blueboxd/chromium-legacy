// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

@JNINamespace("weblayer")
public final class BrowserObserverProxy {
    private long mNativeBrowserObserverProxy;
    private BrowserControllerClient mClient;

    BrowserObserverProxy(long browserController, BrowserControllerClient client) {
        mClient = client;
        mNativeBrowserObserverProxy = nativeCreateBrowserObsererProxy(this, browserController);
    }

    public void destroy() {
        nativeDeleteBrowserObserverProxy(mNativeBrowserObserverProxy);
    }

    @CalledByNative
    private void displayURLChanged(String string) {
        mClient.displayURLChanged(string);
    }

    private static native long nativeCreateBrowserObsererProxy(
            BrowserObserverProxy proxy, long browserController);
    private static native void nativeDeleteBrowserObserverProxy(long proxy);
}
