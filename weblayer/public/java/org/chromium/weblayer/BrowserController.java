// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.net.Uri;
import android.os.RemoteException;
import android.webkit.ValueCallback;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.weblayer_private.aidl.APICallException;
import org.chromium.weblayer_private.aidl.IBrowserController;
import org.chromium.weblayer_private.aidl.IBrowserControllerClient;
import org.chromium.weblayer_private.aidl.IDownloadCallbackClient;
import org.chromium.weblayer_private.aidl.IFullscreenCallbackClient;
import org.chromium.weblayer_private.aidl.IObjectWrapper;
import org.chromium.weblayer_private.aidl.ObjectWrapper;

/**
 * Represents a web-browser. More specifically, owns a NavigationController, and allows configuring
 * state of the browser, such as delegates and callbacks.
 */
public final class BrowserController {
    /** The top level key of the JSON object returned by executeScript(). */
    public static final String SCRIPT_RESULT_KEY = "result";

    private final IBrowserController mImpl;
    private FullscreenCallbackClientImpl mFullscreenCallbackClient;
    private final NavigationController mNavigationController;
    private final ObserverList<BrowserCallback> mCallbacks;
    private DownloadCallbackClientImpl mDownloadCallbackClient;

    BrowserController(IBrowserController impl) {
        mImpl = impl;
        try {
            mImpl.setClient(new BrowserClientImpl());
        } catch (RemoteException e) {
            throw new APICallException(e);
        }

        mCallbacks = new ObserverList<BrowserCallback>();
        mNavigationController = NavigationController.create(mImpl);
    }

    public void setDownloadCallback(@Nullable DownloadCallback callback) {
        ThreadCheck.ensureOnUiThread();
        try {
            if (callback != null) {
                mDownloadCallbackClient = new DownloadCallbackClientImpl(callback);
                mImpl.setDownloadCallbackClient(mDownloadCallbackClient);
            } else {
                mDownloadCallbackClient = null;
                mImpl.setDownloadCallbackClient(null);
            }
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    public void setFullscreenCallback(@Nullable FullscreenCallback callback) {
        ThreadCheck.ensureOnUiThread();
        try {
            if (callback != null) {
                mFullscreenCallbackClient = new FullscreenCallbackClientImpl(callback);
                mImpl.setFullscreenCallbackClient(mFullscreenCallbackClient);
            } else {
                mImpl.setFullscreenCallbackClient(null);
                mFullscreenCallbackClient = null;
            }
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    public DownloadCallback getDownloadCallback() {
        ThreadCheck.ensureOnUiThread();
        return mDownloadCallbackClient != null ? mDownloadCallbackClient.getCallback() : null;
    }

    /**
     * Executes the script in an isolated world, and returns the result as a JSON object to the
     * callback if provided. The object passed to the callback will have a single key
     * SCRIPT_RESULT_KEY which will hold the result of running the script.
     */
    public void executeScript(
            @NonNull String script, @Nullable ValueCallback<JSONObject> callback) {
        ThreadCheck.ensureOnUiThread();
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

    @Nullable
    public FullscreenCallback getFullscreenCallback() {
        ThreadCheck.ensureOnUiThread();
        return mFullscreenCallbackClient != null ? mFullscreenCallbackClient.getCallback() : null;
    }

    @NonNull
    public NavigationController getNavigationController() {
        ThreadCheck.ensureOnUiThread();
        return mNavigationController;
    }

    public void registerBrowserCallback(@Nullable BrowserCallback callback) {
        ThreadCheck.ensureOnUiThread();
        mCallbacks.addObserver(callback);
    }

    public void unregisterBrowserCallback(@Nullable BrowserCallback callback) {
        ThreadCheck.ensureOnUiThread();
        mCallbacks.removeObserver(callback);
    }

    IBrowserController getIBrowserController() {
        return mImpl;
    }

    private final class BrowserClientImpl extends IBrowserControllerClient.Stub {
        @Override
        public void visibleUrlChanged(String url) {
            Uri uri = Uri.parse(url);
            for (BrowserCallback callback : mCallbacks) {
                callback.visibleUrlChanged(uri);
            }
        }
    }

    private static final class DownloadCallbackClientImpl extends IDownloadCallbackClient.Stub {
        private final DownloadCallback mCallback;

        DownloadCallbackClientImpl(DownloadCallback callback) {
            mCallback = callback;
        }

        public DownloadCallback getCallback() {
            return mCallback;
        }

        @Override
        public void downloadRequested(String url, String userAgent, String contentDisposition,
                String mimetype, long contentLength) {
            mCallback.downloadRequested(
                    url, userAgent, contentDisposition, mimetype, contentLength);
        }
    }

    private static final class FullscreenCallbackClientImpl extends IFullscreenCallbackClient.Stub {
        private FullscreenCallback mCallback;

        /* package */ FullscreenCallbackClientImpl(FullscreenCallback callback) {
            mCallback = callback;
        }

        public FullscreenCallback getCallback() {
            return mCallback;
        }

        @Override
        public void enterFullscreen(IObjectWrapper exitFullscreenWrapper) {
            ValueCallback<Void> exitFullscreenCallback = (ValueCallback<Void>) ObjectWrapper.unwrap(
                    exitFullscreenWrapper, ValueCallback.class);
            mCallback.enterFullscreen(() -> exitFullscreenCallback.onReceiveValue(null));
        }

        @Override
        public void exitFullscreen() {
            mCallback.exitFullscreen();
        }
    }
}
