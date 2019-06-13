// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.accounts.Account;
import android.app.Activity;
import android.content.Context;

import org.chromium.base.annotations.JCaller;

/**
 * Interface providing SigninManager access to dependencies that are not part of the SignIn
 * component. This interface interacts with //chrome features such as Policy, Sync, data wiping,
 * Google Play services.
 */
public interface SigninManagerDelegate {
    /**
     * If there is no Google Play Services available, ask the user to fix by showing either a
     * notification or a modal dialog
     * @param activity The activity used to open the dialog, or null to use notifications
     * @param cancelable Whether the dialog can be canceled
     */
    public void handleGooglePlayServicesUnavailability(Activity activity, boolean cancelable);

    /**
     * @return Whether the device has Google Play Services.
     */
    public boolean isGooglePlayServicesPresent(Context context);

    /**
     * Interact with the UserPolicySigninService to retrieve the user policy.
     * @param signinManager a reference on SigninManager used for the native calls
     * @param nativeSigninManagerAndroid a reference on the native SigninManager used for native
     *                                   calls
     * @param username (email) of the user signing in.
     */
    public void fetchAndApplyCloudPolicy(
            @JCaller SigninManager signinManager, long nativeSigninManagerAndroid, String username);

    /**
     * Perform the required cloud policy cleanup when a signin is aborted.
     * @param signinManager a reference on SigninManager used for the native calls
     * @param nativeSigninManagerAndroid a reference on the native SigninManager used for native
     *                                   calls
     */
    public void stopApplyingCloudPolicy(
            @JCaller SigninManager signinManager, long nativeSigninManagerAndroid);

    /**
     * Called AFTER native sign-in is complete, enabling Sync.
     * @param account to be used by sync
     */
    public void enableSync(Account account);

    /**
     * Called AFTER native sign-out is complete, this method clears various
     * account and profile data associated with the previous signin and aborts sync.
     * @param signinManager a reference on SigninManager used for the native calls
     * @param nativeSigninManagerAndroid a reference on the native SigninManager used for native
     *                                   calls
     * @param isManaged if the account is managed, which triggers a different cleanup flow
     * @param wipeDataCallback to be called once profile data cleanup is complete
     */
    public void disableSyncAndWipeData(@JCaller SigninManager signinManager,
            long nativeSigninManagerAndroid, boolean isManaged, Runnable wipeDataCallback);
}
