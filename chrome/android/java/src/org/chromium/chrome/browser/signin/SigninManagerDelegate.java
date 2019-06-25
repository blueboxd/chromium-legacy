// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.accounts.Account;
import android.app.Activity;
import android.content.Context;

import org.chromium.base.Callback;
import org.chromium.base.annotations.JCaller;

/**
 * Interface providing SigninManager access to dependencies that are not part of the SignIn
 * component. This interface interacts with //chrome features such as Policy, Sync, data wiping,
 * Google Play services.
 */
public interface SigninManagerDelegate {
    /**
     * Perform destruction of the object, including destructing the associated native object.
     */
    public void destroy();

    /**
     * If there is no Google Play Services available, ask the user to fix by showing either a
     * notification or a modal dialog
     * @param activity The activity used to open the dialog, or null to use notifications
     * @param cancelable Whether the dialog can be canceled
     */
    public void handleGooglePlayServicesUnavailability(Activity activity, boolean cancelable);

    /**
     * @return the management domain if the signed in account is managed, otherwise null.
     */
    public String getManagementDomain();

    /**
     * @return Whether the device has Google Play Services.
     */
    public boolean isGooglePlayServicesPresent(Context context);

    /**
     * Verifies if the account is managed. Callback may be called either synchronously or
     * asynchronously depending on the availability of the result.
     * @param email An email of the account.
     * @param callback The callback that will receive true if the account is managed, false
     *                 otherwise.
     */
    public void isAccountManaged(String email, final Callback<Boolean> callback);

    /**
     * Interact with the UserPolicySigninService to retrieve the user policy.
     * @param username (email) of the user signing in.
     * @param callback The callback called once the policy is retrieved and applied
     */
    public void fetchAndApplyCloudPolicy(String username, Runnable callback);

    /**
     * Perform the required cloud policy cleanup when a signin is aborted.
     */
    public void stopApplyingCloudPolicy();

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
