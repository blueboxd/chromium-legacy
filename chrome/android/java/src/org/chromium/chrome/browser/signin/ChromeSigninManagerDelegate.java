// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.accounts.Account;
import android.app.Activity;
import android.content.Context;

import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.JCaller;
import org.chromium.chrome.browser.externalauth.ExternalAuthUtils;
import org.chromium.chrome.browser.externalauth.UserRecoverableErrorHandler;
import org.chromium.components.sync.AndroidSyncSettings;

/**
 * This implementation of {@link SigninManagerDelegate} provides {@link SigninManager} access to
 * //chrome/browser level dependencies.
 */
public class ChromeSigninManagerDelegate implements SigninManagerDelegate {
    private final AndroidSyncSettings mAndroidSyncSettings;

    public ChromeSigninManagerDelegate() {
        this(AndroidSyncSettings.get());
    }

    @VisibleForTesting
    ChromeSigninManagerDelegate(AndroidSyncSettings androidSyncSettings) {
        assert androidSyncSettings != null;
        mAndroidSyncSettings = androidSyncSettings;
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
    public void fetchAndApplyCloudPolicy(@JCaller SigninManager signinManager,
            long nativeSigninManagerAndroid, String username) {
        SigninManagerJni.get().fetchAndApplyCloudPolicy(
                signinManager, nativeSigninManagerAndroid, username);
    }

    @Override
    public void stopApplyingCloudPolicy(
            @JCaller SigninManager signinManager, long nativeSigninManagerAndroid) {
        SigninManagerJni.get().abortSignIn(signinManager, nativeSigninManagerAndroid);
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
            long nativeSigninManagerAndroid, boolean isManaged, final Runnable wipeDataCallback) {
        mAndroidSyncSettings.updateAccount(null);
        if (isManaged) {
            SigninManagerJni.get().wipeProfileData(
                    signinManager, nativeSigninManagerAndroid, wipeDataCallback);
        } else {
            SigninManagerJni.get().wipeGoogleServiceWorkerCaches(
                    signinManager, nativeSigninManagerAndroid, wipeDataCallback);
        }
    }
}
