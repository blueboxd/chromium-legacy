// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.aidl;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@IntDef({NewBrowserType.FOREGROUND_TAB, NewBrowserType.BACKGROUND_TAB, NewBrowserType.NEW_POPUP,
        NewBrowserType.NEW_WINDOW})
@Retention(RetentionPolicy.SOURCE)
public @interface NewBrowserType {
    int FOREGROUND_TAB = 0;
    int BACKGROUND_TAB = 1;
    int NEW_POPUP = 2;
    int NEW_WINDOW = 3;
}
