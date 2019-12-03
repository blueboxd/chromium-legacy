// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

/**
 * Callback object for results of asynchronous {@link CrashReporterController} operations.
 */
public abstract class CrashReporterCallback {
    /**
     * Called as a result of a new crash being detected, or with the result of {@link
     * CrashReporterController#getPendingCrashes}
     *
     * @param localIds an array of crash report IDs available to be uploaded.
     */
    public void onPendingCrashReports(String[] localIds) {}

    /**
     * Called when a crash has been deleted.
     *
     * @param localId the local identifier of the crash that was deleted.
     */
    public void onCrashDeleted(String localId) {}

    /**
     * Called when a crash has been uploaded.
     *
     * @param localId the local identifier of the crash that was uploaded.
     * @param reportId the remote identifier for the uploaded crash.
     */
    public void onCrashUploadSucceeded(String localId, String reportId) {}

    /**
     * Called when a crash failed to upload.
     *
     * @param localId the local identifier of the crash that failed to upload.
     * @param failureReason a free text string giving the failure reason.
     */
    public void onCrashUploadFailed(String localId, String faulreReason) {}
}
