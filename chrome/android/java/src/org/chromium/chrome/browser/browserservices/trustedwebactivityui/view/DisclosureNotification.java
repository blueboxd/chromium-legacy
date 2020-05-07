// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.trustedwebactivityui.view;

import static org.chromium.chrome.browser.browserservices.trustedwebactivityui.TrustedWebActivityModel.DISCLOSURE_EVENTS_CALLBACK;
import static org.chromium.chrome.browser.browserservices.trustedwebactivityui.TrustedWebActivityModel.DISCLOSURE_FIRST_TIME;
import static org.chromium.chrome.browser.browserservices.trustedwebactivityui.TrustedWebActivityModel.DISCLOSURE_SCOPE;
import static org.chromium.chrome.browser.browserservices.trustedwebactivityui.TrustedWebActivityModel.DISCLOSURE_STATE;
import static org.chromium.chrome.browser.browserservices.trustedwebactivityui.TrustedWebActivityModel.DISCLOSURE_STATE_NOT_SHOWN;
import static org.chromium.chrome.browser.browserservices.trustedwebactivityui.TrustedWebActivityModel.DISCLOSURE_STATE_SHOWN;
import static org.chromium.chrome.browser.browserservices.trustedwebactivityui.TrustedWebActivityModel.PACKAGE_NAME;
import static org.chromium.chrome.browser.dependency_injection.ChromeCommonQualifiers.APP_CONTEXT;
import static org.chromium.chrome.browser.notifications.NotificationConstants.NOTIFICATION_ID_TWA_DISCLOSURE;

import android.content.Context;
import android.content.res.Resources;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.DisclosureAcceptanceBroadcastReceiver;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.TrustedWebActivityModel;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.StartStopWithNativeObserver;
import org.chromium.chrome.browser.notifications.NotificationBuilderFactory;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.components.browser_ui.notifications.ChromeNotification;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxyImpl;
import org.chromium.components.browser_ui.notifications.NotificationMetadata;
import org.chromium.components.browser_ui.notifications.PendingIntentProvider;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyObservable;

import javax.inject.Inject;
import javax.inject.Named;

import androidx.annotation.Nullable;
import androidx.core.app.NotificationCompat;

/**
 * Displays a notification when the user is on the verified domain. The first such notification (per
 * TWA) is urgent priority, subsequent ones are low priority.
 */
public class DisclosureNotification implements
        PropertyObservable.PropertyObserver<PropertyKey>, StartStopWithNativeObserver {
    private final Context mContext;
    private final Resources mResources;
    private final TrustedWebActivityModel mModel;
    private final NotificationManagerProxy mNotificationManager;
    private String mCurrentScope;

    @Inject
    DisclosureNotification(
            @Named(APP_CONTEXT) Context context,
            Resources resources,
            TrustedWebActivityModel model,
            ActivityLifecycleDispatcher lifecycleDispatcher) {
        mContext = context;
        mResources = resources;
        mNotificationManager = new NotificationManagerProxyImpl(mContext);
        mModel = model;
        mModel.addObserver(this);
        lifecycleDispatcher.register(this);
    }

    private void show() {
        mCurrentScope = mModel.get(DISCLOSURE_SCOPE);
        boolean firstTime = mModel.get(DISCLOSURE_FIRST_TIME);
        String packageName = mModel.get(PACKAGE_NAME);

        ChromeNotification notification = createNotification(firstTime, mCurrentScope, packageName);
        mNotificationManager.notify(notification);

        mModel.get(DISCLOSURE_EVENTS_CALLBACK).onDisclosureShown();
    }

    private void dismiss() {
        mNotificationManager.cancel(mCurrentScope, NOTIFICATION_ID_TWA_DISCLOSURE);
        mCurrentScope = null;
    }

    private ChromeNotification createNotification(boolean firstTime, String scope,
            String packageName) {
        int umaType;
        int preOPriority;
        String channelId;
        if (firstTime) {
            umaType = NotificationUmaTracker.SystemNotificationType.TWA_DISCLOSURE_INITIAL;
            preOPriority = NotificationCompat.PRIORITY_MAX;
            channelId = ChromeChannelDefinitions.ChannelId.TWA_DISCLOSURE_INITIAL;
        } else {
            umaType = NotificationUmaTracker.SystemNotificationType.TWA_DISCLOSURE_SUBSEQUENT;
            preOPriority = NotificationCompat.PRIORITY_MIN;
            channelId = ChromeChannelDefinitions.ChannelId.TWA_DISCLOSURE_SUBSEQUENT;
        }

        // We use the TWA's package name as the notification tag so that multiple different TWAs
        // don't interfere with each other.
        NotificationMetadata metadata =
                new NotificationMetadata(umaType, scope, NOTIFICATION_ID_TWA_DISCLOSURE);

        String title = mResources.getString(R.string.twa_running_in_chrome);
        String text = mResources.getString(R.string.twa_running_in_chrome_v2, scope);

        boolean preferCompat = true;
        // The notification is being displayed by Chrome, so we don't need to provide a
        // remoteAppPackageName.
        String remoteAppPackageName = null;
        PendingIntentProvider intent = DisclosureAcceptanceBroadcastReceiver.createPendingIntent(
                mContext, scope, NOTIFICATION_ID_TWA_DISCLOSURE, packageName);

        return NotificationBuilderFactory
                .createChromeNotificationBuilder(
                        preferCompat, channelId, remoteAppPackageName, metadata)
                .setSmallIcon(R.drawable.ic_chrome)
                .setContentTitle(title)
                .setContentText(text)
                .setContentIntent(intent)
                .setShowWhen(false)
                .setAutoCancel(false)
                .setOngoing(!firstTime)
                .setPriorityBeforeO(preOPriority)
                .buildChromeNotification();
    }

    @Override
    public void onPropertyChanged(PropertyObservable<PropertyKey> source,
            @Nullable PropertyKey propertyKey) {
        if (propertyKey != DISCLOSURE_STATE) return;

        switch (mModel.get(DISCLOSURE_STATE)) {
            case DISCLOSURE_STATE_SHOWN:
                show();
                break;
            case DISCLOSURE_STATE_NOT_SHOWN:
                dismiss();
                break;
        }
    }

    @Override
    public void onStartWithNative() {
        if (mModel.get(DISCLOSURE_STATE) == DISCLOSURE_STATE_SHOWN) show();
    }

    @Override
    public void onStopWithNative() {
        dismiss();
    }
}
