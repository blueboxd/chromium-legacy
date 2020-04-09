// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.os.Build;
import android.os.RemoteException;
import android.text.TextUtils;

import androidx.core.app.NotificationCompat;
import androidx.core.app.NotificationManagerCompat;

import org.chromium.base.ContentUriUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.browser_ui.util.DownloadUtils;
import org.chromium.weblayer_private.interfaces.APICallException;
import org.chromium.weblayer_private.interfaces.DownloadError;
import org.chromium.weblayer_private.interfaces.DownloadState;
import org.chromium.weblayer_private.interfaces.IClientDownload;
import org.chromium.weblayer_private.interfaces.IDownload;
import org.chromium.weblayer_private.interfaces.IDownloadCallbackClient;
import org.chromium.weblayer_private.interfaces.StrictModeWorkaround;

import java.io.File;
import java.util.HashMap;

/**
 * Implementation of IDownload.
 */
@JNINamespace("weblayer")
public final class DownloadImpl extends IDownload.Stub {
    private static final String DOWNLOADS_PREFIX = "org.chromium.weblayer.downloads";

    // These actions have to be synchronized with the receiver defined in AndroidManifest.xml.
    private static final String OPEN_INTENT = DOWNLOADS_PREFIX + ".OPEN";
    private static final String DELETE_INTENT = DOWNLOADS_PREFIX + ".DELETE";
    private static final String PAUSE_INTENT = DOWNLOADS_PREFIX + ".PAUSE";
    private static final String RESUME_INTENT = DOWNLOADS_PREFIX + ".RESUME";
    private static final String CANCEL_INTENT = DOWNLOADS_PREFIX + ".CANCEL";

    private static final String EXTRA_NOTIFICATION_ID = DOWNLOADS_PREFIX + ".NOTIFICATION_ID";
    private static final String EXTRA_NOTIFICATION_LOCATION =
            DOWNLOADS_PREFIX + ".NOTIFICATION_LOCATION";
    private static final String EXTRA_NOTIFICATION_MIME_TYPE =
            DOWNLOADS_PREFIX + ".NOTIFICATION_MIME_TYPE";
    private static final String EXTRA_NOTIFICATION_PROFILE =
            DOWNLOADS_PREFIX + ".NOTIFICATION_PROFILE";
    private static final String CHANNEL_ID = DOWNLOADS_PREFIX + ".channel";
    // The intent prefix is used as the notification's tag since it's guaranteed not to conflict
    // with intent prefixes used by other subsystems that display notifications.
    private static final String NOTIFICATION_TAG = DOWNLOADS_PREFIX;
    private static final String TAG = "DownloadImpl";

    private final String mProfileName;
    private final IDownloadCallbackClient mClient;
    private final IClientDownload mClientDownload;
    // WARNING: DownloadImpl may outlive the native side, in which case this member is set to 0.
    private long mNativeDownloadImpl;
    private boolean mDisableNotification;

    private final int mNotificationId;
    private NotificationCompat.Builder mBuilder;
    private static boolean sCreatedChannel = false;
    private static final HashMap<Integer, DownloadImpl> sMap = new HashMap<Integer, DownloadImpl>();

    /**
     * @return a string that prefixes all intents that can be handled by {@link forwardIntent}.
     */
    public static String getIntentPrefix() {
        return DOWNLOADS_PREFIX;
    }

    public static void forwardIntent(
            Context context, Intent intent, ProfileManager profileManager) {
        if (intent.getAction().equals(OPEN_INTENT)) {
            String location = intent.getStringExtra(EXTRA_NOTIFICATION_LOCATION);
            if (TextUtils.isEmpty(location)) {
                Log.d(TAG, "Didn't find location for open intent");
                return;
            }

            String mimeType = intent.getStringExtra(EXTRA_NOTIFICATION_MIME_TYPE);

            Intent openIntent = new Intent(Intent.ACTION_VIEW);
            if (TextUtils.isEmpty(mimeType)) {
                openIntent.setData(ContentUriUtils.getContentUriFromFile(new File(location)));
            } else {
                openIntent.setDataAndType(
                        ContentUriUtils.getContentUriFromFile(new File(location)), mimeType);
            }
            openIntent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
            openIntent.addFlags(Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
            openIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            try {
                context.startActivity(openIntent);
            } catch (ActivityNotFoundException ex) {
                // TODO: show some UI that there were no apps to handle this?
            }

            return;
        }

        String profileName = intent.getStringExtra(EXTRA_NOTIFICATION_PROFILE);

        ProfileImpl profile = profileManager.getProfile(profileName);
        if (!profile.areDownloadsInitialized()) {
            profile.addDownloadNotificationIntent(intent);
        } else {
            handleIntent(intent);
        }
    }

    public static void handleIntent(Intent intent) {
        int id = intent.getIntExtra(EXTRA_NOTIFICATION_ID, -1);
        DownloadImpl download = sMap.get(id);
        if (download == null) {
            Log.d(TAG, "Didn't find download for " + id);
            // TODO(jam): handle download resumption after restart
            return;
        }

        if (intent.getAction().equals(PAUSE_INTENT)) {
            download.pause();
        } else if (intent.getAction().equals(RESUME_INTENT)) {
            download.resume();
        } else if (intent.getAction().equals(CANCEL_INTENT)) {
            download.cancel();
        } else if (intent.getAction().equals(DELETE_INTENT)) {
            sMap.remove(id);
        }
    }

    public DownloadImpl(
            String profileName, IDownloadCallbackClient client, long nativeDownloadImpl, int id) {
        mProfileName = profileName;
        mClient = client;
        mNativeDownloadImpl = nativeDownloadImpl;
        mNotificationId = id;
        if (mClient == null) {
            mClientDownload = null;
        } else {
            try {
                mClientDownload = mClient.createClientDownload(this);
            } catch (RemoteException e) {
                throw new APICallException(e);
            }
        }
        DownloadImplJni.get().setJavaDownload(mNativeDownloadImpl, DownloadImpl.this);
    }

    public IClientDownload getClientDownload() {
        return mClientDownload;
    }

    @DownloadState
    private static int implStateToJavaType(@ImplDownloadState int type) {
        switch (type) {
            case ImplDownloadState.IN_PROGRESS:
                return DownloadState.IN_PROGRESS;
            case ImplDownloadState.COMPLETE:
                return DownloadState.COMPLETE;
            case ImplDownloadState.PAUSED:
                return DownloadState.PAUSED;
            case ImplDownloadState.CANCELLED:
                return DownloadState.CANCELLED;
            case ImplDownloadState.FAILED:
                return DownloadState.FAILED;
        }
        assert false;
        return DownloadState.FAILED;
    }

    @DownloadError
    private static int implErrorToJavaType(@ImplDownloadError int type) {
        switch (type) {
            case ImplDownloadError.NO_ERROR:
                return DownloadError.NO_ERROR;
            case ImplDownloadError.SERVER_ERROR:
                return DownloadError.SERVER_ERROR;
            case ImplDownloadError.SSL_ERROR:
                return DownloadError.SSL_ERROR;
            case ImplDownloadError.CONNECTIVITY_ERROR:
                return DownloadError.CONNECTIVITY_ERROR;
            case ImplDownloadError.NO_SPACE:
                return DownloadError.NO_SPACE;
            case ImplDownloadError.FILE_ERROR:
                return DownloadError.FILE_ERROR;
            case ImplDownloadError.CANCELLED:
                return DownloadError.CANCELLED;
            case ImplDownloadError.OTHER_ERROR:
                return DownloadError.OTHER_ERROR;
        }
        assert false;
        return DownloadError.OTHER_ERROR;
    }

    @Override
    @DownloadState
    public int getState() {
        StrictModeWorkaround.apply();
        throwIfNativeDestroyed();
        return implStateToJavaType(
                DownloadImplJni.get().getState(mNativeDownloadImpl, DownloadImpl.this));
    }

    @Override
    public long getTotalBytes() {
        StrictModeWorkaround.apply();
        throwIfNativeDestroyed();
        return DownloadImplJni.get().getTotalBytes(mNativeDownloadImpl, DownloadImpl.this);
    }

    @Override
    public long getReceivedBytes() {
        StrictModeWorkaround.apply();
        throwIfNativeDestroyed();
        return DownloadImplJni.get().getReceivedBytes(mNativeDownloadImpl, DownloadImpl.this);
    }

    @Override
    public void pause() {
        StrictModeWorkaround.apply();
        throwIfNativeDestroyed();
        DownloadImplJni.get().pause(mNativeDownloadImpl, DownloadImpl.this);
        updateNotification();
    }

    @Override
    public void resume() {
        StrictModeWorkaround.apply();
        throwIfNativeDestroyed();
        DownloadImplJni.get().resume(mNativeDownloadImpl, DownloadImpl.this);
        updateNotification();
    }

    @Override
    public void cancel() {
        StrictModeWorkaround.apply();
        throwIfNativeDestroyed();
        DownloadImplJni.get().cancel(mNativeDownloadImpl, DownloadImpl.this);
        updateNotification();
    }

    @Override
    public String getLocation() {
        StrictModeWorkaround.apply();
        throwIfNativeDestroyed();
        return DownloadImplJni.get().getLocation(mNativeDownloadImpl, DownloadImpl.this);
    }

    @Override
    public String getMimeType() {
        StrictModeWorkaround.apply();
        throwIfNativeDestroyed();
        return DownloadImplJni.get().getMimeTypeImpl(mNativeDownloadImpl, DownloadImpl.this);
    }

    @Override
    @DownloadError
    public int getError() {
        StrictModeWorkaround.apply();
        throwIfNativeDestroyed();
        return implErrorToJavaType(
                DownloadImplJni.get().getError(mNativeDownloadImpl, DownloadImpl.this));
    }

    @Override
    public void disableNotification() {
        StrictModeWorkaround.apply();
        throwIfNativeDestroyed();
        mDisableNotification = true;

        if (mBuilder != null) {
            NotificationManagerCompat notificationManager = getNotificationManager();
            if (notificationManager != null) {
                notificationManager.cancel(NOTIFICATION_TAG, mNotificationId);
            }
            mBuilder = null;
        }
    }

    private void throwIfNativeDestroyed() {
        if (mNativeDownloadImpl == 0) {
            throw new IllegalStateException("Using Download after native destroyed");
        }
    }

    private Intent createIntent() {
        // Because the intent is using classes from the implementation's class loader,
        // we need to use the constructor which doesn't take the app's context.
        if (WebLayerFactoryImpl.getClientMajorVersion() >= 83) return WebLayerImpl.createIntent();

        try {
            return mClient.createIntent();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    public void downloadStarted() {
        if (mDisableNotification) return;

        // TODO(jam): create a foreground service while the download is running to avoid the process
        // being shut down if the user switches apps.

        if (!sCreatedChannel) createNotificationChannel();

        sMap.put(Integer.valueOf(mNotificationId), this);

        Intent deleteIntent = createIntent();
        deleteIntent.setAction(DELETE_INTENT);
        deleteIntent.putExtra(EXTRA_NOTIFICATION_ID, mNotificationId);
        deleteIntent.putExtra(EXTRA_NOTIFICATION_PROFILE, mProfileName);
        PendingIntent deletePendingIntent = PendingIntent.getBroadcast(
                ContextUtils.getApplicationContext(), mNotificationId, deleteIntent, 0);

        mBuilder = new NotificationCompat.Builder(ContextUtils.getApplicationContext(), CHANNEL_ID)
                           .setSmallIcon(android.R.drawable.stat_sys_download)
                           .setOngoing(true)
                           .setDeleteIntent(deletePendingIntent)
                           .setPriority(NotificationCompat.PRIORITY_DEFAULT);

        updateNotification();
    }

    public void downloadProgressChanged() {
        if (mBuilder == null) return;

        updateNotification();
    }

    public void downloadCompleted() {
        if (mBuilder == null) return;

        updateNotification();
    }

    public void downloadFailed() {
        if (mBuilder == null) return;
        updateNotification();
    }

    private void createNotificationChannel() {
        // Create the NotificationChannel, but only on API 26+ because
        // the NotificationChannel class is new and not in the support library
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            int importance = NotificationManager.IMPORTANCE_LOW;
            NotificationChannel channel =
                    new NotificationChannel(CHANNEL_ID, "Downloads", importance);
            NotificationManager notificationManager =
                    ContextUtils.getApplicationContext().getSystemService(
                            NotificationManager.class);
            notificationManager.createNotificationChannel(channel);
        }

        sCreatedChannel = true;
    }

    private void updateNotification() {
        if (mBuilder == null) return;

        NotificationManagerCompat notificationManager = getNotificationManager();

        // The filename might not have been available initially.
        String location = getLocation();
        if (!TextUtils.isEmpty((location))) {
            mBuilder.setContentTitle((new File(location)).getName());
        }

        @DownloadState
        int state = getState();
        if (state == DownloadState.CANCELLED) {
            if (notificationManager != null) {
                notificationManager.cancel(NOTIFICATION_TAG, mNotificationId);
            }
            mBuilder = null;
            return;
        }

        mBuilder.mActions.clear();

        Context context = ContextUtils.getApplicationContext();
        Resources resources = context.getResources();

        if (state == DownloadState.COMPLETE) {
            Intent openIntent = createIntent();
            openIntent.setAction(OPEN_INTENT);
            openIntent.putExtra(EXTRA_NOTIFICATION_ID, mNotificationId);
            openIntent.putExtra(EXTRA_NOTIFICATION_PROFILE, mProfileName);
            openIntent.putExtra(EXTRA_NOTIFICATION_LOCATION, getLocation());
            openIntent.putExtra(EXTRA_NOTIFICATION_MIME_TYPE, getMimeType());
            PendingIntent openPendingIntent =
                    PendingIntent.getBroadcast(context, mNotificationId, openIntent, 0);

            String contextText =
                    resources.getString(R.string.download_notification_completed_with_size,
                            DownloadUtils.getStringForBytes(context, getTotalBytes()));
            mBuilder.setContentText(contextText)
                    .setOngoing(false)
                    .setSmallIcon(android.R.drawable.stat_sys_download_done)
                    .setContentIntent(openPendingIntent)
                    .setAutoCancel(true)
                    .setProgress(0, 0, false);
        } else if (state == DownloadState.FAILED) {
            mBuilder.setContentText(resources.getString(R.string.download_notification_failed))
                    .setOngoing(false)
                    .setSmallIcon(android.R.drawable.stat_sys_download_done)
                    .setProgress(0, 0, false);
        } else if (state == DownloadState.IN_PROGRESS) {
            Intent pauseIntent = createIntent();
            pauseIntent.setAction(PAUSE_INTENT);
            pauseIntent.putExtra(EXTRA_NOTIFICATION_ID, mNotificationId);
            pauseIntent.putExtra(EXTRA_NOTIFICATION_PROFILE, mProfileName);
            PendingIntent pausePendingIntent =
                    PendingIntent.getBroadcast(context, mNotificationId, pauseIntent, 0);

            long bytes = getReceivedBytes();
            long totalBytes = getTotalBytes();
            boolean indeterminate = totalBytes == -1;
            int progressCurrent = -1;
            if (!indeterminate) progressCurrent = (int) (bytes * 100 / totalBytes);

            String contentText;
            String bytesString = DownloadUtils.getStringForBytes(context, bytes);
            if (indeterminate) {
                contentText =
                        resources.getString(R.string.download_ui_indeterminate_bytes, bytesString);
            } else {
                String totalString = DownloadUtils.getStringForBytes(context, totalBytes);
                contentText = resources.getString(
                        R.string.download_ui_determinate_bytes, bytesString, totalString);
            }
            mBuilder.setContentText(contentText)
                    .addAction(0 /* no icon */,
                            resources.getString(R.string.download_notification_pause_button),
                            pausePendingIntent)
                    .setSmallIcon(android.R.drawable.stat_sys_download)
                    .setProgress(100, progressCurrent, indeterminate);
        } else if (state == DownloadState.PAUSED) {
            Intent resumeIntent = createIntent();
            resumeIntent.setAction(RESUME_INTENT);
            resumeIntent.putExtra(EXTRA_NOTIFICATION_ID, mNotificationId);
            resumeIntent.putExtra(EXTRA_NOTIFICATION_PROFILE, mProfileName);
            PendingIntent resumePendingIntent =
                    PendingIntent.getBroadcast(context, mNotificationId, resumeIntent, 0);
            mBuilder.setContentText(resources.getString(R.string.download_notification_paused))
                    .addAction(0 /* no icon */,
                            resources.getString(R.string.download_notification_resume_button),
                            resumePendingIntent)
                    .setSmallIcon(android.R.drawable.ic_media_pause)
                    .setProgress(0, 0, false);
        }

        if (state == DownloadState.IN_PROGRESS || state == DownloadState.PAUSED) {
            Intent cancelIntent = createIntent();
            cancelIntent.setAction(CANCEL_INTENT);
            cancelIntent.putExtra(EXTRA_NOTIFICATION_ID, mNotificationId);
            cancelIntent.putExtra(EXTRA_NOTIFICATION_PROFILE, mProfileName);
            PendingIntent cancelPendingIntent =
                    PendingIntent.getBroadcast(context, mNotificationId, cancelIntent, 0);
            mBuilder.addAction(0 /* no icon */,
                    resources.getString(R.string.download_notification_cancel_button),
                    cancelPendingIntent);
        }

        if (notificationManager != null) {
            // mNotificationId is a unique int for each notification that you must define.
            notificationManager.notify(NOTIFICATION_TAG, mNotificationId, mBuilder.build());
        }
    }

    /**
     * Returns the notification manager. May return null if there's no context, e.g. when the
     * fragment is moving between activies or when the activity is destroyed before the fragment.
     */
    private NotificationManagerCompat getNotificationManager() {
        if (ContextUtils.getApplicationContext() == null) {
            return null;
        }
        return NotificationManagerCompat.from(ContextUtils.getApplicationContext());
    }

    @CalledByNative
    private void onNativeDestroyed() {
        mNativeDownloadImpl = 0;
        sMap.remove(mNotificationId);
        // TODO: this should likely notify delegate in some way.
    }

    @NativeMethods
    interface Natives {
        void setJavaDownload(long nativeDownloadImpl, DownloadImpl caller);
        int getState(long nativeDownloadImpl, DownloadImpl caller);
        long getTotalBytes(long nativeDownloadImpl, DownloadImpl caller);
        long getReceivedBytes(long nativeDownloadImpl, DownloadImpl caller);
        void pause(long nativeDownloadImpl, DownloadImpl caller);
        void resume(long nativeDownloadImpl, DownloadImpl caller);
        void cancel(long nativeDownloadImpl, DownloadImpl caller);
        String getLocation(long nativeDownloadImpl, DownloadImpl caller);
        String getMimeTypeImpl(long nativeDownloadImpl, DownloadImpl caller);
        int getError(long nativeDownloadImpl, DownloadImpl caller);
    }
}
