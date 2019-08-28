// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.net.Uri;

import org.chromium.weblayer_private.BrowserControllerImpl;

public final class NavigationController {
    private BrowserControllerImpl mBrowserController;

    NavigationController(BrowserControllerImpl browserController) {
        mBrowserController = browserController;
    }

    public void navigate(Uri uri) {
        mBrowserController.navigate(uri.toString());
    }
}
