// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import static org.chromium.chrome.browser.browserservices.TrustedWebActivityUmaRecorder.DelegatedNotificationSmallIconFallback.FALLBACK_ICON_NOT_PROVIDED;
import static org.chromium.chrome.browser.browserservices.TrustedWebActivityUmaRecorder.DelegatedNotificationSmallIconFallback.NO_FALLBACK;

import android.app.Notification;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ResolveInfo;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.net.Uri;
import android.os.RemoteException;

import com.google.common.util.concurrent.ListenableFuture;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.browserservices.TrustedWebActivityUmaRecorder.DelegatedNotificationSmallIconFallback;
import org.chromium.chrome.browser.browserservices.permissiondelegation.TrustedWebActivityPermissionManager;
import org.chromium.chrome.browser.notifications.NotificationBuilderBase;
import org.chromium.chrome.browser.notifications.NotificationMetadata;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.content_public.browser.UiThreadTaskTraits;

import java.util.List;
import java.util.Set;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.Executor;

import javax.inject.Inject;
import javax.inject.Singleton;

import androidx.annotation.Nullable;
import androidx.browser.trusted.Token;
import androidx.browser.trusted.TrustedWebActivityService;
import androidx.browser.trusted.TrustedWebActivityServiceConnection;
import androidx.browser.trusted.TrustedWebActivityServiceConnectionPool;

/**
 * Uses a Trusted Web Activity client to display notifications.
 */
@Singleton
public class TrustedWebActivityClient {
    private static final String TAG = "TWAClient";
    private static final Executor UI_THREAD_EXECUTOR =
            (Runnable r) -> PostTask.postTask(UiThreadTaskTraits.USER_VISIBLE, r);

    private final TrustedWebActivityServiceConnectionPool mConnection;
    private final TrustedWebActivityPermissionManager mDelegatesManager;
    private final TrustedWebActivityUmaRecorder mRecorder;

    /** Interface for callbacks to {@link #checkNotificationPermission}. */
    public interface NotificationPermissionCheckCallback {
        /** May be called as a result of {@link #checkNotificationPermission}. */
        void onPermissionCheck(ComponentName answeringApp, boolean enabled);
    }

    /**
     * Creates a TrustedWebActivityService.
     */
    @Inject
    public TrustedWebActivityClient(TrustedWebActivityServiceConnectionPool connection,
            TrustedWebActivityPermissionManager delegatesManager,
            TrustedWebActivityUmaRecorder recorder) {
        mConnection = connection;
        mDelegatesManager = delegatesManager;
        mRecorder = recorder;
    }

    /**
     * Whether a Trusted Web Activity client is available to display notifications of the given
     * scope.
     * @param scope The scope of the Service Worker that triggered the notification.
     * @return Whether a Trusted Web Activity client was found to show the notification.
     */
    public boolean twaExistsForScope(Uri scope) {
        Origin origin = Origin.create(scope);
        if (origin == null) return false;
        Set<Token> possiblePackages = mDelegatesManager.getAllDelegateApps(origin);
        if (possiblePackages == null) return false;
        return mConnection.serviceExistsForScope(scope, possiblePackages);
    }

    /**
     * Checks whether the TWA of the given origin has the notification permission granted.
     * @param callback Will be called on a background thread with whether the permission is granted.
     * @return {@code false} if no such TWA exists (in which case the callback will not be called).
     *         Ensure that the app has been added to the {@link TrustedWebActivityPermissionManager}
     *         before calling this.
     */
    public boolean checkNotificationPermission(Origin origin,
            NotificationPermissionCheckCallback callback) {
        Resources res = ContextUtils.getApplicationContext().getResources();
        String channelDisplayName = res.getString(R.string.notification_category_group_general);

        return connectAndExecute(origin.uri(), service ->
                callback.onPermissionCheck(service.getComponentName(),
                        service.areNotificationsEnabled(channelDisplayName)));
    }

    /**
     * Displays a notification through a Trusted Web Activity client.
     * @param scope The scope of the Service Worker that triggered the notification.
     * @param platformTag A notification tag.
     * @param platformId A notification id.
     * @param builder A builder for the notification to display.
     *                The Trusted Web Activity client may override the small icon.
     * @param notificationUmaTracker To log Notification UMA.
     */
    public void notifyNotification(Uri scope, String platformTag, int platformId,
            NotificationBuilderBase builder, NotificationUmaTracker notificationUmaTracker) {
        Resources res = ContextUtils.getApplicationContext().getResources();
        String channelDisplayName = res.getString(R.string.notification_category_group_general);
        Origin origin = Origin.createOrThrow(scope);

        connectAndExecute(scope, service -> {
            if (!service.areNotificationsEnabled(channelDisplayName)) {
                mDelegatesManager.updatePermission(origin,
                        service.getComponentName().getPackageName(), false);

                // Attempting to notify when notifications are disabled won't have any effect, but
                // returning here just saves us from doing unnecessary work.
                return;
            }

            fallbackToIconFromServiceIfNecessary(builder, service);

            NotificationMetadata metadata = new NotificationMetadata(
                    NotificationUmaTracker.SystemNotificationType.TRUSTED_WEB_ACTIVITY_SITES,
                    platformTag, platformId);
            Notification notification = builder.build(metadata).getNotification();

            service.notify(platformTag, platformId, notification, channelDisplayName);

            notificationUmaTracker.onNotificationShown(
                    NotificationUmaTracker.SystemNotificationType.TRUSTED_WEB_ACTIVITY_SITES,
                    notification);
        });
    }

    private void fallbackToIconFromServiceIfNecessary(NotificationBuilderBase builder,
            TrustedWebActivityServiceConnection service) throws RemoteException {
        if (builder.hasSmallIconForContent() && builder.hasStatusBarIconBitmap()) {
            recordFallback(NO_FALLBACK);
            return;
        }

        int id = service.getSmallIconId();
        if (id == TrustedWebActivityService.SMALL_ICON_NOT_SET) {
            recordFallback(FALLBACK_ICON_NOT_PROVIDED);
            return;
        }

        recordFallback(builder.hasSmallIconForContent()
                ? DelegatedNotificationSmallIconFallback.FALLBACK_FOR_STATUS_BAR
                : DelegatedNotificationSmallIconFallback.FALLBACK_FOR_STATUS_BAR_AND_CONTENT);

        Bitmap bitmap = service.getSmallIconBitmap();
        if (!builder.hasStatusBarIconBitmap()) {
            builder.setStatusBarIconForRemoteApp(
                    id, bitmap, service.getComponentName().getPackageName());
        }
        if (!builder.hasSmallIconForContent()) {
            builder.setContentSmallIconForRemoteApp(bitmap);
        }
    }

    private void recordFallback(@DelegatedNotificationSmallIconFallback int fallback) {
        mRecorder.recordDelegatedNotificationSmallIconFallback(fallback);
    }

    /**
     * Cancels a notification through a Trusted Web Activity client.
     * @param scope The scope of the Service Worker that triggered the notification.
     * @param platformTag The tag of the notification to cancel.
     * @param platformId The id of the notification to cancel.
     */
    public void cancelNotification(Uri scope, String platformTag, int platformId) {
        connectAndExecute(scope, service -> service.cancel(platformTag, platformId));
    }

    private interface ExecutionCallback {
        void onConnected(TrustedWebActivityServiceConnection service) throws RemoteException;
    }

    private boolean connectAndExecute(Uri scope, ExecutionCallback callback) {
        Origin origin = Origin.createOrThrow(scope);
        Set<Token> possiblePackages = mDelegatesManager.getAllDelegateApps(origin);
        if (possiblePackages == null || possiblePackages.isEmpty()) return false;

        ListenableFuture<TrustedWebActivityServiceConnection> connection =
                mConnection.connect(scope, possiblePackages, AsyncTask.THREAD_POOL_EXECUTOR);
        connection.addListener(() -> {
            try {
                callback.onConnected(connection.get());
            } catch (RemoteException | ExecutionException | InterruptedException e) {
                Log.w(TAG, "Failed to execute TWA command.");
            }
        }, UI_THREAD_EXECUTOR);

        return true;
    }

    /**
     * Searches through the given list of {@link ResolveInfo} for an Activity belonging to a package
     * that is verified for the given url. If such an Activity is found, an Intent to start that
     * Activity as a Trusted Web Activity is returned. Otherwise {@code null} is returned.
     *
     * If multiple {@link ResolveInfo}s in the list match this criteria, the first will be chosen.
     */
    public static @Nullable Intent createLaunchIntentForTwa(Context appContext, String url,
            List<ResolveInfo> resolveInfosForUrl) {
        // This is ugly, but the call site for this is static and called by native.
        TrustedWebActivityClient client =
                ChromeApplication.getComponent().resolveTrustedWebActivityClient();
        return client.createLaunchIntentForTwaInternal(appContext, url, resolveInfosForUrl);
    }

    private @Nullable Intent createLaunchIntentForTwaInternal(Context appContext, String url,
            List<ResolveInfo> resolveInfosForUrl) {
        Origin origin = Origin.createOrThrow(url);

        // Trusted Web Activities only work with https so we can shortcut here.
        if (!UrlConstants.HTTPS_SCHEME.equals(origin.uri().getScheme())) return null;

        Set<Token> verifiedPackages = mDelegatesManager.getAllDelegateApps(origin);
        if (verifiedPackages == null || verifiedPackages.size() == 0) return null;

        String twaPackageName = null;
        String twaActivityName = null;
        for (ResolveInfo info : resolveInfosForUrl) {
            if (info.activityInfo == null) continue;

            Token token =
                    Token.create(info.activityInfo.packageName, appContext.getPackageManager());
            if (token == null) continue;

            if (verifiedPackages.contains(token)) {
                twaPackageName = info.activityInfo.packageName;
                twaActivityName = info.activityInfo.name;
                break;
            }
        }

        if (twaPackageName == null) return null;

        Intent intent = new Intent();
        intent.setData(Uri.parse(url));
        intent.setAction(Intent.ACTION_VIEW);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK
                | Intent.FLAG_ACTIVITY_CLEAR_TOP);
        intent.setComponent(new ComponentName(twaPackageName, twaActivityName));
        return intent;
    }
}
