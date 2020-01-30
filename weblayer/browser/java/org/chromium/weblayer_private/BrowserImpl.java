// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.RemoteException;
import android.view.View;
import android.view.ViewGroup;
import android.webkit.ValueCallback;

import org.chromium.base.ObserverList;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.weblayer_private.interfaces.APICallException;
import org.chromium.weblayer_private.interfaces.IBrowser;
import org.chromium.weblayer_private.interfaces.IBrowserClient;
import org.chromium.weblayer_private.interfaces.IObjectWrapper;
import org.chromium.weblayer_private.interfaces.IProfile;
import org.chromium.weblayer_private.interfaces.ITab;
import org.chromium.weblayer_private.interfaces.ObjectWrapper;
import org.chromium.weblayer_private.interfaces.StrictModeWorkaround;

import java.util.Arrays;
import java.util.List;

/**
 * Implementation of {@link IBrowser}.
 */
@JNINamespace("weblayer")
public class BrowserImpl extends IBrowser.Stub {
    private static final ObserverList<Observer> sLifecycleObservers = new ObserverList<Observer>();

    public static final String SAVED_STATE_SESSION_SERVICE_CRYPTO_KEY =
            "SAVED_STATE_SESSION_SERVICE_CRYPTO_KEY";

    private long mNativeBrowser;
    private final ProfileImpl mProfile;
    private BrowserViewController mViewController;
    private FragmentWindowAndroid mWindowAndroid;
    private IBrowserClient mClient;
    private LocaleChangedBroadcastReceiver mLocaleReceiver;
    private boolean mInDestroy;

    /**
     * Observer interface that can be implemented to observe when the first
     * fragment requiring WebLayer is attached, and when the last such fragment
     * is detached.
     */
    public static interface Observer {
        public void onBrowserCreated();
        public void onBrowserDestroyed();
    }

    public static void addObserver(Observer observer) {
        sLifecycleObservers.addObserver(observer);
    }

    public static void removeObserver(Observer observer) {
        sLifecycleObservers.removeObserver(observer);
    }

    public BrowserImpl(ProfileImpl profile, String persistenceId, Bundle savedInstanceState) {
        mProfile = profile;
        byte[] cryptoKey = savedInstanceState != null
                ? savedInstanceState.getByteArray(SAVED_STATE_SESSION_SERVICE_CRYPTO_KEY)
                : null;
        mNativeBrowser = BrowserImplJni.get().createBrowser(
                profile.getNativeProfile(), persistenceId, cryptoKey, this);

        for (Observer observer : sLifecycleObservers) {
            observer.onBrowserCreated();
        }
    }

    public WindowAndroid getWindowAndroid() {
        return mWindowAndroid;
    }

    public ViewGroup getViewAndroidDelegateContainerView() {
        if (mViewController == null) return null;
        return mViewController.getContentView();
    }

    public void onFragmentAttached(Context context, FragmentWindowAndroid windowAndroid) {
        assert mWindowAndroid == null;
        assert mViewController == null;
        mWindowAndroid = windowAndroid;
        mViewController = new BrowserViewController(context, windowAndroid);
        mLocaleReceiver = new LocaleChangedBroadcastReceiver(context);

        if (getTabs().size() > 0) {
            updateAllTabs();
            mViewController.setActiveTab(getActiveTab());
        } else if (BrowserImplJni.get().getPersistenceId(mNativeBrowser, this).isEmpty()) {
            TabImpl tab = new TabImpl(mProfile, windowAndroid);
            addTab(tab);
            boolean set_active_result = setActiveTab(tab);
            assert set_active_result;
        } // else case is session restore, which will asynchronously create tabs.
    }

    public void onFragmentDetached() {
        destroyAttachmentState();
        updateAllTabs();
    }

    public void onSaveInstanceState(Bundle outState) {
        if (mProfile.isIncognito()
                && !BrowserImplJni.get().getPersistenceId(mNativeBrowser, this).isEmpty()) {
            // Trigger a save now as saving may generate a new crypto key. This doesn't actually
            // save synchronously, rather triggers a save on a background task runner.
            BrowserImplJni.get().saveSessionServiceIfNecessary(mNativeBrowser, this);
            outState.putByteArray(SAVED_STATE_SESSION_SERVICE_CRYPTO_KEY,
                    BrowserImplJni.get().getSessionServiceCryptoKey(mNativeBrowser, this));
        }
    }

    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (mWindowAndroid != null) {
            mWindowAndroid.onActivityResult(requestCode, resultCode, data);
        }
    }

    public void onRequestPermissionsResult(
            int requestCode, String[] permissions, int[] grantResults) {
        if (mWindowAndroid != null) {
            mWindowAndroid.handlePermissionResult(requestCode, permissions, grantResults);
        }
    }

    @Override
    public void setTopView(IObjectWrapper viewWrapper) {
        StrictModeWorkaround.apply();
        getViewController().setTopView(ObjectWrapper.unwrap(viewWrapper, View.class));
    }

    @Override
    public void setSupportsEmbedding(boolean enable, IObjectWrapper valueCallback) {
        StrictModeWorkaround.apply();
        getViewController().setSupportsEmbedding(enable,
                (ValueCallback<Boolean>) ObjectWrapper.unwrap(valueCallback, ValueCallback.class));
    }

    public BrowserViewController getViewController() {
        if (mViewController == null) {
            throw new RuntimeException("Currently Tab requires Activity context, so "
                    + "it exists only while BrowserFragment is attached to an Activity");
        }
        return mViewController;
    }

    public Context getContext() {
        if (mWindowAndroid == null) {
            return null;
        }

        return mWindowAndroid.getContext().get();
    }

    @Override
    public IProfile getProfile() {
        StrictModeWorkaround.apply();
        return mProfile;
    }

    @Override
    public void addTab(ITab iTab) {
        StrictModeWorkaround.apply();
        TabImpl tab = (TabImpl) iTab;
        if (tab.getBrowser() == this) return;
        BrowserImplJni.get().addTab(mNativeBrowser, this, tab.getNativeTab());
    }

    @CalledByNative
    private void createTabForSessionRestore(long nativeTab) {
        new TabImpl(mProfile, mWindowAndroid, nativeTab);
    }

    @CalledByNative
    private void onTabAdded(TabImpl tab) {
        tab.attachToBrowser(this);
        try {
            if (mClient != null) mClient.onTabAdded(tab);
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @CalledByNative
    private void onActiveTabChanged(TabImpl tab) {
        mViewController.setActiveTab(tab);
        if (mInDestroy) return;
        try {
            if (mClient != null) {
                mClient.onActiveTabChanged(tab != null ? tab.getId() : 0);
            }
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @CalledByNative
    private void onTabRemoved(TabImpl tab) {
        if (mInDestroy) return;
        try {
            if (mClient != null) mClient.onTabRemoved(tab.getId());
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
        // This doesn't reset state on TabImpl as |browser| is either about to be
        // destroyed, or switching to a different fragment.
    }

    @Override
    public boolean setActiveTab(ITab controller) {
        StrictModeWorkaround.apply();
        TabImpl tab = (TabImpl) controller;
        if (tab != null && tab.getBrowser() != this) return false;
        BrowserImplJni.get().setActiveTab(
                mNativeBrowser, this, tab != null ? tab.getNativeTab() : 0);
        return true;
    }

    public TabImpl getActiveTab() {
        return BrowserImplJni.get().getActiveTab(mNativeBrowser, this);
    }

    @Override
    public List getTabs() {
        StrictModeWorkaround.apply();
        return Arrays.asList(BrowserImplJni.get().getTabs(mNativeBrowser, this));
    }

    @Override
    public int getActiveTabId() {
        StrictModeWorkaround.apply();
        return getActiveTab() != null ? getActiveTab().getId() : 0;
    }

    @Override
    public void setClient(IBrowserClient client) {
        StrictModeWorkaround.apply();
        mClient = client;
    }

    @Override
    public void destroyTab(ITab iTab) {
        StrictModeWorkaround.apply();
        TabImpl tab = (TabImpl) iTab;
        if (tab.getBrowser() != this) return;
        destroyTabImpl((TabImpl) iTab);
    }

    private void destroyTabImpl(TabImpl tab) {
        BrowserImplJni.get().removeTab(mNativeBrowser, this, tab.getNativeTab());
        tab.destroy();
    }

    public View getFragmentView() {
        return getViewController().getView();
    }

    public void destroy() {
        mInDestroy = true;
        BrowserImplJni.get().prepareForShutdown(mNativeBrowser, this);
        setActiveTab(null);
        for (Object tab : getTabs()) {
            destroyTabImpl((TabImpl) tab);
        }
        destroyAttachmentState();
        for (Observer observer : sLifecycleObservers) {
            observer.onBrowserDestroyed();
        }
        BrowserImplJni.get().deleteBrowser(mNativeBrowser);
    }

    private void destroyAttachmentState() {
        if (mLocaleReceiver != null) {
            mLocaleReceiver.destroy();
            mLocaleReceiver = null;
        }
        if (mViewController != null) {
            mViewController.destroy();
            mViewController = null;
        }
        if (mWindowAndroid != null) {
            mWindowAndroid.destroy();
            mWindowAndroid = null;
        }
    }

    private void updateAllTabs() {
        for (Object tab : getTabs()) {
            ((TabImpl) tab).updateFromBrowser();
        }
    }

    @NativeMethods
    interface Natives {
        long createBrowser(long profile, String persistenceId, byte[] persistenceCryptoKey,
                BrowserImpl caller);
        void deleteBrowser(long browser);
        void addTab(long nativeBrowserImpl, BrowserImpl browser, long nativeTab);
        void removeTab(long nativeBrowserImpl, BrowserImpl browser, long nativeTab);
        TabImpl[] getTabs(long nativeBrowserImpl, BrowserImpl browser);
        void setActiveTab(long nativeBrowserImpl, BrowserImpl browser, long nativeTab);
        TabImpl getActiveTab(long nativeBrowserImpl, BrowserImpl browser);
        void prepareForShutdown(long nativeBrowserImpl, BrowserImpl browser);
        String getPersistenceId(long nativeBrowserImpl, BrowserImpl browser);
        void saveSessionServiceIfNecessary(long nativeBrowserImpl, BrowserImpl browser);
        byte[] getSessionServiceCryptoKey(long nativeBrowserImpl, BrowserImpl browser);
    }
}
