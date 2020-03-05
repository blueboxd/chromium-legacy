// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.os.RemoteException;
import android.support.v4.app.Fragment;
import android.view.View;
import android.webkit.ValueCallback;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.weblayer_private.interfaces.APICallException;
import org.chromium.weblayer_private.interfaces.IBrowser;
import org.chromium.weblayer_private.interfaces.IBrowserClient;
import org.chromium.weblayer_private.interfaces.ITab;
import org.chromium.weblayer_private.interfaces.ObjectWrapper;
import org.chromium.weblayer_private.interfaces.StrictModeWorkaround;

import java.util.Set;

/**
 * Browser contains any number of Tabs, with one active Tab. The active Tab is visible to the user,
 * all other Tabs are hidden.
 *
 * By default Browser has a single active Tab.
 */
public class Browser {
    private final IBrowser mImpl;
    private final ObserverList<TabListCallback> mTabListCallbacks;
    private final UrlBarController mUrlBarController;

    // Constructor for test mocking.
    protected Browser() {
        mImpl = null;
        mTabListCallbacks = null;
        mUrlBarController = null;
    }

    Browser(IBrowser impl) {
        mImpl = impl;
        mTabListCallbacks = new ObserverList<TabListCallback>();

        try {
            mImpl.setClient(new BrowserClientImpl());
            if (WebLayer.getSupportedMajorVersionInternal() >= 82) {
                mUrlBarController = new UrlBarController(mImpl.getUrlBarController());
            } else {
                mUrlBarController = null;
            }
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
        if (WebLayer.getSupportedMajorVersionInternal() < 82) {
            // On WebLayer versions < 82 the tabs are internally created before the client is set,
            // so it doesn't receive the onTabAdded() callbacks; hence the client-side Tab
            // objects need to be manually created to mirror the implementation-side objects.
            try {
                for (Object tab : impl.getTabs()) {
                    // getTabs() returns List<TabImpl>, which isn't accessible from the client
                    // library.
                    ITab iTab = ITab.Stub.asInterface((android.os.IBinder) tab);
                    // Tab's constructor calls registerTab().
                    new Tab(iTab, this);
                }
            } catch (RemoteException e) {
                throw new APICallException(e);
            }
        }
    }

    /**
     * Returns the Browser for the supplied Fragment; null if
     * {@link fragment} was not created by WebLayer.
     *
     * @return the Browser
     */
    @Nullable
    public static Browser fromFragment(@Nullable Fragment fragment) {
        return fragment instanceof BrowserFragment ? ((BrowserFragment) fragment).getBrowser()
                                                   : null;
    }

    // Called prior to notifying IBrowser of destroy().
    void prepareForDestroy() {
        for (Tab tab : getTabs()) {
            Tab.unregisterTab(tab);
        }
    }

    /**
     * Sets the active (visible) Tab. Only one Tab is visible at a time.
     *
     * @param tab The Tab to make active.
     *
     * @throws IllegalStateException if {@link tab} was not added to this
     *         Browser.
     *
     * @see #addTab()
     */
    public void setActiveTab(@NonNull Tab tab) {
        ThreadCheck.ensureOnUiThread();
        try {
            if (getActiveTab() != tab && !mImpl.setActiveTab(tab.getITab())) {
                throw new IllegalStateException("attachTab() must be called before "
                        + "setActiveTab");
            }
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Adds a tab to this Browser. If {link tab} is the active Tab of another Browser, then the
     * other Browser's active tab is set to null. This does nothing if {@link tab} is already
     * contained in this Browser.
     *
     * @param tab The Tab to add.
     */
    public void addTab(@NonNull Tab tab) {
        ThreadCheck.ensureOnUiThread();
        if (tab.getBrowser() == this) return;
        try {
            mImpl.addTab(tab.getITab());
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Returns the active (visible) Tab associated with this
     * Browser.
     *
     * @return The Tab.
     */
    @Nullable
    public Tab getActiveTab() {
        ThreadCheck.ensureOnUiThread();
        try {
            Tab tab = Tab.getTabById(mImpl.getActiveTabId());
            assert tab == null || tab.getBrowser() == this;
            return tab;
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Returns the set of Tabs contained in this Browser.
     *
     * @return The Tabs
     */
    @NonNull
    public Set<Tab> getTabs() {
        ThreadCheck.ensureOnUiThread();
        return Tab.getTabsInBrowser(this);
    }

    /**
     * Disposes a Tab. If {@link tab} is the active Tab, no Tab is made active. After this call
     *  {@link tab} should not be used.
     *
     * Note this will skip any beforeunload handlers. To run those first, use
     * {@link Tab#dispatchBeforeUnloadAndClose} instead.
     *
     * @param tab The Tab to dispose.
     *
     * @throws IllegalStateException is {@link tab} is not in this Browser.
     */
    public void destroyTab(@NonNull Tab tab) {
        ThreadCheck.ensureOnUiThread();
        if (tab.getBrowser() != this) {
            throw new IllegalStateException("destroyTab() must be called on a Tab in the Browser");
        }
        try {
            mImpl.destroyTab(tab.getITab());
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Adds a TabListCallback.
     *
     * @param callback The TabListCallback.
     */
    public void registerTabListCallback(@NonNull TabListCallback callback) {
        ThreadCheck.ensureOnUiThread();
        mTabListCallbacks.addObserver(callback);
    }

    /**
     * Removes a TabListCallback.
     *
     * @param callback The TabListCallback.
     */
    public void unregisterTabListCallback(@NonNull TabListCallback callback) {
        ThreadCheck.ensureOnUiThread();
        mTabListCallbacks.removeObserver(callback);
    }

    /**
     * Sets the View shown at the top of the browser. A value of null removes the view. The
     * top-view is typically used to show the uri. The top-view scrolls with the page.
     *
     * @param view The new top-view.
     */
    public void setTopView(@Nullable View view) {
        ThreadCheck.ensureOnUiThread();
        try {
            mImpl.setTopView(ObjectWrapper.wrap(view));
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Control support for embedding use cases such as animations. This should be enabled when the
     * container view of the fragment is animated in any way, needs to be rotated or blended, or
     * need to control z-order with other views or other BrowserFragmentImpls. Note embedder should
     * keep WebLayer in the default non-embedding mode when user is interacting with the web
     * content. Embedding mode does not support encrypted video.
     *
     * @param enable Whether to support embedding
     * @param callback {@link Callback} to be called with a boolean indicating whether request
     * succeeded. A request might fail if it is subsumed by a subsequent request, or if this object
     * is destroyed.
     */
    public void setSupportsEmbedding(boolean enable, @NonNull Callback<Boolean> callback) {
        ThreadCheck.ensureOnUiThread();
        try {
            mImpl.setSupportsEmbedding(
                    enable, ObjectWrapper.wrap((ValueCallback<Boolean>) callback::onResult));
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Returns {@link Profile} associated with this Browser Fragment. Multiple fragments can share
     * the same Profile.
     */
    @NonNull
    public Profile getProfile() {
        ThreadCheck.ensureOnUiThread();
        try {
            return Profile.of(mImpl.getProfile());
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Returns the UrlBarController.
     * @since 82
     */
    @NonNull
    public UrlBarController getUrlBarController() {
        ThreadCheck.ensureOnUiThread();
        if (WebLayer.getSupportedMajorVersionInternal() < 82) {
            throw new UnsupportedOperationException();
        }
        return mUrlBarController;
    }

    private final class BrowserClientImpl extends IBrowserClient.Stub {
        @Override
        public void onActiveTabChanged(int activeTabId) {
            StrictModeWorkaround.apply();
            Tab tab = Tab.getTabById(activeTabId);
            for (TabListCallback callback : mTabListCallbacks) {
                callback.onActiveTabChanged(tab);
            }
        }

        @Override
        public void onTabAdded(ITab iTab) {
            StrictModeWorkaround.apply();
            int id = 0;
            try {
                id = iTab.getId();
            } catch (RemoteException e) {
                throw new APICallException(e);
            }
            Tab tab = Tab.getTabById(id);
            if (tab == null) {
                tab = new Tab(iTab, Browser.this);
            } else {
                tab.setBrowser(Browser.this);
            }
            for (TabListCallback callback : mTabListCallbacks) {
                callback.onTabAdded(tab);
            }
        }

        @Override
        public void onTabRemoved(int tabId) {
            StrictModeWorkaround.apply();
            Tab tab = Tab.getTabById(tabId);
            // This should only be called with a previously created tab.
            assert tab != null;
            // And this should only be called for tabs attached to this browser.
            assert tab.getBrowser() == Browser.this;

            tab.setBrowser(null);
            for (TabListCallback callback : mTabListCallbacks) {
                callback.onTabRemoved(tab);
            }
        }
    }
}
