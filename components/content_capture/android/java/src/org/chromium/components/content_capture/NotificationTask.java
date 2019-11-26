// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_capture;

import android.annotation.TargetApi;
import android.content.LocusId;
import android.graphics.Rect;
import android.os.Build;
import android.text.TextUtils;
import android.view.ViewStructure;
import android.view.autofill.AutofillId;
import android.view.contentcapture.ContentCaptureContext;
import android.view.contentcapture.ContentCaptureSession;

import org.chromium.base.Log;
import org.chromium.base.annotations.VerifiesOnQ;
import org.chromium.base.task.AsyncTask;
import org.chromium.components.content_capture.PlatformSession.PlatformSessionData;

/**
 * The background task to talk to the ContentCapture Service.
 */
@VerifiesOnQ
@TargetApi(Build.VERSION_CODES.Q)
abstract class NotificationTask extends AsyncTask<Boolean> {
    private static final String TAG = "ContentCapture";
    private static Boolean sDump;

    protected final FrameSession mSession;
    protected final PlatformSession mPlatformSession;

    public NotificationTask(FrameSession session, PlatformSession platformSession) {
        mSession = session;
        mPlatformSession = platformSession;
        if (sDump == null) sDump = ContentCaptureFeatures.isDumpForTestingEnabled();
    }

    // Build up FrameIdToPlatformSessionData map of mSession, and return the current
    // session the task should run against.
    public PlatformSessionData buildCurrentSession() {
        if (mSession == null || mSession.isEmpty()) {
            return mPlatformSession.getRootPlatformSessionData();
        }
        // Build the session from root.
        PlatformSessionData platformSessionData = mPlatformSession.getRootPlatformSessionData();
        for (int i = mSession.size() - 1; i >= 0; i--) {
            platformSessionData = createOrGetSession(platformSessionData, mSession.get(i));
            if (platformSessionData == null) break;
        }
        return platformSessionData;
    }

    protected AutofillId notifyViewAppeared(
            PlatformSessionData parentPlatformSessionData, ContentCaptureData data) {
        ViewStructure viewStructure =
                parentPlatformSessionData.contentCaptureSession.newVirtualViewStructure(
                        parentPlatformSessionData.autofillId, data.getId());

        if (!data.hasChildren()) viewStructure.setText(data.getValue());
        Rect rect = data.getBounds();
        // Always set scroll as (0, 0).
        viewStructure.setDimens(rect.left, rect.top, 0, 0, rect.width(), rect.height());
        parentPlatformSessionData.contentCaptureSession.notifyViewAppeared(viewStructure);
        return viewStructure.getAutofillId();
    }

    public PlatformSessionData createOrGetSession(
            PlatformSessionData parentPlatformSessionData, ContentCaptureData frame) {
        PlatformSessionData platformSessionData =
                mPlatformSession.getFrameIdToPlatformSessionData().get(frame.getId());
        if (platformSessionData == null && !TextUtils.isEmpty(frame.getValue())) {
            ContentCaptureSession session =
                    parentPlatformSessionData.contentCaptureSession.createContentCaptureSession(
                            new ContentCaptureContext.Builder(new LocusId(frame.getValue()))
                                    .build());
            AutofillId autofillId = parentPlatformSessionData.contentCaptureSession.newAutofillId(
                    mPlatformSession.getRootPlatformSessionData().autofillId, frame.getId());
            autofillId = notifyViewAppeared(parentPlatformSessionData, frame);
            platformSessionData = new PlatformSessionData(session, autofillId);
            mPlatformSession.getFrameIdToPlatformSessionData().put(
                    frame.getId(), platformSessionData);
        }
        return platformSessionData;
    }

    protected void log(String message) {
        if (sDump.booleanValue()) Log.i(TAG, message);
    }

    @Override
    protected void onPostExecute(Boolean result) {}
}
