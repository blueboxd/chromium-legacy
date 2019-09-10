// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

/**
 * A class to record User Keyed Metrics relevant to WebAPKs. This
 * will allow us to concentrate on the use cases for the most used WebAPKs.
 */
public class WebApkUkmRecorder {
    /**
     * Records the duration, in exponentially-bucketed milliseconds, of a WebAPK session (from
     * launch/foreground to background).
     */
    public static void recordWebApkSessionDuration(String manifestUrl,
            @WebApkDistributor int distributor, int versionCode, long duration) {
        nativeRecordSessionDuration(manifestUrl, distributor, versionCode, duration);
    }

    /**
     * Records that WebAPK was launched and the reason for the launch.
     */
    public static void recordWebApkLaunch(
            String manifestUrl, @WebApkDistributor int distributor, int versionCode, int source) {
        nativeRecordVisit(manifestUrl, distributor, versionCode, source);
    }

    private static native void nativeRecordSessionDuration(
            String manifestUrl, int distributor, int versionCode, long duration);
    private static native void nativeRecordVisit(
            String manifestUrl, int distributor, int versionCode, int source);
}
