// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@IntDef({SettingType.BASIC_SAFE_BROWSING_ENABLED})
@Retention(RetentionPolicy.SOURCE)
public @interface SettingType {
    int BASIC_SAFE_BROWSING_ENABLED = 0;
}
