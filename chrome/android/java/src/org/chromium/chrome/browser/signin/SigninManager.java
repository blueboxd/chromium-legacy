// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.accounts.Account;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.support.annotation.MainThread;
import android.support.annotation.Nullable;

import org.chromium.base.ActivityState;
import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.Promise;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JCaller;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.task.PostTask;
import org.chromium.components.signin.AccountIdProvider;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountTrackerService;
import org.chromium.components.signin.ChromeSigninController;
import org.chromium.content_public.browser.UiThreadTaskTraits;

import java.util.ArrayList;
import java.util.List;

/**
 * Android wrapper of the SigninManager which provides access from the Java layer.
 * <p/>
 * This class handles common paths during the sign-in and sign-out flows.
 * <p/>
 * Only usable from the UI thread as the native SigninManager requires its access to be in the
 * UI thread.
 * <p/>
 * See chrome/browser/signin/signin_manager_android.h for more details.
 */
public class SigninManager implements AccountTrackerService.OnSystemAccountsSeededListener {
    private static final String TAG = "SigninManager";

    /**
     * A SignInStateObserver is notified when the user signs in to or out of Chrome.
     */
    public interface SignInStateObserver {
        /**
         * Invoked when the user has signed in to Chrome.
         */
        void onSignedIn();

        /**
         * Invoked when the user has signed out of Chrome.
         */
        void onSignedOut();
    }

    /**
     * SignInAllowedObservers will be notified once signing-in becomes allowed or disallowed.
     */
    public interface SignInAllowedObserver {
        /**
         * Invoked once all startup checks are done and signing-in becomes allowed, or disallowed.
         */
        void onSignInAllowedChanged();
    }

    /**
     * Callbacks for the sign-in flow.
     */
    public interface SignInCallback {
        /**
         * Invoked after sign-in is completed successfully.
         */
        void onSignInComplete();

        /**
         * Invoked if the sign-in processes does not complete for any reason.
         */
        void onSignInAborted();
    }

    /**
     * Hooks for wiping data during sign out.
     */
    public interface WipeDataHooks {
        /**
         * Called before data is wiped.
         */
        void preWipeData();

        /**
         * Called after data is wiped.
         */
        void postWipeData();
    }

    /**
     * Contains all the state needed for signin. This forces signin flow state to be
     * cleared atomically, and all final fields to be set upon initialization.
     */
    private static class SignInState {
        final Account mAccount;
        final Activity mActivity;
        final SignInCallback mCallback;

        /**
         * If the system accounts need to be seeded, the sign in flow will block for that to occur.
         * This boolean should be set to true during that time and then reset back to false
         * afterwards. This allows the manager to know if it should progress the flow when the
         * account tracker broadcasts updates.
         */
        boolean mBlockedOnAccountSeeding;

        /**
         * @param account The account to sign in to.
         * @param activity Reference to the UI to use for dialogs. Null means forced signin.
         * @param callback Called when the sign-in process finishes or is cancelled. Can be null.
         */
        SignInState(
                Account account, @Nullable Activity activity, @Nullable SignInCallback callback) {
            this.mAccount = account;
            this.mActivity = activity;
            this.mCallback = callback;
        }

        /**
         * Returns whether this is an interactive sign-in flow.
         */
        boolean isInteractive() {
            return mActivity != null;
        }

        /**
         * Returns whether the sign-in flow activity was set but is no longer visible to the user.
         */
        boolean isActivityInvisible() {
            return mActivity != null
                    && (ApplicationStatus.getStateForActivity(mActivity) == ActivityState.STOPPED
                            || ApplicationStatus.getStateForActivity(mActivity)
                                    == ActivityState.DESTROYED);
        }
    }

    /**
     * Contains all the state needed for sign out. Like SignInState, this forces flow state to be
     * cleared atomically, and all final fields to be set upon initialization.
     */
    private static class SignOutState {
        final Runnable mCallback;
        final WipeDataHooks mWipeDataHooks;
        final String mManagementDomain;
        final boolean mForceWipeUserData;

        /**
         * @param callback Called after sign-out finishes and all data has been cleared.
         * @param wipeDataHooks Hooks to call before/after data wiping phase of sign-out.
         * @param managementDomain Domain when account is managed.
         * @param forceWipeUserData Flag to wipe user data when account is not managed.
         */
        SignOutState(@Nullable Runnable callback, @Nullable WipeDataHooks wipeDataHooks,
                @Nullable String managementDomain, boolean forceWipeUserData) {
            this.mCallback = callback;
            this.mWipeDataHooks = wipeDataHooks;
            this.mManagementDomain = managementDomain;
            this.mForceWipeUserData = forceWipeUserData;
        }
    }

    @SuppressLint("StaticFieldLeak")
    private static SigninManager sTestingSigninManager;
    private static int sSignInAccessPoint = SigninAccessPoint.UNKNOWN;

    /**
     * Address of the native Signin Manager android.
     * This is not final, as destroy() updates this.
     */
    private long mNativeSigninManagerAndroid;
    private final Context mContext;
    private final SigninManagerDelegate mDelegate;
    private final AccountTrackerService mAccountTrackerService;
    private final ObserverList<SignInStateObserver> mSignInStateObservers = new ObserverList<>();
    private final ObserverList<SignInAllowedObserver> mSignInAllowedObservers =
            new ObserverList<>();
    private List<Runnable> mCallbacksWaitingForPendingOperation = new ArrayList<>();
    private boolean mSigninAllowedByPolicy;

    /**
     * Tracks whether the First Run check has been completed.
     *
     * A new sign-in can not be started while this is pending, to prevent the
     * pending check from eventually starting a 2nd sign-in.
     */
    private boolean mFirstRunCheckIsPending = true;

    /**
     * Will be set during the sign in process, and nulled out when there is not a pending sign in.
     * Needs to be null checked after ever async entry point because it can be nulled out at any
     * time by system accounts changing.
     */
    private @Nullable SignInState mSignInState;

    /**
     * Set during sign-out process and nulled out once complete. Helps to atomically gather/clear
     * various sign-out state.
     */
    private @Nullable SignOutState mSignOutState;

    /**
     * Called by native to create an instance of SigninManager.
     * @param nativeSigninManagerAndroid A pointer to native's SigninManagerAndroid.
     * @return
     */
    @CalledByNative
    private static SigninManager create(long nativeSigninManagerAndroid,
            SigninManagerDelegate delegate, AccountTrackerService accountTrackerService) {
        assert nativeSigninManagerAndroid != 0;
        assert delegate != null;
        assert accountTrackerService != null;
        return new SigninManager(ContextUtils.getApplicationContext(), nativeSigninManagerAndroid,
                delegate, accountTrackerService);
    }

    @VisibleForTesting
    SigninManager(Context context, long nativeSigninManagerAndroid, SigninManagerDelegate delegate,
            AccountTrackerService accountTrackerService) {
        ThreadUtils.assertOnUiThread();
        assert context != null;
        mContext = context;
        mNativeSigninManagerAndroid = nativeSigninManagerAndroid;
        mDelegate = delegate;
        mAccountTrackerService = accountTrackerService;

        mSigninAllowedByPolicy =
                SigninManagerJni.get().isSigninAllowedByPolicy(this, mNativeSigninManagerAndroid);

        mAccountTrackerService.addSystemAccountsSeededListener(this);
    }

    /**
     * Triggered during SigninManagerAndroidWrapper's KeyedService::Shutdown.
     * Drop references with external services and native.
     */
    @CalledByNative
    public void destroy() {
        mAccountTrackerService.removeSystemAccountsSeededListener(this);
        mNativeSigninManagerAndroid = 0;
    }

    /**
    * Log the access point when the user see the view of choosing account to sign in.
    * @param accessPoint the enum value of AccessPoint defined in signin_metrics.h.
    */
    public static void logSigninStartAccessPoint(int accessPoint) {
        RecordHistogram.recordEnumeratedHistogram(
                "Signin.SigninStartedAccessPoint", accessPoint, SigninAccessPoint.MAX);
        sSignInAccessPoint = accessPoint;
    }

    private void logSigninCompleteAccessPoint() {
        RecordHistogram.recordEnumeratedHistogram(
                "Signin.SigninCompletedAccessPoint", sSignInAccessPoint, SigninAccessPoint.MAX);
        sSignInAccessPoint = SigninAccessPoint.UNKNOWN;
    }

    /**
     * Notifies the SigninManager that the First Run check has completed.
     *
     * The user will be allowed to sign-in once this is signaled.
     */
    public void onFirstRunCheckDone() {
        mFirstRunCheckIsPending = false;

        if (isSignInAllowed()) {
            notifySignInAllowedChanged();
        }
    }

    /**
     * Returns true if signin can be started now.
     */
    public boolean isSignInAllowed() {
        return !mFirstRunCheckIsPending && mSignInState == null && mSigninAllowedByPolicy
                && ChromeSigninController.get().getSignedInUser() == null && isSigninSupported();
    }

    /**
     * Returns true if signin is disabled by policy.
     */
    public boolean isSigninDisabledByPolicy() {
        return !mSigninAllowedByPolicy;
    }

    /**
     * @return Whether true if the current user is not demo user and the user has a reasonable
     *         Google Play Services installed.
     */
    public boolean isSigninSupported() {
        return !ApiCompatibilityUtils.isDemoUser(mContext)
                && mDelegate.isGooglePlayServicesPresent(mContext)
                && !SigninManagerJni.get().isMobileIdentityConsistencyEnabled();
    }

    /**
     * @return Whether force sign-in is enabled by policy.
     */
    public boolean isForceSigninEnabled() {
        return SigninManagerJni.get().isForceSigninEnabled(this, mNativeSigninManagerAndroid);
    }

    /**
     * Registers a SignInStateObserver to be notified when the user signs in or out of Chrome.
     */
    public void addSignInStateObserver(SignInStateObserver observer) {
        mSignInStateObservers.addObserver(observer);
    }

    /**
     * Unregisters a SignInStateObserver to be notified when the user signs in or out of Chrome.
     */
    public void removeSignInStateObserver(SignInStateObserver observer) {
        mSignInStateObservers.removeObserver(observer);
    }

    public void addSignInAllowedObserver(SignInAllowedObserver observer) {
        mSignInAllowedObservers.addObserver(observer);
    }

    public void removeSignInAllowedObserver(SignInAllowedObserver observer) {
        mSignInAllowedObservers.removeObserver(observer);
    }

    private void notifySignInAllowedChanged() {
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> {
            for (SignInAllowedObserver observer : mSignInAllowedObservers) {
                observer.onSignInAllowedChanged();
            }
        });
    }

    /**
    * Continue pending sign in after system accounts have been seeded into AccountTrackerService.
    */
    @Override
    public void onSystemAccountsSeedingComplete() {
        if (mSignInState != null && mSignInState.mBlockedOnAccountSeeding) {
            mSignInState.mBlockedOnAccountSeeding = false;
            progressSignInFlowCheckPolicy();
        }
    }

    /**
    * Clear pending sign in when system accounts in AccountTrackerService were refreshed.
    */
    @Override
    public void onSystemAccountsChanged() {
        if (mSignInState != null) {
            abortSignIn();
        }
    }

    /**
     * Starts the sign-in flow, and executes the callback when finished.
     *
     * If an activity is provided, it is considered an "interactive" sign-in and the user can be
     * prompted to confirm various aspects of sign-in using dialogs inside the activity.
     * The sign-in flow goes through the following steps:
     *
     *   - Wait for AccountTrackerService to be seeded.
     *   - If interactive, confirm the account change with the user.
     *   - Wait for policy to be checked for the account.
     *   - If interactive and the account is managed, warn the user.
     *   - If managed, wait for the policy to be fetched.
     *   - Complete sign-in with the native SigninManager and kick off token requests.
     *   - Call the callback if provided.
     *
     * @param account The account to sign in to.
     * @param activity The activity used to launch UI prompts, or null for a forced signin.
     * @param callback Optional callback for when the sign-in process is finished.
     */
    public void signIn(
            Account account, @Nullable Activity activity, @Nullable SignInCallback callback) {
        if (account == null) {
            Log.w(TAG, "Ignoring sign-in request due to null account.");
            if (callback != null) callback.onSignInAborted();
            return;
        }

        if (mSignInState != null) {
            Log.w(TAG, "Ignoring sign-in request as another sign-in request is pending.");
            if (callback != null) callback.onSignInAborted();
            return;
        }

        if (mFirstRunCheckIsPending) {
            Log.w(TAG, "Ignoring sign-in request until the First Run check completes.");
            if (callback != null) callback.onSignInAborted();
            return;
        }

        mSignInState = new SignInState(account, activity, callback);
        notifySignInAllowedChanged();

        progressSignInFlowSeedSystemAccounts();
    }

    /**
     * Same as above but retrieves the Account object for the given accountName.
     */
    public void signIn(String accountName, @Nullable final Activity activity,
            @Nullable final SignInCallback callback) {
        AccountManagerFacade.get().getAccountFromName(
                accountName, account -> signIn(account, activity, callback));
    }

    private void progressSignInFlowSeedSystemAccounts() {
        if (mAccountTrackerService.checkAndSeedSystemAccounts()) {
            progressSignInFlowCheckPolicy();
        } else if (AccountIdProvider.getInstance().canBeUsed()) {
            mSignInState.mBlockedOnAccountSeeding = true;
        } else {
            Activity activity = mSignInState.mActivity;
            mDelegate.handleGooglePlayServicesUnavailability(activity, !isForceSigninEnabled());
            Log.w(TAG, "Cancelling the sign-in process as Google Play services is unavailable");
            abortSignIn();
        }
    }

    /**
     * Continues the signin flow by checking if there is a policy that the account is subject to.
     */
    private void progressSignInFlowCheckPolicy() {
        if (mSignInState == null) {
            Log.w(TAG, "Ignoring sign in progress request as no pending sign in.");
            return;
        }

        if (mSignInState.isActivityInvisible()) {
            abortSignIn();
            return;
        }

        Log.d(TAG, "Checking if account has policy management enabled");
        mDelegate.fetchAndApplyCloudPolicy(
                mSignInState.mAccount.name, this::onPolicyFetchedBeforeSignIn);
    }

    @VisibleForTesting
    /**
     * If the user is managed, its policy has been fetched and is being enforced; features like sync
     * may now be disabled by policy, and the rest of the sign-in flow can be resumed.
     */
    void onPolicyFetchedBeforeSignIn() {
        finishSignIn();
    }

    private void finishSignIn() {
        // This method should be called at most once per sign-in flow.
        assert mSignInState != null;

        SigninManagerJni.get().onSignInCompleted(
                this, mNativeSigninManagerAndroid, mSignInState.mAccount.name);

        // Cache the signed-in account name. This must be done after the native call, otherwise
        // sync tries to start without being signed in natively and crashes.
        ChromeSigninController.get().setSignedInAccountName(mSignInState.mAccount.name);
        mDelegate.enableSync(mSignInState.mAccount);

        if (mSignInState.mCallback != null) {
            mSignInState.mCallback.onSignInComplete();
        }

        // Trigger token requests via native.
        logInSignedInUser();

        if (mSignInState.isInteractive()) {
            // If signin was a user action, record that it succeeded.
            RecordUserAction.record("Signin_Signin_Succeed");
            logSigninCompleteAccessPoint();
            // Log signin in reason as defined in signin_metrics.h. Right now only
            // SIGNIN_PRIMARY_ACCOUNT available on Android.
            RecordHistogram.recordEnumeratedHistogram("Signin.SigninReason",
                    SigninReason.SIGNIN_PRIMARY_ACCOUNT, SigninReason.MAX);
        }

        Log.d(TAG, "Signin completed.");
        mSignInState = null;
        notifyCallbacksWaitingForOperation();
        notifySignInAllowedChanged();

        for (SignInStateObserver observer : mSignInStateObservers) {
            observer.onSignedIn();
        }
    }

    /**
     * Returns true if a sign-in or sign-out operation is in progress. See also
     * {@link #runAfterOperationInProgress}.
     */
    @MainThread
    public boolean isOperationInProgress() {
        ThreadUtils.assertOnUiThread();
        return mSignInState != null || mSignOutState != null;
    }

    /**
     * Schedules the runnable to be invoked after currently ongoing a sign-in or sign-out operation
     * is finished. If there's no operation is progress, posts the callback to the UI thread right
     * away. See also {@link #isOperationInProgress}.
     */
    @MainThread
    public void runAfterOperationInProgress(Runnable runnable) {
        ThreadUtils.assertOnUiThread();
        if (isOperationInProgress()) {
            mCallbacksWaitingForPendingOperation.add(runnable);
            return;
        }
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, runnable);
    }

    private void notifyCallbacksWaitingForOperation() {
        ThreadUtils.assertOnUiThread();
        for (Runnable callback : mCallbacksWaitingForPendingOperation) {
            PostTask.postTask(UiThreadTaskTraits.DEFAULT, callback);
        }
        mCallbacksWaitingForPendingOperation.clear();
    }

    /**
     * Invokes signOut and returns a {@link Promise} that will be fulfilled on completion.
     * This is equivalent to calling {@link #signOut(@SignoutReason int signoutSource, Runnable
     * callback)} with a callback that fulfills the returned {@link Promise}.
     */
    public Promise<Void> signOutPromise(@SignoutReason int signoutSource) {
        final Promise<Void> promise = new Promise<>();
        signOut(signoutSource, () -> promise.fulfill(null));

        return promise;
    }

    /**
     * Invokes signOut with no callback or wipeDataHooks.
     */
    public void signOut(@SignoutReason int signoutSource) {
        signOut(signoutSource, null, null, false);
    }

    /**
     * Invokes signOut() with no wipeDataHooks.
     */
    public void signOut(@SignoutReason int signoutSource, Runnable callback) {
        signOut(signoutSource, callback, null, false);
    }

    /**
     * Signs out of Chrome.
     * <p/>
     * This method clears the signed-in username, stops sync and sends out a
     * sign-out notification on the native side.
     *
     * @param signoutSource describes the event driving the signout (e.g.
     *         {@link SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS}).
     * @param callback Will be invoked after sign-out completes, if not null.
     * @param wipeDataHooks Hooks to call before/after data wiping phase of sign-out.
     * @param forceWipeUserData Whether user selected to wipe all device data.
     */
    public void signOut(@SignoutReason int signoutSource, Runnable callback,
            WipeDataHooks wipeDataHooks, boolean forceWipeUserData) {
        // Only one signOut at a time!
        assert mSignOutState == null;

        // Grab the management domain before nativeSignOut() potentially clears it.
        mSignOutState =
                new SignOutState(callback, wipeDataHooks, getManagementDomain(), forceWipeUserData);

        Log.d(TAG, "Signing out, management domain: " + mSignOutState.mManagementDomain);

        // User data will be wiped in mDelegate.disableSyncAndWipeData(), called from
        // onNativeSignOut().
        SigninManagerJni.get().signOut(this, mNativeSigninManagerAndroid, signoutSource);
    }

    /**
     * Returns the management domain if the signed in account is managed, otherwise returns null.
     */
    public String getManagementDomain() {
        return mDelegate.getManagementDomain();
    }

    @VisibleForTesting
    void logInSignedInUser() {
        SigninManagerJni.get().logInSignedInUser(this, mNativeSigninManagerAndroid);
    }

    public void clearLastSignedInUser() {
        SigninManagerJni.get().clearLastSignedInUser(this, mNativeSigninManagerAndroid);
    }

    /**
     * Aborts the current sign in.
     *
     * Package protected to allow dialog fragments to abort the signin flow.
     */
    void abortSignIn() {
        // Ensure this function can only run once per signin flow.
        SignInState signInState = mSignInState;
        assert signInState != null;
        mSignInState = null;
        notifyCallbacksWaitingForOperation();

        if (signInState.mCallback != null) {
            signInState.mCallback.onSignInAborted();
        }

        mDelegate.stopApplyingCloudPolicy();

        Log.d(TAG, "Signin flow aborted.");
        notifySignInAllowedChanged();
    }

    @VisibleForTesting
    @CalledByNative
    void onNativeSignOut() {
        if (mSignOutState == null) {
            // TODO(https://crbug.com/873671): Management domain is not captured in signOut() for
            // sign-outs that are initiated from the native side. But grabbing it here may be too
            // late! The management domain may be already cleared due to race condition with
            // sign-out observers on the native side.
            mSignOutState = new SignOutState(null, null, getManagementDomain(), false);
        }

        Log.d(TAG, "Native signed out, management domain: " + mSignOutState.mManagementDomain);

        // Native sign-out must happen before resetting the account so data is deleted correctly.
        // http://crbug.com/589028
        ChromeSigninController.get().setSignedInAccountName(null);
        if (mSignOutState.mWipeDataHooks != null) mSignOutState.mWipeDataHooks.preWipeData();
        mDelegate.disableSyncAndWipeData(
                mSignOutState.mManagementDomain != null || mSignOutState.mForceWipeUserData,
                this::onProfileDataWiped);
        mAccountTrackerService.invalidateAccountSeedStatus(true);
    }

    @VisibleForTesting
    protected void onProfileDataWiped() {
        // Should be set at start of sign-out flow.
        assert mSignOutState != null;

        if (mSignOutState.mWipeDataHooks != null) mSignOutState.mWipeDataHooks.postWipeData();
        finishSignOut();
    }

    private void finishSignOut() {
        // Should be set at start of sign-out flow.
        assert mSignOutState != null;

        if (mSignOutState.mCallback != null) {
            PostTask.postTask(UiThreadTaskTraits.DEFAULT, mSignOutState.mCallback);
        }
        mSignOutState = null;
        notifyCallbacksWaitingForOperation();

        for (SignInStateObserver observer : mSignInStateObservers) {
            observer.onSignedOut();
        }
    }

    /**
     * @return Whether there is a signed in account on the native side.
     */
    public boolean isSignedInOnNative() {
        return SigninManagerJni.get().isSignedInOnNative(this, mNativeSigninManagerAndroid);
    }

    @CalledByNative
    private void onSigninAllowedByPolicyChanged(boolean newSigninAllowedByPolicy) {
        mSigninAllowedByPolicy = newSigninAllowedByPolicy;
        notifySignInAllowedChanged();
    }

    /**
     * Verifies if the account is managed. Callback may be called either
     * synchronously or asynchronously depending on the availability of the
     * result.
     * @param email An email of the account.
     * @param callback The callback that will receive true if the account is managed, false
     *                 otherwise.
     */
    public void isAccountManaged(String email, final Callback<Boolean> callback) {
        mDelegate.isAccountManaged(email, callback);
    }

    public static String extractDomainName(String email) {
        return SigninManagerJni.get().extractDomainName(email);
    }

    // Native methods.
    @NativeMethods
    interface Natives {
        boolean isSigninAllowedByPolicy(
                @JCaller SigninManager self, long nativeSigninManagerAndroid);

        boolean isForceSigninEnabled(@JCaller SigninManager self, long nativeSigninManagerAndroid);

        void onSignInCompleted(
                @JCaller SigninManager self, long nativeSigninManagerAndroid, String username);

        void signOut(@JCaller SigninManager self, long nativeSigninManagerAndroid,
                @SignoutReason int reason);

        void clearLastSignedInUser(@JCaller SigninManager self, long nativeSigninManagerAndroid);

        void logInSignedInUser(@JCaller SigninManager self, long nativeSigninManagerAndroid);

        boolean isSignedInOnNative(@JCaller SigninManager self, long nativeSigninManagerAndroid);

        String extractDomainName(String email);

        boolean isMobileIdentityConsistencyEnabled();
    }
}
