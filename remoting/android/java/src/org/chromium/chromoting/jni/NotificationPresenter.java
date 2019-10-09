// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting.jni;

import android.app.Activity;
import android.content.DialogInterface;
import android.content.Intent;
import android.net.Uri;
import android.support.v7.app.AlertDialog;

import androidx.annotation.IntDef;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chromoting.Preconditions;
import org.chromium.chromoting.R;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Helper for fetching and presenting a notification */
@JNINamespace("remoting")
public final class NotificationPresenter {
    @IntDef({State.NOT_FETCHED, State.FETCHING, State.FETCHED})
    @Retention(RetentionPolicy.SOURCE)
    private @interface State {
        int NOT_FETCHED = 0;
        int FETCHING = 1;
        int FETCHED = 2;
    }

    private final long mNativeJniNotificationPresenter;
    private final Activity mActivity;

    private @State int mState;

    /**
     * @param activity The activity on which the notification will be shown.
     */
    public NotificationPresenter(Activity activity) {
        mNativeJniNotificationPresenter = NotificationPresenterJni.get().init(this);
        mActivity = activity;
        mState = State.NOT_FETCHED;
    }

    @Override
    public void finalize() {
        NotificationPresenterJni.get().destroy(mNativeJniNotificationPresenter);
    }

    /**
     * Presents notification for the given |username| if no previous notification has been shown,
     * and the user is selected for the notification.
     * @param username String that can uniquely identify a user
     */
    public void presentIfNecessary(String username) {
        if (mState != State.NOT_FETCHED) {
            return;
        }
        mState = State.FETCHING;
        NotificationPresenterJni.get().fetchNotification(mNativeJniNotificationPresenter, username);
    }

    @CalledByNative
    void onNotificationFetched(String messageText, String linkText, String linkUrl) {
        Preconditions.isTrue(mState == State.FETCHING);
        mState = State.FETCHED;

        // TODO(yuweih): Add Don't Show Again support.
        final AlertDialog.Builder builder = new AlertDialog.Builder(mActivity);
        builder.setMessage(messageText);
        builder.setPositiveButton(linkText, (DialogInterface dialog, int id) -> {
            Intent openLink = new Intent(Intent.ACTION_VIEW, Uri.parse(linkUrl));
            mActivity.startActivity(openLink);
        });
        builder.setNegativeButton(R.string.dismiss,
                (DialogInterface dialog, int id)
                        -> {
                                // Do nothing
                        });
        final AlertDialog dialog = builder.create();
        dialog.show();
    }

    @CalledByNative
    void onNoNotification() {
        Preconditions.isTrue(mState == State.FETCHING);
        mState = State.NOT_FETCHED;
    }

    @NativeMethods
    interface Natives {
        long init(NotificationPresenter javaPresenter);
        void fetchNotification(long nativeJniNotificationPresenter, String username);
        void destroy(long nativeJniNotificationPresenter);
    }
}
