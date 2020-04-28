// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.util.Log;

import androidx.annotation.StringRes;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.notifications.NotificationBuilderFactory;
import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.chrome.browser.settings.SettingsLauncher;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.browser.signin.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.GoogleServiceAuthError.State;
import org.chromium.chrome.browser.sync.settings.SyncAndServicesSettings;
import org.chromium.chrome.browser.sync.ui.PassphraseActivity;
import org.chromium.components.browser_ui.notifications.ChromeNotification;
import org.chromium.components.browser_ui.notifications.ChromeNotificationBuilder;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxyImpl;
import org.chromium.components.browser_ui.notifications.NotificationMetadata;
import org.chromium.components.browser_ui.notifications.PendingIntentProvider;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.sync.AndroidSyncSettings;
import org.chromium.components.sync.PassphraseType;

/**
 * {@link SyncNotificationController} provides functionality for displaying Android notifications
 * regarding the user sync status.
 */
public class SyncNotificationController implements ProfileSyncService.SyncStateChangedListener {
    private static final String TAG = "SyncNotificationController";
    private final NotificationManagerProxy mNotificationManager;
    private final ProfileSyncService mProfileSyncService;

    public SyncNotificationController() {
        mNotificationManager =
                new NotificationManagerProxyImpl(ContextUtils.getApplicationContext());
        mProfileSyncService = ProfileSyncService.get();
        assert mProfileSyncService != null;
    }

    /**
     * Callback for {@link ProfileSyncService.SyncStateChangedListener}.
     */
    @Override
    public void syncStateChanged() {
        ThreadUtils.assertOnUiThread();

        // Auth errors take precedence over passphrase errors.
        if (!AndroidSyncSettings.get().isSyncEnabled()) {
            mNotificationManager.cancel(NotificationConstants.NOTIFICATION_ID_SYNC);
            return;
        }
        if (shouldSyncAuthErrorBeShown()) {
            showSyncNotification(
                    GoogleServiceAuthError.getMessageID(mProfileSyncService.getAuthError()),
                    createSettingsIntent());
        } else if (mProfileSyncService.isEngineInitialized()
                && mProfileSyncService.isPassphraseRequiredForPreferredDataTypes()) {
            assert (!mProfileSyncService.isTrustedVaultKeyRequiredForPreferredDataTypes());

            if (mProfileSyncService.isPassphrasePrompted()) {
                return;
            }
            switch (mProfileSyncService.getPassphraseType()) {
                case PassphraseType.IMPLICIT_PASSPHRASE: // Falling through intentionally.
                case PassphraseType.FROZEN_IMPLICIT_PASSPHRASE: // Falling through intentionally.
                case PassphraseType.CUSTOM_PASSPHRASE:
                    showSyncNotification(R.string.sync_need_passphrase, createPasswordIntent());
                    break;
                case PassphraseType.TRUSTED_VAULT_PASSPHRASE:
                    assert false : "Passphrase cannot be required with trusted vault passphrase";
                    return;
                case PassphraseType.KEYSTORE_PASSPHRASE: // Falling through intentionally.
                default:
                    mNotificationManager.cancel(NotificationConstants.NOTIFICATION_ID_SYNC);
                    return;
            }
        } else if (mProfileSyncService.isEngineInitialized()
                && mProfileSyncService.isTrustedVaultKeyRequiredForPreferredDataTypes()) {
            CoreAccountInfo primaryAccountInfo =
                    IdentityServicesProvider.get().getIdentityManager().getPrimaryAccountInfo(
                            ConsentLevel.SYNC);
            if (primaryAccountInfo != null) {
                int flags = Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TOP;
                // TODO(crbug.com/1012659): Upon intent completion, the new keys should be fetched.
                TrustedVaultClient.get()
                        .createKeyRetrievalIntent(primaryAccountInfo)
                        .then(
                                (pendingIntent)
                                        -> {
                                    showSyncNotificationForPendingIntent(
                                            mProfileSyncService.isEncryptEverythingEnabled()
                                                    ? R.string.sync_error_card_title
                                                    : R.string.sync_passwords_error_card_title,
                                            new PendingIntentProvider(pendingIntent, flags));
                                },
                                (exception) -> {
                                    Log.w(TAG, "Error creating key retrieval intent: ", exception);
                                });
            }
        } else {
            mNotificationManager.cancel(NotificationConstants.NOTIFICATION_ID_SYNC);
            return;
        }
    }

    /**
     * Builds and shows a notification for the |message|.
     *
     * @param message Resource id of the message to display in the notification.
     * @param contentIntent represents intent to send when the user activates the notification.
     */
    private void showSyncNotificationForPendingIntent(
            @StringRes int message, PendingIntentProvider contentIntent) {
        Context applicationContext = ContextUtils.getApplicationContext();
        String title = null;
        String text = null;
        // From Android N, notification by default has the app name and title should not be the same
        // as app name.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            title = applicationContext.getString(R.string.sign_in_sync);
            text = applicationContext.getString(message);
        } else {
            title = applicationContext.getString(R.string.app_name);
            text = applicationContext.getString(R.string.sign_in_sync) + ": "
                    + applicationContext.getString(message);
        }

        // There is no need to provide a group summary notification because the NOTIFICATION_ID_SYNC
        // notification id ensures there's only one sync notification at a time.
        ChromeNotificationBuilder builder =
                NotificationBuilderFactory
                        .createChromeNotificationBuilder(true /* preferCompat */,
                                ChromeChannelDefinitions.ChannelId.BROWSER,
                                null /*remoteAppPackageName*/,
                                new NotificationMetadata(
                                        NotificationUmaTracker.SystemNotificationType.SYNC, null,
                                        NotificationConstants.NOTIFICATION_ID_SYNC))
                        .setAutoCancel(true)
                        .setContentIntent(contentIntent)
                        .setContentTitle(title)
                        .setContentText(text)
                        .setSmallIcon(R.drawable.ic_chrome)
                        .setTicker(text)
                        .setLocalOnly(true)
                        .setGroup(NotificationConstants.GROUP_SYNC);

        ChromeNotification notification = builder.buildWithBigTextStyle(text);

        mNotificationManager.notify(notification);
        NotificationUmaTracker.getInstance().onNotificationShown(
                NotificationUmaTracker.SystemNotificationType.SYNC, notification.getNotification());
    }

    /**
     * Builds and shows a notification for the |message|.
     *
     * @param message Resource id of the message to display in the notification.
     * @param intent Intent to send when the user activates the notification.
     */
    private void showSyncNotification(@StringRes int message, Intent intent) {
        Context applicationContext = ContextUtils.getApplicationContext();
        PendingIntentProvider contentIntent =
                PendingIntentProvider.getActivity(applicationContext, 0, intent, 0);
        showSyncNotificationForPendingIntent(message, contentIntent);
    }

    private boolean shouldSyncAuthErrorBeShown() {
        switch (mProfileSyncService.getAuthError()) {
            case State.NONE:
            case State.CONNECTION_FAILED:
            case State.SERVICE_UNAVAILABLE:
            case State.REQUEST_CANCELED:
            case State.INVALID_GAIA_CREDENTIALS:
                return false;
            case State.USER_NOT_SIGNED_UP:
                return true;
            default:
                Log.w(TAG, "Not showing unknown Auth Error: " + mProfileSyncService.getAuthError());
                return false;
        }
    }

    /**
     * Creates an intent that launches the Chrome settings, and automatically opens the fragment
     * for signed in users.
     *
     * @return the intent for opening the settings
     */
    private Intent createSettingsIntent() {
        SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
        return settingsLauncher.createSettingsActivityIntent(ContextUtils.getApplicationContext(),
                SyncAndServicesSettings.class.getName(),
                SyncAndServicesSettings.createArguments(false));
    }

    /**
     * Creates an intent that launches an activity that requests the users password/passphrase.
     *
     * @return the intent for opening the password/passphrase activity
     */
    private Intent createPasswordIntent() {
        // Make sure we don't prompt too many times.
        mProfileSyncService.setPassphrasePrompted(true);

        Intent intent = new Intent(ContextUtils.getApplicationContext(), PassphraseActivity.class);
        // This activity will become the start of a new task on this history stack.
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        // Clears the task stack above this activity if it already exists.
        intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP);
        return intent;
    }
}
