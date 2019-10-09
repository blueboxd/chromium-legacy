// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

/**
 * Temporary adapter while downstream depends on this package name.
 */
public class AwSafeBrowsingConfigHelper {
    // Deprecated: remove this once downstream no longer depends on AwSafeBrowsingConfigHelper in
    // this package name.
    public static Boolean getSafeBrowsingUserOptIn() {
        return org.chromium.android_webview.safe_browsing.AwSafeBrowsingConfigHelper
                .getSafeBrowsingUserOptIn();
    }

    // Not meant to be instantiated.
    private AwSafeBrowsingConfigHelper() {}
}
