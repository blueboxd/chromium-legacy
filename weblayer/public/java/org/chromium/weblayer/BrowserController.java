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
    private BrowserFragmentController mBrowserFragmentController;
    private DownloadCallbackClientImpl mDownloadCallbackClient;
    private NewBrowserCallback mNewBrowserCallback;

    BrowserController(
            IBrowserController impl, BrowserFragmentController browserFragmentController) {
        mImpl = impl;
        mBrowserFragmentController = browserFragmentController;
        try {
            mImpl.setClient(new BrowserClientImpl());
        } catch (RemoteException e) {
            throw new APICallException(e);
        }

        mCallbacks = new ObserverList<BrowserCallback>();
        mNavigationController = NavigationController.create(mImpl);
        mBrowserFragmentController.registerBrowserController(this);
    }

    public BrowserFragmentController getBrowserFragmentController() {
        ThreadCheck.ensureOnUiThread();
        return mBrowserFragmentController;
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
     * Executes the script, and returns the result as a JSON object to the callback if provided. The
     * object passed to the callback will have a single key SCRIPT_RESULT_KEY which will hold the
     * result of running the script.
     * @param useSeparateIsolate If true, runs the script in a separate v8 Isolate. This uses more
     * memory, but separates the injected scrips from scripts in the page. This prevents any
     * potentially malicious interaction between first-party scripts in the page, and injected
     * scripts. Use with caution, only pass false for this argument if you know this isn't an issue
     * or you need to interact with first-party scripts.
     */
    public void executeScript(@NonNull String script, boolean useSeparateIsolate,
            @Nullable ValueCallback<JSONObject> callback) {
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
            mImpl.executeScript(script, useSeparateIsolate, ObjectWrapper.wrap(stringCallback));
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    public void setNewBrowserCallback(@Nullable NewBrowserCallback callback) {
        ThreadCheck.ensureOnUiThread();
        mNewBrowserCallback = callback;
        try {
            mImpl.setNewBrowsersEnabled(mNewBrowserCallback != null);
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

        @Override
        public void onNewBrowser(IBrowserController browser, int mode) {
            // This should only be hit if setNewBrowserCallback() has been called with a non-null
            // value.
            assert mNewBrowserCallback != null;
            BrowserController browserController =
                    new BrowserController(browser, mBrowserFragmentController);
            mNewBrowserCallback.onNewBrowser(browserController, mode);
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
