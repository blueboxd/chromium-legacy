// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.android_webview.ui.util;

import java.util.List;

/**
 * An abstract class that collects info about WebView crashes.
 */
public abstract class CrashInfoLoader {
    /**
     * Crash file report/minidump upload status.
     */
    public static enum UploadState {
        SKIPPED,
        PENDING,
        PENDING_USER_REQUESTED,
        UPLOADED,
    }

    /**
     * A class that bundles various information about a crash.
     */
    public static class CrashInfo {
        public UploadState uploadState;

        // ID for locally stored data that may or may not be uploaded.
        public String localId;
        // The time the data was captured. This is useful if the data is stored locally when
        // captured and uploaded at a later time.
        public long captureTime = -1;
        public String packageName;
        public List<String> variations;

        // These fields are only valid when |uploadState| == Uploaded.
        public String uploadId;
        public long uploadTime = -1;
    }

    /**
     * Loads all crashes info from source.
     *
     * @return list of crashes info.
     */
    public abstract List<CrashInfo> loadCrashesInfo();
}
