// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_check;

import android.content.Context;

import org.chromium.base.Callback;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.omaha.OmahaBase.UpdateStatus;
import org.chromium.chrome.browser.omaha.OmahaService;
import org.chromium.chrome.browser.safety_check.SafetyCheckModel.Updates;
import org.chromium.content_public.browser.UiThreadTaskTraits;

/**
 * Glue code for interactions between Safety check and Omaha on Android.
 *
 * This class is needed because {@link OmahaService} is in //chrome/android,
 * while Safety check is modularized in //chrome/browser. Once Omaha is
 * modularized as well, this class will not be needed anymore.
 */
public class SafetyCheckOmahaClient implements SafetyCheckUpdatesDelegate {
    private OmahaService mOmaha;

    /**
     * Creates a new instance of the glue class to be passed to
     * {@link SafetyCheckSettingsFragment}.
     * @param context A {@link Context} object, used by Omaha.
     */
    public SafetyCheckOmahaClient(Context context) {
        mOmaha = OmahaService.getInstance(context);
    }

    /**
     * Converts Omaha's {@link UpdateStatus} into a
     * {@link SafetyCheckModel.Updates} enum value.
     * @param status Update status returned by Omaha.
     * @return A corresponding {@link SafetyCheckModel.Updates} value.
     */
    public static Updates convertOmahaUpdateStatus(@UpdateStatus int status) {
        switch (status) {
            case UpdateStatus.UPDATED:
                return Updates.UPDATED;
            case UpdateStatus.OUTDATED:
                return Updates.OUTDATED;
            case UpdateStatus.OFFLINE:
                return Updates.OFFLINE;
            case UpdateStatus.FAILED: // Intentional fall through.
            default:
                return Updates.ERROR;
        }
    }

    /**
     * Assynchronously checks for updates and invokes the provided callback with
     * the result.
     * @param statusCallback A callback to invoke with the result.
     */
    @Override
    public void checkForUpdates(Callback<Updates> statusCallback) {
        PostTask.postTask(TaskTraits.USER_VISIBLE, () -> {
            @UpdateStatus
            int status = mOmaha.checkForUpdates();
            // Post the results back to the UI thread.
            PostTask.postTask(UiThreadTaskTraits.DEFAULT,
                    () -> { statusCallback.onResult(convertOmahaUpdateStatus(status)); });
        });
    }
}
