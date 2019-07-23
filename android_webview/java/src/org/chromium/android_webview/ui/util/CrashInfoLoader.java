// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.android_webview.ui.util;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.ArrayList;
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

        private static final String CRASH_LOCAL_ID_KEY = "crash-local-id";
        private static final String CRASH_CAPTURE_TIME_KEY = "crash-capture-time";
        private static final String CRASH_PACKAGE_NAME_KEY = "app-package-name";
        private static final String CRASH_VARIATIONS_KEY = "variations";
        private static final String CRASH_UPLOAD_ID_KEY = "crash-upload-id";
        private static final String CRASH_UPLOAD_TIME_KEY = "crash-upload-time";

        /**
         * Serialize {@code CrashInfo} object into a JSON object string.
         *
         * @return serialized string for the object.
         */
        public String serializeToJson() {
            try {
                JSONObject jsonObj = new JSONObject();
                if (localId != null) {
                    jsonObj.put(CRASH_LOCAL_ID_KEY, localId);
                }
                if (captureTime != -1) {
                    jsonObj.put(CRASH_CAPTURE_TIME_KEY, captureTime);
                }
                if (packageName != null) {
                    jsonObj.put(CRASH_PACKAGE_NAME_KEY, packageName);
                }
                if (variations != null && !variations.isEmpty()) {
                    jsonObj.put(CRASH_VARIATIONS_KEY, new JSONArray(variations));
                }
                if (uploadId != null) {
                    jsonObj.put(CRASH_UPLOAD_ID_KEY, uploadId);
                }
                if (uploadTime != -1) {
                    jsonObj.put(CRASH_UPLOAD_TIME_KEY, uploadTime);
                }
                return jsonObj.toString();
            } catch (JSONException e) {
                return null;
            }
        }

        /**
         * Load {@code CrashInfo} from a JSON string.
         *
         * @param jsonString JSON string to load {@code CrashInfo} from.
         * @return {@code CrashInfo} loaded from the serialized JSON object string.
         * @throws JSONException if it's a malformatted string.
         */
        public static CrashInfo readFromJsonString(String jsonString) throws JSONException {
            JSONObject jsonObj = new JSONObject(jsonString);
            CrashInfo crashInfo = new CrashInfo();

            if (jsonObj.has(CRASH_LOCAL_ID_KEY)) {
                crashInfo.localId = jsonObj.getString(CRASH_LOCAL_ID_KEY);
            }
            if (jsonObj.has(CRASH_CAPTURE_TIME_KEY)) {
                crashInfo.captureTime = jsonObj.getLong(CRASH_CAPTURE_TIME_KEY);
            }
            if (jsonObj.has(CRASH_PACKAGE_NAME_KEY)) {
                crashInfo.packageName = jsonObj.getString(CRASH_PACKAGE_NAME_KEY);
            }
            if (jsonObj.has(CRASH_VARIATIONS_KEY)) {
                JSONArray variationsJSONArr = jsonObj.getJSONArray(CRASH_VARIATIONS_KEY);
                if (variationsJSONArr != null) {
                    crashInfo.variations = new ArrayList<>();
                    for (int i = 0; i < variationsJSONArr.length(); i++) {
                        crashInfo.variations.add(variationsJSONArr.getString(i));
                    }
                }
            }
            if (jsonObj.has(CRASH_UPLOAD_ID_KEY)) {
                crashInfo.uploadId = jsonObj.getString(CRASH_UPLOAD_ID_KEY);
            }
            if (jsonObj.has(CRASH_UPLOAD_TIME_KEY)) {
                crashInfo.uploadTime = jsonObj.getLong(CRASH_UPLOAD_TIME_KEY);
            }

            return crashInfo;
        }
    }

    /**
     * Loads all crashes info from source.
     *
     * @return list of crashes info.
     */
    public abstract List<CrashInfo> loadCrashesInfo();
}
