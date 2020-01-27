// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.os.RemoteException;
import android.webkit.ValueCallback;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.weblayer_private.interfaces.IDownloadCallbackClient;
import org.chromium.weblayer_private.interfaces.ObjectWrapper;

/**
 * Owns the c++ DownloadCallbackProxy class, which is responsible for forwarding all
 * DownloadDelegate delegate calls to this class, which in turn forwards to the
 * DownloadCallbackClient.
 */
@JNINamespace("weblayer")
public final class DownloadCallbackProxy {
    private long mNativeDownloadCallbackProxy;
    private BrowserImpl mBrowser;
    private IDownloadCallbackClient mClient;

    DownloadCallbackProxy(BrowserImpl browser, long tab, IDownloadCallbackClient client) {
        assert client != null;
        mBrowser = browser;
        mClient = client;
        mNativeDownloadCallbackProxy =
                DownloadCallbackProxyJni.get().createDownloadCallbackProxy(this, tab);
    }

    public void setClient(IDownloadCallbackClient client) {
        assert client != null;
        mClient = client;
    }

    public void destroy() {
        DownloadCallbackProxyJni.get().deleteDownloadCallbackProxy(mNativeDownloadCallbackProxy);
        mNativeDownloadCallbackProxy = 0;
    }

    @CalledByNative
    private boolean interceptDownload(String url, String userAgent, String contentDisposition,
            String mimetype, long contentLength) throws RemoteException {
        return mClient.interceptDownload(
                url, userAgent, contentDisposition, mimetype, contentLength);
    }

    @CalledByNative
    private void allowDownload(String url, String requestMethod, String requestInitiator,
            long callbackId) throws RemoteException {
        if (WebLayerFactoryImpl.getClientMajorVersion() < 81) {
            DownloadCallbackProxyJni.get().allowDownload(callbackId, true);
            return;
        }

        ValueCallback<Boolean> callback = new ValueCallback<Boolean>() {
            @Override
            public void onReceiveValue(Boolean result) {
                if (mNativeDownloadCallbackProxy == 0) {
                    throw new IllegalStateException("Called after destroy()");
                }
                DownloadCallbackProxyJni.get().allowDownload(callbackId, result);
            }
        };

        mClient.allowDownload(url, requestMethod, requestInitiator, ObjectWrapper.wrap(callback));
    }

    @CalledByNative
    private DownloadImpl createDownload(long nativeDownloadImpl) {
        return new DownloadImpl(mBrowser, mClient, nativeDownloadImpl);
    }

    @CalledByNative
    private void downloadStarted(DownloadImpl download) throws RemoteException {
        mClient.downloadStarted(download.getClientDownload());
        download.downloadStarted();
    }

    @CalledByNative
    private void downloadProgressChanged(DownloadImpl download) throws RemoteException {
        mClient.downloadProgressChanged(download.getClientDownload());
        download.downloadProgressChanged();
    }

    @CalledByNative
    private void downloadCompleted(DownloadImpl download) throws RemoteException {
        mClient.downloadCompleted(download.getClientDownload());
        download.downloadCompleted();
    }

    @CalledByNative
    private void downloadFailed(DownloadImpl download) throws RemoteException {
        mClient.downloadFailed(download.getClientDownload());
        download.downloadFailed();
    }

    @NativeMethods
    interface Natives {
        long createDownloadCallbackProxy(DownloadCallbackProxy proxy, long tab);
        void deleteDownloadCallbackProxy(long proxy);
        void allowDownload(long callbackId, boolean allow);
    }
}
