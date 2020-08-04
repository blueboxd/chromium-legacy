// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_check;

import android.content.Intent;
import android.net.Uri;
import android.os.Handler;
import android.os.SystemClock;
import android.text.format.DateUtils;
import android.view.View;

import androidx.annotation.VisibleForTesting;
import androidx.preference.Preference;

import org.chromium.base.BuildConfig;
import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.password_check.BulkLeakCheckServiceState;
import org.chromium.chrome.browser.password_manager.ManagePasswordsReferrer;
import org.chromium.chrome.browser.password_manager.PasswordManagerHelper;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.safe_browsing.settings.SecuritySettingsFragment;
import org.chromium.chrome.browser.safety_check.SafetyCheckBridge.SafetyCheckCommonObserver;
import org.chromium.chrome.browser.safety_check.SafetyCheckProperties.PasswordsState;
import org.chromium.chrome.browser.safety_check.SafetyCheckProperties.SafeBrowsingState;
import org.chromium.chrome.browser.safety_check.SafetyCheckProperties.UpdatesState;
import org.chromium.chrome.browser.settings.SettingsLauncher;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.ref.WeakReference;

class SafetyCheckMediator implements SafetyCheckCommonObserver {
    /** The minimal amount of time to show the checking state. */
    private static final int CHECKING_MIN_DURATION_MS = 1000;
    /** Time after which the null-states will be shown: 10 minutes. */
    private static final long RESET_TO_NULL_AFTER_MS = 10 * DateUtils.MINUTE_IN_MILLIS;

    /** Bridge to the C++ side for the Safe Browsing and passwords checks. */
    private SafetyCheckBridge mSafetyCheckBridge;
    /** Model representing the current state of the checks. */
    private PropertyModel mModel;
    /** Client to interact with Omaha for the updates check. */
    private SafetyCheckUpdatesDelegate mUpdatesClient;
    /** Callbacks and related objects to show the checking state for at least 1 second. */
    private Handler mHandler;
    private Runnable mRunnablePasswords;
    private Runnable mRunnableSafeBrowsing;
    private Runnable mRunnableUpdates;
    private long mCheckStartTime = -1;

    private final SharedPreferencesManager mPreferenceManager;

    /**
     * Callback that gets invoked once the result of the updates check is available. Not inlined
     * because a {@link WeakReference} needs to be passed (the check is asynchronous).
     */
    private final Callback<Integer> mUpdatesCheckCallback = (status) -> {
        mRunnableUpdates = () -> {
            if (mModel != null) {
                mModel.set(SafetyCheckProperties.UPDATES_STATE, status);
            }
        };
        if (mHandler != null) {
            // Show the checking state for at least 1 second for a smoother UX.
            mHandler.postDelayed(mRunnableUpdates, getModelUpdateDelay());
        }
    };

    /**
     * Creates a new instance given a model, an updates client, and a settings launcher.
     *
     * @param model A model instance.
     * @param client An updates client.
     * @param settingsLauncher An instance of the {@link SettingsLauncher} implementation.
     */
    public SafetyCheckMediator(PropertyModel model, SafetyCheckUpdatesDelegate client,
            SettingsLauncher settingsLauncher) {
        this(model, client, settingsLauncher, null, new Handler());
        // Have to initialize this after the constructor call, since a "this" instance is needed.
        mSafetyCheckBridge = new SafetyCheckBridge(SafetyCheckMediator.this);
        // Determine and set the initial state.
        setInitialState();
    }

    @VisibleForTesting
    SafetyCheckMediator(PropertyModel model, SafetyCheckUpdatesDelegate client,
            SettingsLauncher settingsLauncher, SafetyCheckBridge bridge, Handler handler) {
        mModel = model;
        mUpdatesClient = client;
        mSafetyCheckBridge = bridge;
        mHandler = handler;
        mPreferenceManager = SharedPreferencesManager.getInstance();
        // Set the listener for clicking the updates element.
        mModel.set(SafetyCheckProperties.UPDATES_CLICK_LISTENER,
                (Preference.OnPreferenceClickListener) (p) -> {
                    if (!BuildConfig.IS_CHROME_BRANDED) {
                        return true;
                    }
                    String chromeAppId = ContextUtils.getApplicationContext().getPackageName();
                    // Open the Play Store page for the installed Chrome channel.
                    p.getContext().startActivity(new Intent(Intent.ACTION_VIEW,
                            Uri.parse(ContentUrlConstants.PLAY_STORE_URL_PREFIX + chromeAppId)));
                    return true;
                });
        // Set the listener for clicking the Safe Browsing element.
        mModel.set(SafetyCheckProperties.SAFE_BROWSING_CLICK_LISTENER,
                (Preference.OnPreferenceClickListener) (p) -> {
                    String safeBrowsingSettingsClassName;
                    if (ChromeFeatureList.isEnabled(
                                ChromeFeatureList.SAFE_BROWSING_SECURITY_SECTION_UI)) {
                        // Open the Security settings since the flag for them is enabled.
                        safeBrowsingSettingsClassName = SecuritySettingsFragment.class.getName();
                    } else {
                        // Open the Sync and Services settings.
                        // TODO(crbug.com/1070620): replace the hardcoded class name with an import
                        // and ".class.getName()" once SyncAndServicesSettings is moved out of
                        // //chrome/android.
                        safeBrowsingSettingsClassName =
                                "org.chromium.chrome.browser.sync.settings.SyncAndServicesSettings";
                    }
                    p.getContext().startActivity(settingsLauncher.createSettingsActivityIntent(
                            p.getContext(), safeBrowsingSettingsClassName));
                    return true;
                });
        // Set the listener for clicking the passwords element.
        mModel.set(SafetyCheckProperties.PASSWORDS_CLICK_LISTENER,
                (Preference.OnPreferenceClickListener) (p) -> {
                    // Open the Passwords settings.
                    PasswordManagerHelper.showPasswordSettings(p.getContext(),
                            ManagePasswordsReferrer.CHROME_SETTINGS, settingsLauncher);
                    return true;
                });
        // Set the listener for clicking the Check button.
        mModel.set(SafetyCheckProperties.SAFETY_CHECK_BUTTON_CLICK_LISTENER,
                (View.OnClickListener) (v) -> performSafetyCheck());
        // Get the timestamp of the last run.
        mModel.set(SafetyCheckProperties.LAST_RUN_TIMESTAMP,
                mPreferenceManager.readLong(
                        ChromePreferenceKeys.SETTINGS_SAFETY_CHECK_LAST_RUN_TIMESTAMP, 0));
        if (mSafetyCheckBridge != null) {
            // Determine and set the initial state.
            setInitialState();
        }
    }

    /**
     * Determines the initial state to show, triggering any fast checks if necessary based on the
     * last run timestamp.
     */
    public void setInitialState() {
        long currentTime = System.currentTimeMillis();
        long lastRun = mPreferenceManager.readLong(
                ChromePreferenceKeys.SETTINGS_SAFETY_CHECK_LAST_RUN_TIMESTAMP, 0);
        // Always show the passwords unsafe state.
        if (mSafetyCheckBridge.getNumberOfPasswordLeaksFromLastCheck() != 0) {
            mModel.set(SafetyCheckProperties.PASSWORDS_STATE, PasswordsState.COMPROMISED_EXIST);
        }
        if (currentTime - lastRun < RESET_TO_NULL_AFTER_MS) {
            // Show the passwords safe state
            if (!mSafetyCheckBridge.savedPasswordsExist()) {
                mModel.set(SafetyCheckProperties.PASSWORDS_STATE, PasswordsState.NO_PASSWORDS);
            } else if (mSafetyCheckBridge.getNumberOfPasswordLeaksFromLastCheck() == 0) {
                mModel.set(SafetyCheckProperties.PASSWORDS_STATE, PasswordsState.SAFE);
            }
            // Rerun the updates and Safe Browsing checks.
            mModel.set(SafetyCheckProperties.SAFE_BROWSING_STATE, SafeBrowsingState.CHECKING);
            mModel.set(SafetyCheckProperties.UPDATES_STATE, UpdatesState.CHECKING);
            mSafetyCheckBridge.checkSafeBrowsing();
            mUpdatesClient.checkForUpdates(new WeakReference(mUpdatesCheckCallback));
        } else {
            // The unsafe state was already set above, so only set to unchecked if it is safe.
            if (mSafetyCheckBridge.getNumberOfPasswordLeaksFromLastCheck() == 0) {
                mModel.set(SafetyCheckProperties.PASSWORDS_STATE, PasswordsState.UNCHECKED);
            }
            mModel.set(SafetyCheckProperties.SAFE_BROWSING_STATE, SafeBrowsingState.UNCHECKED);
            mModel.set(SafetyCheckProperties.UPDATES_STATE, UpdatesState.UNCHECKED);
        }
    }

    /** Triggers all safety check child checks. */
    public void performSafetyCheck() {
        // Cancel pending delayed show callbacks if a new check is starting while any existing
        // elements are pending.
        cancelCallbacks();
        // Record the start time for tracking 1 second checking delay in the UI.
        mCheckStartTime = SystemClock.elapsedRealtime();
        // Record the absolute start time for showing when the last Safety check was performed.
        long currentTime = System.currentTimeMillis();
        mModel.set(SafetyCheckProperties.LAST_RUN_TIMESTAMP, currentTime);
        mPreferenceManager.writeLong(
                ChromePreferenceKeys.SETTINGS_SAFETY_CHECK_LAST_RUN_TIMESTAMP, currentTime);
        // Increment the stored number of Safety check starts.
        mPreferenceManager.incrementInt(ChromePreferenceKeys.SETTINGS_SAFETY_CHECK_RUN_COUNTER);
        // Set the checking state for all elements.
        mModel.set(SafetyCheckProperties.PASSWORDS_STATE, PasswordsState.CHECKING);
        mModel.set(SafetyCheckProperties.SAFE_BROWSING_STATE, SafeBrowsingState.CHECKING);
        mModel.set(SafetyCheckProperties.UPDATES_STATE, UpdatesState.CHECKING);
        // Start all the checks.
        mSafetyCheckBridge.checkSafeBrowsing();
        mSafetyCheckBridge.checkPasswords();
        mUpdatesClient.checkForUpdates(new WeakReference(mUpdatesCheckCallback));
    }

    /**
     * Gets invoked once the Safe Browsing check is completed.
     *
     * @param status SafetyCheck::SafeBrowsingStatus enum value representing the Safe Browsing state
     *     (see //components/safety_check/safety_check.h).
     */
    @Override
    public void onSafeBrowsingCheckResult(@SafeBrowsingStatus int status) {
        mRunnableSafeBrowsing = () -> {
            mModel.set(SafetyCheckProperties.SAFE_BROWSING_STATE,
                    SafetyCheckProperties.safeBrowsingStateFromNative(status));
        };
        // Show the checking state for at least 1 second for a smoother UX.
        mHandler.postDelayed(mRunnableSafeBrowsing, getModelUpdateDelay());
    }

    /**
     * Gets invoked by the C++ code every time another credential is checked.
     *
     * @param checked Number of passwords already checked.
     * @param total Total number of passwords to check.
     */
    @Override
    public void onPasswordCheckCredentialDone(int checked, int total) {}

    /**
     * Gets invoked by the C++ code when the status of the password check changes.
     *
     * @param state BulkLeakCheckService::State enum value representing the state (see
     *     //components/password_manager/core/browser/bulk_leak_check_service_interface.h).
     */
    @Override
    public void onPasswordCheckStateChange(@BulkLeakCheckServiceState int state) {
        if (state == BulkLeakCheckServiceState.RUNNING) {
            return;
        }
        mSafetyCheckBridge.stopObservingPasswordsCheck();
        mRunnablePasswords = () -> {
            // Handle the error states.
            if (state != BulkLeakCheckServiceState.IDLE) {
                mModel.set(SafetyCheckProperties.PASSWORDS_STATE,
                        SafetyCheckProperties.passwordsStatefromErrorState(state));
                return;
            }
            // Non-error state depends on whether there are any passwords saved and/or leaked.
            if (!mSafetyCheckBridge.savedPasswordsExist()) {
                mModel.set(SafetyCheckProperties.PASSWORDS_STATE, PasswordsState.NO_PASSWORDS);
            } else if (mSafetyCheckBridge.getNumberOfPasswordLeaksFromLastCheck() == 0) {
                mModel.set(SafetyCheckProperties.PASSWORDS_STATE, PasswordsState.SAFE);
            } else {
                mModel.set(SafetyCheckProperties.PASSWORDS_STATE, PasswordsState.COMPROMISED_EXIST);
            }
        };
        // Show the checking state for at least 1 second for a smoother UX.
        mHandler.postDelayed(mRunnablePasswords, getModelUpdateDelay());
    }

    /** Cancels any pending callbacks and registered observers.  */
    public void destroy() {
        cancelCallbacks();
        mSafetyCheckBridge.destroy();
        mSafetyCheckBridge = null;
        mUpdatesClient = null;
        mModel = null;
        mHandler = null;
    }

    /** Cancels any delayed show callbacks. */
    private void cancelCallbacks() {
        if (mRunnablePasswords != null) {
            mHandler.removeCallbacks(mRunnablePasswords);
            mRunnablePasswords = null;
        }
        if (mRunnableSafeBrowsing != null) {
            mHandler.removeCallbacks(mRunnableSafeBrowsing);
            mRunnableSafeBrowsing = null;
        }
        if (mRunnableUpdates != null) {
            mHandler.removeCallbacks(mRunnableUpdates);
            mRunnableUpdates = null;
        }
    }

    /**
     * @return The delay in ms for updating the model in the running state.
     */
    private long getModelUpdateDelay() {
        return Math.max(
                0, mCheckStartTime + CHECKING_MIN_DURATION_MS - SystemClock.elapsedRealtime());
    }
}
