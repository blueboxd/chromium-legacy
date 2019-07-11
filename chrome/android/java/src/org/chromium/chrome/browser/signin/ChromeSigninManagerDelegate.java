// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.accounts.Account;
import android.app.Activity;
import android.content.Context;

import org.chromium.base.Callback;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
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

    @CalledByNative
    private static ChromeSigninManagerDelegate create(long nativeChromeSigninManagerDelegate) {
        assert nativeChromeSigninManagerDelegate != 0;
        return new ChromeSigninManagerDelegate(
                nativeChromeSigninManagerDelegate, AndroidSyncSettings.get());
    }

    @CalledByNative
    public void destroy() {
        mNativeChromeSigninManagerDelegate = 0;
    }

    @VisibleForTesting
    ChromeSigninManagerDelegate(
            long nativeChromeSigninManagerDelegate, AndroidSyncSettings androidSyncSettings) {
        assert androidSyncSettings != null;
        mAndroidSyncSettings = androidSyncSettings;
        mNativeChromeSigninManagerDelegate = nativeChromeSigninManagerDelegate;
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
    public void disableSyncAndWipeData(
            boolean isManagedOrForceWipe, final Runnable wipeDataCallback) {
        mAndroidSyncSettings.updateAccount(null);
        if (isManagedOrForceWipe) {
            ChromeSigninManagerDelegateJni.get().wipeProfileData(
                    this, mNativeChromeSigninManagerDelegate, wipeDataCallback);
        } else {
            ChromeSigninManagerDelegateJni.get().wipeGoogleServiceWorkerCaches(
                    this, mNativeChromeSigninManagerDelegate, wipeDataCallback);
        }
    }

    // Native methods.
    @NativeMethods
    interface Natives {
        void fetchAndApplyCloudPolicy(@JCaller ChromeSigninManagerDelegate self,
                long nativeChromeSigninManagerDelegate, String username, Runnable callback);

        void stopApplyingCloudPolicy(
                @JCaller ChromeSigninManagerDelegate self, long nativeChromeSigninManagerDelegate);

        void isAccountManaged(@JCaller ChromeSigninManagerDelegate self,
                long nativeChromeSigninManagerDelegate, String username,
                Callback<Boolean> callback);

        String getManagementDomain(
                @JCaller ChromeSigninManagerDelegate self, long nativeChromeSigninManagerDelegate);

        void wipeProfileData(@JCaller ChromeSigninManagerDelegate self,
                long nativeChromeSigninManagerDelegate, Runnable callback);

        void wipeGoogleServiceWorkerCaches(@JCaller ChromeSigninManagerDelegate self,
                long nativeChromeSigninManagerDelegate, Runnable callback);
    }
}
