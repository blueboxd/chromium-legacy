// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_info;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.browserservices.permissiondelegation.TrustedWebActivityPermissionManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.components.page_info.PermissionParamsListBuilderDelegate;

/**
 * Chrome's customization of PermissionParamsListBuilderDelegate logic.
 */
public class ChromePermissionParamsListBuilderDelegate extends PermissionParamsListBuilderDelegate {
    private static Profile sProfileForTesting;

    public ChromePermissionParamsListBuilderDelegate() {
        super((sProfileForTesting != null) ? sProfileForTesting
                                           : Profile.getLastUsedRegularProfile());
    }

    @Override
    @Nullable
    public String getDelegateAppName(Origin origin) {
        assert origin != null;
        TrustedWebActivityPermissionManager manager = TrustedWebActivityPermissionManager.get();
        return manager.getDelegateAppName(origin);
    }

    @VisibleForTesting
    public static void setProfileForTesting(Profile profileForTesting) {
        sProfileForTesting = profileForTesting;
    }
}
