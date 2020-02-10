// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox;

import androidx.annotation.DrawableRes;

import org.chromium.components.security_state.ConnectionSecurityLevel;

public class SecurityStatusIcon {
    @DrawableRes
    public static int getSecurityIconResource(@ConnectionSecurityLevel int securityLevel,
            boolean shouldShowDangerTriangleForWarningLevel, boolean isSmallDevice,
            boolean skipIconForNeutralState) {
        switch (securityLevel) {
            case ConnectionSecurityLevel.NONE:
                if (isSmallDevice && skipIconForNeutralState) return 0;
                return R.drawable.omnibox_info;
            case ConnectionSecurityLevel.WARNING:
                if (shouldShowDangerTriangleForWarningLevel) {
                    return R.drawable.omnibox_not_secure_warning;
                }
                return R.drawable.omnibox_info;
            case ConnectionSecurityLevel.DANGEROUS:
                return R.drawable.omnibox_not_secure_warning;
            case ConnectionSecurityLevel.SECURE_WITH_POLICY_INSTALLED_CERT:
            case ConnectionSecurityLevel.SECURE:
            case ConnectionSecurityLevel.EV_SECURE:
                return R.drawable.omnibox_https_valid;
            default:
                assert false;
        }
        return 0;
    }
}
