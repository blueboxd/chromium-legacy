// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_capture;

import android.annotation.TargetApi;
import android.content.ComponentName;
import android.content.Context;
import android.content.LocusId;
import android.os.Build;
import android.view.contentcapture.ContentCaptureCondition;
import android.view.contentcapture.ContentCaptureManager;
import android.view.contentcapture.DataRemovalRequest;

import org.chromium.base.BuildInfo;
import org.chromium.base.Log;
import org.chromium.base.annotations.VerifiesOnQ;

import java.util.Set;

/**
 * The implementation of ContentCaptureController to verify ContentCaptureService
 * is Aiai and signed by right certificate, this class also read whitelist from
 * framework and set to native side.
 */
// TODO(crbug.com/965566): Write junit tests for this when junit supports q.
@VerifiesOnQ
@TargetApi(Build.VERSION_CODES.Q)
public class ContentCaptureControllerImpl extends ContentCaptureController {
    private static final String TAG = "ContentCapture";

    private static final String AIAI_PACKAGE_NAME = "com.google.android.as";

    private boolean mShouldStartCapture;
    private ContentCaptureManager mContentCaptureManager;

    public static void init(Context context) {
        sContentCaptureController = new ContentCaptureControllerImpl(context);
    }

    public ContentCaptureControllerImpl(Context context) {
        mContentCaptureManager = context.getSystemService(ContentCaptureManager.class);
        verifyService();
    }

    @Override
    public boolean shouldStartCapture() {
        return mShouldStartCapture;
    }

    private void verifyService() {
        if (mContentCaptureManager == null) {
            log("ContentCaptureManager isn't available.");
            return;
        }

        ComponentName componentName = mContentCaptureManager.getServiceComponentName();
        if (componentName == null) {
            log("Service isn't available.");
            return;
        }

        mShouldStartCapture = AIAI_PACKAGE_NAME.equals(componentName.getPackageName());
        if (!mShouldStartCapture) {
            log("Package doesn't match, current one is "
                    + mContentCaptureManager.getServiceComponentName().getPackageName());
            // Disable the ContentCapture if there is no testing flag.
            if (!BuildInfo.isDebugAndroid() && !ContentCaptureFeatures.isDumpForTestingEnabled()) {
                return;
            }
        }

        mShouldStartCapture = mContentCaptureManager.isContentCaptureEnabled();
        if (!mShouldStartCapture) {
            log("ContentCapture disabled.");
        }
    }

    @Override
    protected void pullWhitelist() {
        Set<ContentCaptureCondition> conditions =
                mContentCaptureManager.getContentCaptureConditions();
        String[] whiteList = null;
        boolean[] isRegEx = null;
        if (conditions != null) {
            whiteList = new String[conditions.size()];
            isRegEx = new boolean[conditions.size()];
            int index = 0;
            for (ContentCaptureCondition c : conditions) {
                whiteList[index] = c.getLocusId().getId();
                isRegEx[index] = (c.getFlags() & ContentCaptureCondition.FLAG_IS_REGEX) != 0;
                index++;
            }
        }
        setWhitelist(whiteList, isRegEx);
    }

    private void log(String msg) {
        if (!ContentCaptureFeatures.isDumpForTestingEnabled()) return;
        Log.i(TAG, msg);
    }

    @Override
    public void clearAllContentCaptureData() {
        if (mContentCaptureManager == null) return;

        mContentCaptureManager.removeData(new DataRemovalRequest.Builder().forEverything().build());
    }

    @Override
    public void clearContentCaptureDataForURLs(String[] urlsToDelete) {
        if (mContentCaptureManager == null) return;

        DataRemovalRequest.Builder builder = new DataRemovalRequest.Builder();
        for (String url : urlsToDelete) {
            builder = builder.addLocusId(
                    new LocusId(url), /* Signals that we aren't using extra flags */ 0);
        }
        mContentCaptureManager.removeData(builder.build());
    }
}
