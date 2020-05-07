// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_info;

import androidx.annotation.Nullable;

import org.chromium.components.embedder_support.browser_context.BrowserContextHandle;
import org.chromium.components.embedder_support.util.Origin;

/**
 * Delegate class to allow customization of PermissionParamsListBuilderDelegate
 * logic.
 */
public class PermissionParamsListBuilderDelegate {
    public PermissionParamsListBuilderDelegate() {}

    /**
     * Returns the user visible name of the app that will handle permission delegation for the
     * origin.
     */
    @Nullable
    public String getDelegateAppName(Origin origin) {
        return null;
    }

    /**
     * Returns the BrowserContextHandle to be used by PermissionParamsListBuilder.
     */
    public BrowserContextHandle getBrowserContextHandle() {
        return null;
    }
}
