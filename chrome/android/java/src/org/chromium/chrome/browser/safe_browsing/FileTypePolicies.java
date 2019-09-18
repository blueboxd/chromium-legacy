// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safe_browsing;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/**
 * This class reports UMA values based on files' extensions.
 */
@JNINamespace("safe_browsing")
public final class FileTypePolicies {
    /**
     * @param path The file path.
     * @return The UMA value for the file.
     */
    public static int umaValueForFile(String path) {
        return FileTypePoliciesJni.get().umaValueForFile(path);
    }

    @NativeMethods
    interface Natives {
        int umaValueForFile(String path);
    }
}
