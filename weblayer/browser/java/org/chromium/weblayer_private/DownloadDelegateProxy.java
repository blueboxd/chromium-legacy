// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.os.RemoteException;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.weblayer_private.aidl.APICallException;
import org.chromium.weblayer_private.aidl.IDownloadDelegateClient;

/**
 * Owns the c++ DownloadDelegateProxy class, which is responsible for forwarding all
 * DownloadDelegate delegate calls to this class, which in turn forwards to the
 * DownloadDelegateClient.
 */
@JNINamespace("weblayer")
public final class DownloadDelegateProxy {
    private long mNativeDownloadDelegateProxy;
    private IDownloadDelegateClient mClient;

    DownloadDelegateProxy(long browserController, IDownloadDelegateClient client) {
        assert client != null;
        mClient = client;
        mNativeDownloadDelegateProxy =
                DownloadDelegateProxyJni.get().createDownloadDelegateProxy(this, browserController);
    }

    public void setClient(IDownloadDelegateClient client) {
        assert client != null;
        mClient = client;
    }

    public void destroy() {
        DownloadDelegateProxyJni.get().deleteDownloadDelegateProxy(mNativeDownloadDelegateProxy);
        mNativeDownloadDelegateProxy = 0;
    }

    @CalledByNative
    private void downloadRequested(String url, String userAgent, String contentDisposition,
            String mimetype, long contentLength) {
        try {
            mClient.downloadRequested(url, userAgent, contentDisposition, mimetype, contentLength);
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @NativeMethods
    interface Natives {
        long createDownloadDelegateProxy(DownloadDelegateProxy proxy, long browserController);
        void deleteDownloadDelegateProxy(long proxy);
    }
}
