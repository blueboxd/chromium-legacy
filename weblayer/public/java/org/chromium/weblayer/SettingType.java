// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * @hide
 */
@IntDef({SettingType.BASIC_SAFE_BROWSING_ENABLED})
@Retention(RetentionPolicy.SOURCE)
public @interface SettingType {
    /**
     * Allows the embedder to set whether it wants to disable/enable the Safe Browsing functionality
     * (which checks that the loaded URLs are safe). Safe Browsing is enabled by default.
     */
    int BASIC_SAFE_BROWSING_ENABLED =
            org.chromium.weblayer_private.interfaces.SettingType.BASIC_SAFE_BROWSING_ENABLED;
}
