// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.accounts.Account;
import android.app.Activity;
import android.content.Context;

import org.chromium.base.Callback;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.JCaller;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.externalauth.ExternalAuthUtils;
import org.chromium.chrome.browser.externalauth.UserRecoverableErrorHandler;
import org.chromium.components.sync.AndroidSyncSettings;

/**
 * This implementation of {@link SigninManagerDelegate} provides {@link SigninManager} access to
 * //chrome/browser level dependencies.
 */
public class ChromeSigninManagerDelegate implements SigninManagerDelegate {
    private final AndroidSyncSettings mAndroidSyncSettings;
    private long mNativeChromeSigninManagerDelegate;

    public ChromeSigninManagerDelegate() {
        this(AndroidSyncSettings.get());
    }

    @VisibleForTesting
    ChromeSigninManagerDelegate(AndroidSyncSettings androidSyncSettings) {
        assert androidSyncSettings != null;
        mAndroidSyncSettings = androidSyncSettings;
        mNativeChromeSigninManagerDelegate = ChromeSigninManagerDelegateJni.get().init(this);
    }

    @Override
    public void destroy() {
        if (mNativeChromeSigninManagerDelegate != 0) {
            ChromeSigninManagerDelegateJni.get().destroy(this, mNativeChromeSigninManagerDelegate);
            mNativeChromeSigninManagerDelegate = 0;
        }
    }

    @Override
    public String getManagementDomain() {
        return ChromeSigninManagerDelegateJni.get().getManagementDomain(
                this, mNativeChromeSigninManagerDelegate);
    }

    @Override
    public void handleGooglePlayServicesUnavailability(Activity activity, boolean cancelable) {
        UserRecoverableErrorHandler errorHandler = activity != null
                ? new UserRecoverableErrorHandler.ModalDialog(activity, cancelable)
                : new UserRecoverableErrorHandler.SystemNotification();
        ExternalAuthUtils.getInstance().canUseGooglePlayServices(errorHandler);
    }

    @Override
    public boolean isGooglePlayServicesPresent(Context context) {
        return !ExternalAuthUtils.getInstance().isGooglePlayServicesMissing(context);
    }

    @Override
    public void isAccountManaged(String email, final Callback<Boolean> callback) {
        ChromeSigninManagerDelegateJni.get().isAccountManaged(
                this, mNativeChromeSigninManagerDelegate, email, callback);
    }

    @Override
    public void fetchAndApplyCloudPolicy(String username, final Runnable callback) {
        ChromeSigninManagerDelegateJni.get().fetchAndApplyCloudPolicy(
                this, mNativeChromeSigninManagerDelegate, username, callback);
    }

    @Override
    public void stopApplyingCloudPolicy() {
        ChromeSigninManagerDelegateJni.get().stopApplyingCloudPolicy(
                this, mNativeChromeSigninManagerDelegate);
    }

    @Override
    public void enableSync(Account account) {
        // Cache the signed-in account name. This must be done after the native call, otherwise
        // sync tries to start without being signed in natively and crashes.
        mAndroidSyncSettings.updateAccount(account);
        mAndroidSyncSettings.enableChromeSync();
    }

    @Override
    public void disableSyncAndWipeData(@JCaller SigninManager signinManager,
            long nativeSigninManagerAndroid, boolean isManagedOrForceWipe,
            final Runnable wipeDataCallback) {
        mAndroidSyncSettings.updateAccount(null);
        if (isManagedOrForceWipe) {
            SigninManagerJni.get().wipeProfileData(
                    signinManager, nativeSigninManagerAndroid, wipeDataCallback);
        } else {
            SigninManagerJni.get().wipeGoogleServiceWorkerCaches(
                    signinManager, nativeSigninManagerAndroid, wipeDataCallback);
        }
    }

    // Native methods.
    @NativeMethods
    interface Natives {
        long init(@JCaller ChromeSigninManagerDelegate self);

        void destroy(
                @JCaller ChromeSigninManagerDelegate self, long nativeChromeSigninManagerDelegate);

        void fetchAndApplyCloudPolicy(@JCaller ChromeSigninManagerDelegate self,
                long nativeChromeSigninManagerDelegate, String username, Runnable callback);

        void stopApplyingCloudPolicy(
                @JCaller ChromeSigninManagerDelegate self, long nativeChromeSigninManagerDelegate);

        void isAccountManaged(@JCaller ChromeSigninManagerDelegate self,
                long nativeChromeSigninManagerDelegate, String username,
                Callback<Boolean> callback);

        String getManagementDomain(
                @JCaller ChromeSigninManagerDelegate self, long nativeChromeSigninManagerDelegate);
    }
}
