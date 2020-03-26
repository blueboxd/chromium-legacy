// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.content.Intent;
import android.net.Uri;
import android.os.RemoteException;
import android.webkit.ValueCallback;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.weblayer_private.interfaces.APICallException;
import org.chromium.weblayer_private.interfaces.IClientDownload;
import org.chromium.weblayer_private.interfaces.IDownload;
import org.chromium.weblayer_private.interfaces.IDownloadCallbackClient;
import org.chromium.weblayer_private.interfaces.IErrorPageCallbackClient;
import org.chromium.weblayer_private.interfaces.IFullscreenCallbackClient;
import org.chromium.weblayer_private.interfaces.IObjectWrapper;
import org.chromium.weblayer_private.interfaces.ITab;
import org.chromium.weblayer_private.interfaces.ITabClient;
import org.chromium.weblayer_private.interfaces.ObjectWrapper;
import org.chromium.weblayer_private.interfaces.StrictModeWorkaround;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/**
 * Represents a single tab in a browser. More specifically, owns a NavigationController, and allows
 * configuring state of the tab, such as delegates and callbacks.
 */
public class Tab {
    /** The top level key of the JSON object returned by executeScript(). */
    public static final String SCRIPT_RESULT_KEY = "result";

    // Maps from id (as returned from ITab.getId()) to Tab.
    private static final Map<Integer, Tab> sTabMap = new HashMap<Integer, Tab>();

    private final ITab mImpl;
    private final NavigationController mNavigationController;
    private final FindInPageController mFindInPageController;
    private final ObserverList<TabCallback> mCallbacks;
    private Browser mBrowser;
    private DownloadCallbackClientImpl mDownloadCallbackClient;
    private FullscreenCallbackClientImpl mFullscreenCallbackClient;
    private NewTabCallback mNewTabCallback;
    // Id from the remote side.
    private final int mId;

    // Constructor for test mocking.
    protected Tab() {
        mImpl = null;
        mNavigationController = null;
        mFindInPageController = null;
        mCallbacks = null;
        mId = 0;
    }

    Tab(ITab impl, Browser browser) {
        mImpl = impl;
        mBrowser = browser;
        try {
            mId = impl.getId();
            mImpl.setClient(new TabClientImpl());
        } catch (RemoteException e) {
            throw new APICallException(e);
        }

        mCallbacks = new ObserverList<TabCallback>();
        mNavigationController = NavigationController.create(mImpl);
        mFindInPageController = new FindInPageController(mImpl);
        registerTab(this);
    }

    static void registerTab(Tab tab) {
        assert getTabById(tab.getId()) == null;
        sTabMap.put(tab.getId(), tab);
    }

    static void unregisterTab(Tab tab) {
        assert getTabById(tab.getId()) != null;
        sTabMap.remove(tab.getId());
    }

    static Tab getTabById(int id) {
        return sTabMap.get(id);
    }

    static Set<Tab> getTabsInBrowser(Browser browser) {
        Set<Tab> tabs = new HashSet<Tab>();
        for (Tab tab : sTabMap.values()) {
            if (tab.getBrowser() == browser) tabs.add(tab);
        }
        return tabs;
    }

    int getId() {
        return mId;
    }

    void setBrowser(Browser browser) {
        mBrowser = browser;
    }

    @NonNull
    public Browser getBrowser() {
        ThreadCheck.ensureOnUiThread();
        return mBrowser;
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

    public void setErrorPageCallback(@Nullable ErrorPageCallback callback) {
        ThreadCheck.ensureOnUiThread();
        try {
            mImpl.setErrorPageCallbackClient(
                    callback == null ? null : new ErrorPageCallbackClientImpl(callback));
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

    @Nullable
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

    /**
     * Runs the beforeunload handler for the main frame or any sub frame, if necessary; otherwise,
     * asynchronously closes the tab.
     *
     * If there is a beforeunload handler a dialog is shown to the user which will allow them to
     * choose whether to proceed with closing the tab. The closure will be notified via {@link
     * NewTabCallback#onCloseTab}. The tab will not close if the user chooses to cancel the action.
     * If there is no beforeunload handler, the tab closure will be asynchronous (but immediate) and
     * will be notified in the same way.
     *
     * To close the tab synchronously without running beforeunload, use {@link Browser#destroyTab}.
     *
     * @since 82
     */
    public void dispatchBeforeUnloadAndClose() {
        ThreadCheck.ensureOnUiThread();
        if (WebLayer.getSupportedMajorVersionInternal() < 82) {
            throw new UnsupportedOperationException();
        }
        try {
            mImpl.dispatchBeforeUnloadAndClose();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Dismisses one active transient UI, if any.
     *
     * This is useful, for example, to handle presses on the system back button. UI such as tab
     * modal dialogs, text selection popups and fullscreen will be dismissed. At most one piece of
     * UI will be dismissed, but this distinction isn't very meaningful in practice since only one
     * such kind of UI would tend to be active at a time.
     *
     * @return true if some piece of UI was dismissed, or false if nothing happened.
     *
     * @since 82
     */
    public boolean dismissTransientUi() {
        ThreadCheck.ensureOnUiThread();
        if (WebLayer.getSupportedMajorVersionInternal() < 82) {
            throw new UnsupportedOperationException();
        }
        try {
            return mImpl.dismissTransientUi();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    public void setNewTabCallback(@Nullable NewTabCallback callback) {
        ThreadCheck.ensureOnUiThread();
        mNewTabCallback = callback;
        try {
            mImpl.setNewTabsEnabled(mNewTabCallback != null);
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

    @NonNull
    public FindInPageController getFindInPageController() {
        ThreadCheck.ensureOnUiThread();
        return mFindInPageController;
    }

    public void registerTabCallback(@Nullable TabCallback callback) {
        ThreadCheck.ensureOnUiThread();
        mCallbacks.addObserver(callback);
    }

    public void unregisterTabCallback(@Nullable TabCallback callback) {
        ThreadCheck.ensureOnUiThread();
        mCallbacks.removeObserver(callback);
    }

    ITab getITab() {
        return mImpl;
    }

    /**
     * Returns a unique id that persists across restarts.
     *
     * @return the unique id.
     * @since 82
     */
    public String getGuid() {
        ThreadCheck.ensureOnUiThread();
        if (WebLayer.getSupportedMajorVersionInternal() < 82) {
            throw new UnsupportedOperationException();
        }
        try {
            return mImpl.getGuid();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    private final class TabClientImpl extends ITabClient.Stub {
        @Override
        public void visibleUriChanged(String uriString) {
            StrictModeWorkaround.apply();
            Uri uri = Uri.parse(uriString);
            for (TabCallback callback : mCallbacks) {
                callback.onVisibleUriChanged(uri);
            }
        }

        @Override
        public void onNewTab(int tabId, int mode) {
            StrictModeWorkaround.apply();
            // This should only be hit if setNewTabCallback() has been called with a non-null
            // value.
            assert mNewTabCallback != null;
            Tab tab = getTabById(tabId);
            // Tab should have already been created by way of BrowserClient.
            assert tab != null;
            assert tab.getBrowser() == getBrowser();
            mNewTabCallback.onNewTab(tab, mode);
        }

        @Override
        public void onCloseTab() {
            StrictModeWorkaround.apply();
            // This should only be hit if setNewTabCallback() has been called with a non-null
            // value.
            assert mNewTabCallback != null;
            mNewTabCallback.onCloseTab();
        }

        @Override
        public void onRenderProcessGone() {
            StrictModeWorkaround.apply();
            for (TabCallback callback : mCallbacks) {
                callback.onRenderProcessGone();
            }
        }

        @Override
        public void showContextMenu(IObjectWrapper pageUrl, IObjectWrapper linkUrl,
                IObjectWrapper linkText, IObjectWrapper titleOrAltText, IObjectWrapper srcUrl) {
            StrictModeWorkaround.apply();
            String pageUrlString = ObjectWrapper.unwrap(pageUrl, String.class);
            String linkUrlString = ObjectWrapper.unwrap(linkUrl, String.class);
            String srcUrlString = ObjectWrapper.unwrap(srcUrl, String.class);
            ContextMenuParams params = new ContextMenuParams(Uri.parse(pageUrlString),
                    linkUrlString != null ? Uri.parse(linkUrlString) : null,
                    ObjectWrapper.unwrap(linkText, String.class),
                    ObjectWrapper.unwrap(titleOrAltText, String.class),
                    srcUrlString != null ? Uri.parse(srcUrlString) : null);
            for (TabCallback callback : mCallbacks) {
                callback.showContextMenu(params);
            }
        }

        @Override
        public void onTabModalStateChanged(boolean isTabModalShowing) {
            StrictModeWorkaround.apply();
            for (TabCallback callback : mCallbacks) {
                callback.onTabModalStateChanged(isTabModalShowing);
            }
        }

        @Override
        public void onTitleUpdated(IObjectWrapper title) {
            StrictModeWorkaround.apply();
            String titleString = ObjectWrapper.unwrap(title, String.class);
            for (TabCallback callback : mCallbacks) {
                callback.onTitleUpdated(titleString);
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
        public boolean interceptDownload(String uriString, String userAgent,
                String contentDisposition, String mimetype, long contentLength) {
            StrictModeWorkaround.apply();
            return mCallback.onInterceptDownload(
                    Uri.parse(uriString), userAgent, contentDisposition, mimetype, contentLength);
        }

        @Override
        public void allowDownload(String uriString, String requestMethod,
                String requestInitiatorString, IObjectWrapper valueCallback) {
            StrictModeWorkaround.apply();
            Uri requestInitiator;
            if (requestInitiatorString != null) {
                requestInitiator = Uri.parse(requestInitiatorString);
            } else {
                requestInitiator = Uri.EMPTY;
            }
            mCallback.allowDownload(Uri.parse(uriString), requestMethod, requestInitiator,
                    (ValueCallback<Boolean>) ObjectWrapper.unwrap(
                            valueCallback, ValueCallback.class));
        }

        @Override
        public IClientDownload createClientDownload(IDownload downloadImpl) {
            StrictModeWorkaround.apply();
            return new Download(downloadImpl);
        }

        @Override
        public void downloadStarted(IClientDownload download) {
            StrictModeWorkaround.apply();
            mCallback.onDownloadStarted((Download) download);
        }

        @Override
        public void downloadProgressChanged(IClientDownload download) {
            StrictModeWorkaround.apply();
            mCallback.onDownloadProgressChanged((Download) download);
        }

        @Override
        public void downloadCompleted(IClientDownload download) {
            StrictModeWorkaround.apply();
            mCallback.onDownloadCompleted((Download) download);
        }

        @Override
        public void downloadFailed(IClientDownload download) {
            StrictModeWorkaround.apply();
            mCallback.onDownloadFailed((Download) download);
        }

        @Override
        public Intent createIntent() {
            StrictModeWorkaround.apply();
            // Intent objects need to be created in the client library so they can refer to the
            // broadcast receiver that will handle them. The broadcast receiver needs to be in the
            // client library because it's referenced in the manifest.
            return new Intent(WebLayer.getAppContext(), DownloadBroadcastReceiver.class);
        }
    }

    private static final class ErrorPageCallbackClientImpl extends IErrorPageCallbackClient.Stub {
        private final ErrorPageCallback mCallback;

        ErrorPageCallbackClientImpl(ErrorPageCallback callback) {
            mCallback = callback;
        }

        public ErrorPageCallback getCallback() {
            return mCallback;
        }

        @Override
        public boolean onBackToSafety() {
            StrictModeWorkaround.apply();
            return mCallback.onBackToSafety();
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
            StrictModeWorkaround.apply();
            ValueCallback<Void> exitFullscreenCallback = (ValueCallback<Void>) ObjectWrapper.unwrap(
                    exitFullscreenWrapper, ValueCallback.class);
            mCallback.onEnterFullscreen(() -> exitFullscreenCallback.onReceiveValue(null));
        }

        @Override
        public void exitFullscreen() {
            StrictModeWorkaround.apply();
            mCallback.onExitFullscreen();
        }
    }
}
