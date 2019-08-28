// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.shell;

import android.app.Activity;
import android.net.Uri;
import android.os.Bundle;

import org.chromium.base.CommandLine;
import org.chromium.weblayer.BrowserController;
import org.chromium.weblayer.Profile;
import org.chromium.weblayer.WebLayer;

import java.io.File;

/**
 * Activity for managing the Demo Shell.
 */
public class WebLayerShellActivity extends Activity {
    private static final String TAG = "WebLayerShell";

    private WebLayer mWebLayer;
    private Profile mProfile;
    private BrowserController mBrowserController;

    @Override
    protected void onCreate(final Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // TODO: move this to WebLayer.
        // Initializing the command line must occur before loading the library.
        if (!CommandLine.isInitialized()) {
            ((WebLayerShellApplication) getApplication()).initCommandLine();
        }

        mProfile = WebLayer.getInstance().createProfile(new File(""));
        mBrowserController = new BrowserController(this, mProfile);
        mBrowserController.getNavigationController().navigate(Uri.parse("http://google.com"));
    }

    @Override
    protected void onStart() {
        super.onStart();
    }
}
