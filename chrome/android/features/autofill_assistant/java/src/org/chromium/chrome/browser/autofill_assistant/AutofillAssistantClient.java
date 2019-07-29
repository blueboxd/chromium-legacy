// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.accounts.Account;
import android.content.Context;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.telephony.TelephonyManager;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.OAuth2TokenService;
import org.chromium.content_public.browser.WebContents;

import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * An Autofill Assistant client, associated with a specific WebContents.
 *
 * This mainly a bridge to autofill_assistant::ClientAndroid.
 */
@JNINamespace("autofill_assistant")
class AutofillAssistantClient {
    /** OAuth2 scope that RPCs require. */
    private static final String AUTH_TOKEN_TYPE =
            "oauth2:https://www.googleapis.com/auth/userinfo.profile";
    private static final String PARAMETER_USER_EMAIL = "USER_EMAIL";

    /**
     * Pointer to the corresponding native autofill_assistant::ClientAndroid instance. Might be 0 if
     * the native instance has been deleted. Always check before use.
     */
    private long mNativeClientAndroid;

    /**
     * Indicates whether account initialization was started.
     */
    private boolean mAccountInitializationStarted;

    /**
     * Indicates whether {@link mAccount} has been initialized.
     */
    private boolean mAccountInitialized;

    /**
     * Account that was used to initiate AutofillAssistant.
     *
     * <p>This account is used to  authenticate when sending RPCs and as default account for Payment
     * Request. Not relevant until the accounts have been fetched, and mAccountInitialized set to
     * true. Can still be null after the accounts are fetched, in which case authentication is
     * disabled.
     */
    @Nullable
    private Account mAccount;

    /** If set, fetch the access token once the accounts are fetched. */
    private boolean mShouldFetchAccessToken;

    /** Returns the client for the given web contents, creating it if necessary. */
    public static AutofillAssistantClient fromWebContents(WebContents webContents) {
        return nativeFromWebContents(webContents);
    }

    private AutofillAssistantClient(long nativeClientAndroid) {
        mNativeClientAndroid = nativeClientAndroid;
    }

    private void checkNativeClientIsAliveOrThrow() {
        if (mNativeClientAndroid == 0) {
            throw new IllegalStateException("Native instance is dead");
        }
    }

    /**
     * Start a flow on the current URL, autostarting scripts defined for that URL.
     *
     * <p>This immediately shows the UI, with a loading message, then fetches scripts
     * from the server and autostarts one of them.
     *
     * @param initialUrl the original deep link, if known. When started from CCT, this
     * is the URL included into the intent
     * @param parameters autobot parameters to set during the whole flow
     * @param experimentIds comma-separated set of experiments to use while running the flow
     * @param intentExtras extras of the original intent
     * @param onboardingCoordinator if non-null, reuse existing UI elements, usually created to show
     *         onboarding.
     *
     * @return true if the flow was started, false if the controller is in a state where
     * autostarting is not possible, such as can happen if a script is already running. The flow can
     * still fail after this method returns true; the failure will be displayed on the UI.
     */
    boolean start(String initialUrl, Map<String, String> parameters, String experimentIds,
            Bundle intentExtras, @Nullable AssistantOnboardingCoordinator onboardingCoordinator) {
        if (mNativeClientAndroid == 0) return false;

        checkNativeClientIsAliveOrThrow();
        chooseAccountAsyncIfNecessary(parameters.get(PARAMETER_USER_EMAIL), intentExtras);
        return nativeStart(mNativeClientAndroid, initialUrl, experimentIds,
                parameters.keySet().toArray(new String[parameters.size()]),
                parameters.values().toArray(new String[parameters.size()]), onboardingCoordinator,
                AutofillAssistantServiceInjector.getServiceToInject());
    }

    /**
     * Gets rid of the UI, if there is one. Leaves Autofill Assistant running.
     */
    public void destroyUi() {
        if (mNativeClientAndroid == 0) return;

        nativeDestroyUI(mNativeClientAndroid);
    }

    /**
     * Transfers ownership of the UI to an instance of Autofill Assistant running on
     * the given tab/WebContents. Leaves Autofill Assistant running.
     *
     * <p>If Autofill Assistant is not running on the given WebContents, the UI is destroyed, as if
     * {@link #destroyUi} was called.
     */
    public void transferUiTo(WebContents otherWebContents) {
        if (mNativeClientAndroid == 0) return;

        nativeTransferUITo(mNativeClientAndroid, otherWebContents);
    }

    /** Lists available direct actions. */
    public void listDirectActions(String userName, String experimentIds,
            Map<String, String> arguments, Callback<Set<String>> callback) {
        if (mNativeClientAndroid == 0) {
            callback.onResult(Collections.emptySet());
            return;
        }

        chooseAccountAsyncIfNecessary(userName.isEmpty() ? null : userName, null);

        // The native side calls sendDirectActionList() on the callback once the controller has
        // results.
        nativeListDirectActions(mNativeClientAndroid, experimentIds,
                arguments.keySet().toArray(new String[arguments.size()]),
                arguments.values().toArray(new String[arguments.size()]), callback);
    }

    /**
     * Performs a direct action.
     *
     * @param actionId id of the action
     * @param experimentIds comma-separated set of experiments to use while running the flow
     * @param arguments report these as autobot parameters while performing this specific action
     * @param onboardingcoordinator if non-null, reuse existing UI elements, usually created to show
     *         onboarding.
     * @return true if the action was found started, false otherwise. The action can still fail
     * after this method returns true; the failure will be displayed on the UI.
     */
    public boolean performDirectAction(String actionId, String experimentIds,
            Map<String, String> arguments,
            @Nullable AssistantOnboardingCoordinator onboardingCoordinator) {
        if (mNativeClientAndroid == 0) return false;

        // Note that only listDirectActions can start AA, so only it needs
        // chooseAccountAsyncIfNecessary.
        return nativePerformDirectAction(mNativeClientAndroid, actionId, experimentIds,
                arguments.keySet().toArray(new String[arguments.size()]),
                arguments.values().toArray(new String[arguments.size()]), onboardingCoordinator);
    }

    @CalledByNative
    private static AutofillAssistantClient create(long nativeClientAndroid) {
        return new AutofillAssistantClient(nativeClientAndroid);
    }

    private void chooseAccountAsyncIfNecessary(
            @Nullable String accountFromParameter, @Nullable Bundle extras) {
        if (mAccountInitializationStarted) return;
        mAccountInitializationStarted = true;

        AccountManagerFacade.get().tryGetGoogleAccounts(accounts -> {
            if (mNativeClientAndroid == 0) return;
            if (accounts.size() == 1) {
                // If there's only one account, there aren't any doubts.
                onAccountChosen(accounts.get(0));
                return;
            }
            Account signedIn =
                    findAccountByName(accounts, nativeGetPrimaryAccountName(mNativeClientAndroid));
            if (signedIn != null) {
                // TODO(crbug.com/806868): Compare against account name from extras and complain if
                // they don't match.
                onAccountChosen(signedIn);
                return;
            }

            if (accountFromParameter != null) {
                Account account = findAccountByName(accounts, accountFromParameter);
                if (account != null) {
                    onAccountChosen(account);
                    return;
                }
            }

            if (extras != null) {
                for (String extra : extras.keySet()) {
                    // TODO(crbug.com/806868): Deprecate ACCOUNT_NAME.
                    if (extra.endsWith("ACCOUNT_NAME")) {
                        Account account = findAccountByName(accounts, extras.getString(extra));
                        if (account != null) {
                            onAccountChosen(account);
                            return;
                        }
                    }
                }
            }
            onAccountChosen(null);
        });
    }

    private void onAccountChosen(@Nullable Account account) {
        mAccount = account;
        mAccountInitialized = true;
        // TODO(crbug.com/806868): Consider providing a way of signing in this case, to enforce
        // that all calls are authenticated.

        if (mShouldFetchAccessToken) {
            mShouldFetchAccessToken = false;
            fetchAccessToken();
        }
    }

    private static Account findAccountByName(List<Account> accounts, String name) {
        for (int i = 0; i < accounts.size(); i++) {
            Account account = accounts.get(i);
            if (account.name.equals(name)) {
                return account;
            }
        }
        return null;
    }

    @CalledByNative
    private void fetchAccessToken() {
        if (!mAccountInitialized) {
            // Still getting the account list. Fetch the token as soon as an account is available.
            mShouldFetchAccessToken = true;
            return;
        }
        if (mAccount == null) {
            if (mNativeClientAndroid != 0) nativeOnAccessToken(mNativeClientAndroid, true, "");
            return;
        }

        OAuth2TokenService.getAccessToken(
                mAccount, AUTH_TOKEN_TYPE, new OAuth2TokenService.GetAccessTokenCallback() {
                    @Override
                    public void onGetTokenSuccess(String token) {
                        if (mNativeClientAndroid != 0) {
                            nativeOnAccessToken(mNativeClientAndroid, true, token);
                        }
                    }

                    @Override
                    public void onGetTokenFailure(boolean isTransientError) {
                        if (!isTransientError && mNativeClientAndroid != 0) {
                            nativeOnAccessToken(mNativeClientAndroid, false, "");
                        }
                    }
                });
    }

    @CalledByNative
    private void invalidateAccessToken(String accessToken) {
        if (mAccount == null) {
            return;
        }

        OAuth2TokenService.invalidateAccessToken(accessToken);
    }

    /** Returns the e-mail address that corresponds to the access token or an empty string. */
    @CalledByNative
    private String getAccountEmailAddress() {
        return mAccount != null ? mAccount.name : "";
    }

    /**
     * Returns the country that the device is currently located in. This currently only works
     * for devices with active SIM cards. For a more general solution, we should probably use
     * the LocationManager together with the Geocoder.
     */
    @CalledByNative
    private String getCountryCode() {
        TelephonyManager telephonyManager =
                (TelephonyManager) ContextUtils.getApplicationContext().getSystemService(
                        Context.TELEPHONY_SERVICE);

        // According to API, location for CDMA networks is unreliable
        if (telephonyManager != null
                && telephonyManager.getPhoneType() != TelephonyManager.PHONE_TYPE_CDMA)
            return telephonyManager.getNetworkCountryIso();

        return null;
    }

    /** Adds a dynamic action to the given reporter. */
    @CalledByNative
    private void sendDirectActionList(Callback<Set<String>> callback, String[] names) {
        Set<String> set = new HashSet<>();
        for (String name : names) {
            set.add(name);
        }
        callback.onResult(set);
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativeClientAndroid = 0;
    }

    private static native AutofillAssistantClient nativeFromWebContents(WebContents webContents);
    private native boolean nativeStart(long nativeClientAndroid, String initialUrl,
            String experimentIds, String[] parameterNames, String[] parameterValues,
            @Nullable AssistantOnboardingCoordinator onboardingCoordinator, long nativeService);
    private native void nativeOnAccessToken(
            long nativeClientAndroid, boolean success, String accessToken);
    private native String nativeGetPrimaryAccountName(long nativeClientAndroid);
    private native void nativeDestroyUI(long nativeClientAndroid);
    private native void nativeTransferUITo(long nativeClientAndroid, Object otherWebContents);
    private native void nativeListDirectActions(long nativeClientAndroid, String experimentIds,
            String[] argumentNames, String[] argumentValues, Object callback);
    private native boolean nativePerformDirectAction(long nativeClientAndroid, String actionId,
            String experimentId, String[] argumentNames, String[] argumentValues,
            @Nullable AssistantOnboardingCoordinator onboardingCoordinator);
}
