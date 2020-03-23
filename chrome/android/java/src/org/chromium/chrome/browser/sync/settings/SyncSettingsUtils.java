// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.sync.settings;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.IntentSender;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.provider.Browser;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.fragment.app.Fragment;
import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;

import org.chromium.base.BuildInfo;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.LaunchIntentDispatcher;
import org.chromium.chrome.browser.browserservices.BrowserServicesIntentDataProvider.CustomTabsUiType;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.signin.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.GoogleServiceAuthError;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.chrome.browser.sync.TrustedVaultClient;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.sync.AndroidSyncSettings;
import org.chromium.components.sync.StopSource;
import org.chromium.ui.UiUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Helper methods for sync settings.
 */
public class SyncSettingsUtils {
    private static final String DASHBOARD_URL = "https://www.google.com/settings/chrome/sync";
    private static final String MY_ACCOUNT_URL = "https://myaccount.google.com/smartlink/home";
    private static final String TAG = "SyncSettingsUtils";

    @IntDef({SyncError.NO_ERROR, SyncError.ANDROID_SYNC_DISABLED, SyncError.AUTH_ERROR,
            SyncError.PASSPHRASE_REQUIRED, SyncError.CLIENT_OUT_OF_DATE,
            SyncError.SYNC_SETUP_INCOMPLETE, SyncError.OTHER_ERRORS})
    @Retention(RetentionPolicy.SOURCE)
    public @interface SyncError {
        int NO_ERROR = -1;
        int ANDROID_SYNC_DISABLED = 0;
        int AUTH_ERROR = 1;
        int PASSPHRASE_REQUIRED = 2;
        int TRUSTED_VAULT_KEY_REQUIRED_FOR_EVERYTHING = 3;
        int TRUSTED_VAULT_KEY_REQUIRED_FOR_PASSWORDS = 4;
        int CLIENT_OUT_OF_DATE = 5;
        int SYNC_SETUP_INCOMPLETE = 6;
        int OTHER_ERRORS = 128;
    }

    /**
     * Returns the type of the sync error.
     */
    @SyncError
    public static int getSyncError() {
        if (!AndroidSyncSettings.get().isMasterSyncEnabled()) {
            return SyncError.ANDROID_SYNC_DISABLED;
        }

        if (!AndroidSyncSettings.get().isChromeSyncEnabled()) {
            return SyncError.NO_ERROR;
        }

        ProfileSyncService profileSyncService = ProfileSyncService.get();
        if (profileSyncService == null) {
            return SyncError.NO_ERROR;
        }
        if (profileSyncService.getAuthError()
                == GoogleServiceAuthError.State.INVALID_GAIA_CREDENTIALS) {
            return SyncError.AUTH_ERROR;
        }

        if (profileSyncService.requiresClientUpgrade()) {
            return SyncError.CLIENT_OUT_OF_DATE;
        }

        if (profileSyncService.getAuthError() != GoogleServiceAuthError.State.NONE
                || profileSyncService.hasUnrecoverableError()) {
            return SyncError.OTHER_ERRORS;
        }

        if (profileSyncService.isEngineInitialized()
                && profileSyncService.isPassphraseRequiredForPreferredDataTypes()) {
            return SyncError.PASSPHRASE_REQUIRED;
        }

        if (profileSyncService.isEngineInitialized()
                && profileSyncService.isTrustedVaultKeyRequiredForPreferredDataTypes()) {
            return profileSyncService.isEncryptEverythingEnabled()
                    ? SyncError.TRUSTED_VAULT_KEY_REQUIRED_FOR_EVERYTHING
                    : SyncError.TRUSTED_VAULT_KEY_REQUIRED_FOR_PASSWORDS;
        }

        if (!profileSyncService.isFirstSetupComplete()) {
            return SyncError.SYNC_SETUP_INCOMPLETE;
        }

        return SyncError.NO_ERROR;
    }

    /**
     * Gets hint message to resolve sync error.
     * @param context The application context.
     * @param error The sync error.
     */
    public static String getSyncErrorHint(Context context, @SyncError int error) {
        switch (error) {
            case SyncError.ANDROID_SYNC_DISABLED:
                return context.getString(R.string.hint_android_sync_disabled);
            case SyncError.AUTH_ERROR:
                return context.getString(R.string.hint_sync_auth_error);
            case SyncError.CLIENT_OUT_OF_DATE:
                return context.getString(
                        R.string.hint_client_out_of_date, BuildInfo.getInstance().hostPackageLabel);
            case SyncError.OTHER_ERRORS:
                return context.getString(R.string.hint_other_sync_errors);
            case SyncError.PASSPHRASE_REQUIRED:
                return context.getString(R.string.hint_passphrase_required);
            case SyncError.TRUSTED_VAULT_KEY_REQUIRED_FOR_EVERYTHING:
            case SyncError.TRUSTED_VAULT_KEY_REQUIRED_FOR_PASSWORDS:
                return context.getString(R.string.hint_sync_retrieve_keys);
            case SyncError.SYNC_SETUP_INCOMPLETE:
                return context.getString(R.string.hint_sync_settings_not_confirmed_description);
            case SyncError.NO_ERROR:
            default:
                return null;
        }
    }

    /**
     * Return a short summary of the current sync status.
     */
    public static String getSyncStatusSummary(Context context) {
        if (!IdentityServicesProvider.get().getIdentityManager().hasPrimaryAccount()) return "";

        ProfileSyncService profileSyncService = ProfileSyncService.get();
        Resources res = context.getResources();

        if (!AndroidSyncSettings.get().isMasterSyncEnabled()) {
            return res.getString(R.string.sync_android_master_sync_disabled);
        }

        if (profileSyncService == null) {
            return res.getString(R.string.sync_is_disabled);
        }

        if (profileSyncService.isSyncDisabledByEnterprisePolicy()) {
            return res.getString(R.string.sync_is_disabled_by_administrator);
        }

        if (!profileSyncService.isFirstSetupComplete()) {
            return res.getString(R.string.sync_settings_not_confirmed);
        }

        if (profileSyncService.getAuthError() != GoogleServiceAuthError.State.NONE) {
            return res.getString(
                    GoogleServiceAuthError.getMessageID(profileSyncService.getAuthError()));
        }

        if (profileSyncService.requiresClientUpgrade()) {
            return res.getString(
                    R.string.sync_error_upgrade_client, BuildInfo.getInstance().hostPackageLabel);
        }

        if (profileSyncService.hasUnrecoverableError()) {
            return res.getString(R.string.sync_error_generic);
        }

        boolean syncEnabled = AndroidSyncSettings.get().isSyncEnabled();
        if (syncEnabled) {
            if (!profileSyncService.isSyncActive()) {
                return res.getString(R.string.sync_setup_progress);
            }

            if (profileSyncService.isPassphraseRequiredForPreferredDataTypes()) {
                return res.getString(R.string.sync_need_passphrase);
            }

            if (profileSyncService.isTrustedVaultKeyRequiredForPreferredDataTypes()) {
                return profileSyncService.isEncryptEverythingEnabled()
                        ? context.getString(R.string.sync_error_card_title)
                        : context.getString(R.string.sync_passwords_error_card_title);
            }

            return context.getString(R.string.sync_and_services_summary_sync_on);
        }
        return context.getString(R.string.sync_is_disabled);
    }

    /**
     * Returns an icon that represents the current sync state.
     */
    public static @Nullable Drawable getSyncStatusIcon(Context context) {
        if (!IdentityServicesProvider.get().getIdentityManager().hasPrimaryAccount()) return null;

        ProfileSyncService profileSyncService = ProfileSyncService.get();
        if (profileSyncService == null || !AndroidSyncSettings.get().isSyncEnabled()) {
            return UiUtils.getTintedDrawable(
                    context, R.drawable.ic_sync_green_40dp, R.color.default_icon_color);
        }

        if (profileSyncService.isSyncDisabledByEnterprisePolicy()) {
            return UiUtils.getTintedDrawable(
                    context, R.drawable.ic_sync_error_40dp, R.color.default_icon_color);
        }

        if (!profileSyncService.isFirstSetupComplete()) {
            return UiUtils.getTintedDrawable(
                    context, R.drawable.ic_sync_error_40dp, R.color.default_red);
        }

        if (profileSyncService.isEngineInitialized()
                && (profileSyncService.hasUnrecoverableError()
                        || profileSyncService.getAuthError() != GoogleServiceAuthError.State.NONE
                        || profileSyncService.isPassphraseRequiredForPreferredDataTypes()
                        || profileSyncService.isTrustedVaultKeyRequiredForPreferredDataTypes())) {
            return UiUtils.getTintedDrawable(
                    context, R.drawable.ic_sync_error_40dp, R.color.default_red);
        }

        return UiUtils.getTintedDrawable(
                context, R.drawable.ic_sync_green_40dp, R.color.default_green);
    }

    /**
     * Enables or disables {@link ProfileSyncService} and optionally records metrics that the sync
     * was disabled from settings. Requires that {@link ProfileSyncService#get()} returns non-null
     * reference.
     */
    public static void enableSync(boolean enable) {
        ProfileSyncService profileSyncService = ProfileSyncService.get();
        if (enable == profileSyncService.isSyncRequested()) return;

        if (enable) {
            profileSyncService.requestStart();
        } else {
            RecordHistogram.recordEnumeratedHistogram("Sync.StopSource",
                    StopSource.CHROME_SYNC_SETTINGS, StopSource.STOP_SOURCE_LIMIT);
            profileSyncService.requestStop();
        }
    }

    /**
     * Creates a wrapper around {@link Runnable} that calls the runnable only if
     * {@link PreferenceFragmentCompat} is still in resumed state. Click events that arrive after
     * the fragment has been paused will be ignored. See http://b/5983282.
     * @param fragment The fragment that hosts the preference.
     * @param runnable The runnable to call from {@link Preference.OnPreferenceClickListener}.
     */
    static Preference.OnPreferenceClickListener toOnClickListener(
            PreferenceFragmentCompat fragment, Runnable runnable) {
        return preference -> {
            if (!fragment.isResumed()) {
                // This event could come in after onPause if the user clicks back and the preference
                // at roughly the same time. See http://b/5983282.
                return false;
            }
            runnable.run();
            return false;
        };
    }

    /**
     * Opens web dashboard to specified url in a custom tab.
     * @param activity The activity to use for starting the intent.
     * @param url The url link to open in the custom tab.
     */
    private static void openCustomTabWithURL(Activity activity, String url) {
        CustomTabsIntent customTabIntent =
                new CustomTabsIntent.Builder().setShowTitle(false).build();
        customTabIntent.intent.setData(Uri.parse(url));

        Intent intent = LaunchIntentDispatcher.createCustomTabActivityIntent(
                activity, customTabIntent.intent);
        intent.setPackage(activity.getPackageName());
        intent.putExtra(CustomTabIntentDataProvider.EXTRA_UI_TYPE, CustomTabsUiType.DEFAULT);
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, activity.getPackageName());
        IntentHandler.addTrustedIntentExtras(intent);

        IntentUtils.safeStartActivity(activity, intent);
    }

    /**
     * Opens web dashboard to manage sync in a custom tab.
     * @param activity The activity to use for starting the intent.
     */
    public static void openSyncDashboard(Activity activity) {
        // TODO(https://crbug.com/948103): Create a builder for custom tab intents.
        openCustomTabWithURL(activity, DASHBOARD_URL);
    }

    /**
     * Opens web dashboard to manage google account in a custom tab.
     * @param activity The activity to use for starting the intent.
     */
    public static void openGoogleMyAccount(Activity activity) {
        assert IdentityServicesProvider.get().getIdentityManager().hasPrimaryAccount();
        RecordUserAction.record("SyncPreferences_ManageGoogleAccountClicked");
        openCustomTabWithURL(activity, MY_ACCOUNT_URL);
    }

    /**
     * Displays a UI that allows the user to reauthenticate and retrieve the sync encryption keys
     * from a trusted vault.
     *
     * @param fragment Fragment to use when starting the dialog.
     * @param accountInfo Account representing the user.
     * @param requestCode Arbitrary request code that upon completion will be passed back via
     *         Fragment.onActivityResult().
     */
    public static void openTrustedVaultKeyRetrievalDialog(
            Fragment fragment, CoreAccountInfo accountInfo, int requestCode) {
        TrustedVaultClient.get()
                .createKeyRetrievalIntent(accountInfo)
                .then(
                        (pendingIntent)
                                -> {
                            try {
                                fragment.startIntentSenderForResult(pendingIntent.getIntentSender(),
                                        requestCode,
                                        /* fillInIntent */ null, /* flagsMask */ 0,
                                        /* flagsValues */ 0, /* extraFlags */ 0,
                                        /* options */ null);
                            } catch (IntentSender.SendIntentException exception) {
                                Log.w(TAG, "Error sending key retrieval intent: ", exception);
                            }
                        },
                        (exception) -> {
                            Log.e(TAG, "Error opening key retrieval dialog: ", exception);
                        });
    }
}
