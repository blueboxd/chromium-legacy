// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.net.Uri;
import android.os.RemoteException;
import android.webkit.ValueCallback;

import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.weblayer_private.aidl.APICallException;
import org.chromium.weblayer_private.aidl.IBrowserController;
import org.chromium.weblayer_private.aidl.IBrowserControllerClient;
import org.chromium.weblayer_private.aidl.IDownloadDelegateClient;
import org.chromium.weblayer_private.aidl.IFullscreenDelegateClient;
import org.chromium.weblayer_private.aidl.IObjectWrapper;
import org.chromium.weblayer_private.aidl.ObjectWrapper;

public final class BrowserController {
    /** The top level key of the JSON object returned by executeScript(). */
    public static final String SCRIPT_RESULT_KEY = "result";

    private final IBrowserController mImpl;
    private FullscreenDelegateClientImpl mFullscreenDelegateClient;
    private final NavigationController mNavigationController;
    private final ObserverList<BrowserObserver> mObservers;
    private DownloadDelegateClientImpl mDownloadDelegateClient;

    BrowserController(IBrowserController impl) {
        mImpl = impl;
        try {
            mImpl.setClient(new BrowserClientImpl());
        } catch (RemoteException e) {
            throw new APICallException(e);
        }

        mObservers = new ObserverList<BrowserObserver>();
        mNavigationController = NavigationController.create(mImpl);
    }

    public void setDownloadDelegate(DownloadDelegate delegate) {
        try {
            if (delegate != null) {
                mDownloadDelegateClient = new DownloadDelegateClientImpl(delegate);
                mImpl.setDownloadDelegateClient(mDownloadDelegateClient);
            } else {
                mDownloadDelegateClient = null;
                mImpl.setDownloadDelegateClient(null);
            }
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    public void setFullscreenDelegate(FullscreenDelegate delegate) {
        try {
            if (delegate != null) {
                mFullscreenDelegateClient = new FullscreenDelegateClientImpl(delegate);
                mImpl.setFullscreenDelegateClient(mFullscreenDelegateClient);
            } else {
                mImpl.setFullscreenDelegateClient(null);
            }
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    public DownloadDelegate getDownloadDelegate() {
        return mDownloadDelegateClient != null ? mDownloadDelegateClient.getDelegate() : null;
    }

    /**
     * Executes the script in an isolated world, and returns the result as a JSON object to the
     * callback if provided. The object passed to the callback will have a single key
     * SCRIPT_RESULT_KEY which will hold the result of running the script.
     */
    public void executeScript(String script, ValueCallback<JSONObject> callback) {
        try {
            ValueCallback<String> stringCallback = (String result) -> {
                if (callback == null) {
                    return;
                }

                try {
                    callback.onReceiveValue(
                            new JSONObject("{\"" + SCRIPT_RESULT_KEY + "\":" + result + "}"));
                } catch (JSONException e) {
                    // This should never happen since the result should be well formed.
                    throw new RuntimeException(e);
                }
            };
            mImpl.executeScript(script, ObjectWrapper.wrap(stringCallback));
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    public FullscreenDelegate getFullscreenDelegate() {
        return mFullscreenDelegateClient != null ? mFullscreenDelegateClient.getDelegate() : null;
    }

    @Override
    protected void finalize() {
        // TODO(sky): figure out right assertion here if mProfile is non-null.
    }

    public NavigationController getNavigationController() {
        return mNavigationController;
    }

    public void addObserver(BrowserObserver observer) {
        mObservers.addObserver(observer);
    }

    public void removeObserver(BrowserObserver observer) {
        mObservers.removeObserver(observer);
    }

    IBrowserController getIBrowserController() {
        return mImpl;
    }

    private final class BrowserClientImpl extends IBrowserControllerClient.Stub {
        @Override
        public void visibleUrlChanged(String url) {
            Uri uri = Uri.parse(url);
            for (BrowserObserver observer : mObservers) {
                observer.visibleUrlChanged(uri);
            }
        }

        @Override
        public void loadingStateChanged(boolean isLoading, boolean toDifferentDocument) {
            for (BrowserObserver observer : mObservers) {
                observer.loadingStateChanged(isLoading, toDifferentDocument);
            }
        }

        @Override
        public void loadProgressChanged(double progress) {
            for (BrowserObserver observer : mObservers) {
                observer.loadProgressChanged(progress);
            }
        }
    }

    private final class DownloadDelegateClientImpl extends IDownloadDelegateClient.Stub {
        private final DownloadDelegate mDelegate;

        DownloadDelegateClientImpl(DownloadDelegate delegate) {
            mDelegate = delegate;
        }

        public DownloadDelegate getDelegate() {
            return mDelegate;
        }

        @Override
        public void downloadRequested(String url, String userAgent, String contentDisposition,
                String mimetype, long contentLength) {
            mDelegate.downloadRequested(
                    url, userAgent, contentDisposition, mimetype, contentLength);
        }
    }

    private final class FullscreenDelegateClientImpl extends IFullscreenDelegateClient.Stub {
        private FullscreenDelegate mDelegate;

        /* package */ FullscreenDelegateClientImpl(FullscreenDelegate delegate) {
            mDelegate = delegate;
        }

        public FullscreenDelegate getDelegate() {
            return mDelegate;
        }

        @Override
        public void enterFullscreen(IObjectWrapper exitFullscreenWrapper) {
            ValueCallback<Void> exitFullscreenCallback = (ValueCallback<Void>) ObjectWrapper.unwrap(
                    exitFullscreenWrapper, ValueCallback.class);
            mDelegate.enterFullscreen(() -> exitFullscreenCallback.onReceiveValue(null));
        }

        @Override
        public void exitFullscreen() {
            mDelegate.exitFullscreen();
        }
    }
}
