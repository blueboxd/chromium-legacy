// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import androidx.annotation.NonNull;

/**
 * Used for handling new browsers (such as occurs when window.open() is called). If this is not
 * set, popups are disabled.
 */
public abstract class NewBrowserCallback {
    public abstract void onNewBrowser(
            @NonNull BrowserController browserController, @NewBrowserType int type);
}
