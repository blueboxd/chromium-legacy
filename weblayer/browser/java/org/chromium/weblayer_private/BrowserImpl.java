// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.RemoteException;
import android.provider.Settings;
import android.view.View;
import android.view.ViewGroup;
import android.webkit.ValueCallback;

import org.chromium.base.ObserverList;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.weblayer_private.interfaces.APICallException;
import org.chromium.weblayer_private.interfaces.IBrowser;
import org.chromium.weblayer_private.interfaces.IBrowserClient;
import org.chromium.weblayer_private.interfaces.IObjectWrapper;
import org.chromium.weblayer_private.interfaces.IProfile;
import org.chromium.weblayer_private.interfaces.ITab;
import org.chromium.weblayer_private.interfaces.IUrlBarController;
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
    private final ObserverList<VisibleSecurityStateObserver> mVisibleSecurityStateObservers =
            new ObserverList<VisibleSecurityStateObserver>();

    // Key used to save the crypto key in instance state.
    public static final String SAVED_STATE_SESSION_SERVICE_CRYPTO_KEY =
            "SAVED_STATE_SESSION_SERVICE_CRYPTO_KEY";

    // Key used to save the minimal persistence state in instance state. Only used if a persistence
    // id was not specified.
    public static final String SAVED_STATE_MINIMAL_PERSISTENCE_STATE_KEY =
            "SAVED_STATE_MINIMAL_PERSISTENCE_STATE_KEY";

    private long mNativeBrowser;
    private final ProfileImpl mProfile;
    private BrowserViewController mViewController;
    private FragmentWindowAndroid mWindowAndroid;
    private IBrowserClient mClient;
    private LocaleChangedBroadcastReceiver mLocaleReceiver;
    private boolean mInDestroy;
    private final UrlBarControllerImpl mUrlBarController;
    private boolean mFragmentStarted;
    private boolean mFragmentResumed;
    // Cache the value instead of querying system every time.
    private Boolean mPasswordEchoEnabled;

    // Created in the constructor from saved state and used in setClient().
    private PersistenceInfo mPersistenceInfo;

    private static final class PersistenceInfo {
        String mPersistenceId;
        byte[] mCryptoKey;
        byte[] mMinimalPersistenceState;
    };

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

    /**
     * Allows observing of visible security state of the active tab.
     */
    public static interface VisibleSecurityStateObserver {
        public void onVisibleSecurityStateOfActiveTabChanged();
    }
    public void addVisibleSecurityStateObserver(VisibleSecurityStateObserver observer) {
        mVisibleSecurityStateObservers.addObserver(observer);
    }
    public void removeVisibleSecurityStateObserver(VisibleSecurityStateObserver observer) {
        mVisibleSecurityStateObservers.removeObserver(observer);
    }

    public BrowserImpl(ProfileImpl profile, String persistenceId, Bundle savedInstanceState,
            FragmentWindowAndroid windowAndroid) {
        profile.checkNotDestroyed();
        mProfile = profile;

        mPersistenceInfo = new PersistenceInfo();
        mPersistenceInfo.mPersistenceId = persistenceId;
        mPersistenceInfo.mCryptoKey = savedInstanceState != null
                ? savedInstanceState.getByteArray(SAVED_STATE_SESSION_SERVICE_CRYPTO_KEY)
                : null;
        mPersistenceInfo.mMinimalPersistenceState =
                (savedInstanceState != null && (persistenceId == null || persistenceId.isEmpty()))
                ? savedInstanceState.getByteArray(SAVED_STATE_MINIMAL_PERSISTENCE_STATE_KEY)
                : null;

        createAttachmentState(windowAndroid);
        mNativeBrowser = BrowserImplJni.get().createBrowser(profile.getNativeProfile(), this);
        mUrlBarController = new UrlBarControllerImpl(this, mNativeBrowser);

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

    // Called from constructor and onFragmentAttached() to configure state needed when attached.
    private void createAttachmentState(FragmentWindowAndroid windowAndroid) {
        assert mViewController == null;
        assert mWindowAndroid == null;
        mWindowAndroid = windowAndroid;
        mViewController = new BrowserViewController(windowAndroid);
        mLocaleReceiver = new LocaleChangedBroadcastReceiver(windowAndroid.getContext().get());
        mPasswordEchoEnabled = null;
    }

    public void onFragmentAttached(FragmentWindowAndroid windowAndroid) {
        createAttachmentState(windowAndroid);
        updateAllTabsAndSetActive();
    }

    public void onFragmentDetached() {
        destroyAttachmentState();
        updateAllTabs();
    }

    public void onSaveInstanceState(Bundle outState) {
        boolean hasPersistenceId =
                !BrowserImplJni.get().getPersistenceId(mNativeBrowser, this).isEmpty();
        if (mProfile.isIncognito() && hasPersistenceId) {
            // Trigger a save now as saving may generate a new crypto key. This doesn't actually
            // save synchronously, rather triggers a save on a background task runner.
            BrowserImplJni.get().saveBrowserPersisterIfNecessary(mNativeBrowser, this);
            outState.putByteArray(SAVED_STATE_SESSION_SERVICE_CRYPTO_KEY,
                    BrowserImplJni.get().getBrowserPersisterCryptoKey(mNativeBrowser, this));
        } else if (!hasPersistenceId) {
            outState.putByteArray(SAVED_STATE_MINIMAL_PERSISTENCE_STATE_KEY,
                    BrowserImplJni.get().getMinimalPersistenceState(mNativeBrowser, this));
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

    public boolean isWindowOnSmallDevice() {
        assert mWindowAndroid != null;
        return !DeviceFormFactor.isWindowOnTablet(mWindowAndroid);
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

    private void checkPasswordEchoEnabled() {
        if (mPasswordEchoEnabled == null) return;
        boolean oldEnabled = mPasswordEchoEnabled;
        mPasswordEchoEnabled = null;
        boolean newEnabled = getPasswordEchoEnabled();
        if (oldEnabled != newEnabled) {
            BrowserImplJni.get().webPreferencesChanged(mNativeBrowser);
        }
    }

    @CalledByNative
    private boolean getPasswordEchoEnabled() {
        Context context = getContext();
        if (context == null) return false;
        if (mPasswordEchoEnabled == null) {
            mPasswordEchoEnabled = Settings.System.getInt(context.getContentResolver(),
                                           Settings.System.TEXT_SHOW_PASSWORD, 1)
                    == 1;
        }
        return mPasswordEchoEnabled;
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

    @CalledByNative
    private void onVisibleSecurityStateOfActiveTabChanged() {
        for (VisibleSecurityStateObserver observer : mVisibleSecurityStateObservers) {
            observer.onVisibleSecurityStateOfActiveTabChanged();
        }
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

        // This function is called from the client once everything has been setup (meaning all the
        // client classes have been created and AIDL interfaces established in both directions).
        // This function is called immediately after the constructor of BrowserImpl from the client.
        assert mPersistenceInfo != null;
        PersistenceInfo persistenceInfo = mPersistenceInfo;
        mPersistenceInfo = null;
        BrowserImplJni.get().restoreStateIfNecessary(mNativeBrowser, this,
                persistenceInfo.mPersistenceId, persistenceInfo.mCryptoKey,
                persistenceInfo.mMinimalPersistenceState);

        if (getTabs().size() > 0) {
            updateAllTabsAndSetActive();
        } else if (persistenceInfo.mPersistenceId == null
                || persistenceInfo.mPersistenceId.isEmpty()) {
            TabImpl tab = new TabImpl(mProfile, mWindowAndroid);
            addTab(tab);
            boolean set_active_result = setActiveTab(tab);
            assert set_active_result;
        } // else case is session restore, which will asynchronously create tabs.
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

    @Override
    public IUrlBarController getUrlBarController() {
        StrictModeWorkaround.apply();
        return mUrlBarController;
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

        // mUrlBarController keeps a reference to mNativeBrowser, and hence must be destroyed before
        // mNativeBrowser.
        mUrlBarController.destroy();
        BrowserImplJni.get().deleteBrowser(mNativeBrowser);
    }

    public void onFragmentStart() {
        mFragmentStarted = true;
        updateAllTabs();
        checkPasswordEchoEnabled();
    }

    public void onFragmentStop() {
        mFragmentStarted = false;
        updateAllTabs();
    }

    public void onFragmentResume() {
        mFragmentResumed = true;
    }

    public void onFragmentPause() {
        mFragmentResumed = false;
    }

    public boolean isStarted() {
        return mFragmentStarted;
    }

    public boolean isResumed() {
        return mFragmentResumed;
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

        mVisibleSecurityStateObservers.clear();
    }

    private void updateAllTabsAndSetActive() {
        if (getTabs().size() > 0) {
            updateAllTabs();
            mViewController.setActiveTab(getActiveTab());
        }
    }

    private void updateAllTabs() {
        for (Object tab : getTabs()) {
            ((TabImpl) tab).updateFromBrowser();
        }
    }

    @NativeMethods
    interface Natives {
        long createBrowser(long profile, BrowserImpl caller);
        void deleteBrowser(long browser);
        void addTab(long nativeBrowserImpl, BrowserImpl browser, long nativeTab);
        void removeTab(long nativeBrowserImpl, BrowserImpl browser, long nativeTab);
        TabImpl[] getTabs(long nativeBrowserImpl, BrowserImpl browser);
        void setActiveTab(long nativeBrowserImpl, BrowserImpl browser, long nativeTab);
        TabImpl getActiveTab(long nativeBrowserImpl, BrowserImpl browser);
        void prepareForShutdown(long nativeBrowserImpl, BrowserImpl browser);
        String getPersistenceId(long nativeBrowserImpl, BrowserImpl browser);
        void saveBrowserPersisterIfNecessary(long nativeBrowserImpl, BrowserImpl browser);
        byte[] getBrowserPersisterCryptoKey(long nativeBrowserImpl, BrowserImpl browser);
        byte[] getMinimalPersistenceState(long nativeBrowserImpl, BrowserImpl browser);
        void restoreStateIfNecessary(long nativeBrowserImpl, BrowserImpl browser,
                String persistenceId, byte[] persistenceCryptoKey, byte[] minimalPersistenceState);
        void webPreferencesChanged(long nativeBrowserImpl);
    }
}
