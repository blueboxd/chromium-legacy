// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.shell;

import android.app.Activity;
import android.os.Bundle;

import org.chromium.base.CommandLine;
import org.chromium.base.Log;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.components.embedder_support.view.ContentViewRenderView;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.content_public.browser.DeviceUtils;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.weblayer.BrowserController;
import org.chromium.weblayer.Profile;

/**
 * Activity for managing the Demo Shell.
 */
public class WebLayerShellActivity extends Activity {
    private static final String TAG = "WebLayerShell";

    private ActivityWindowAndroid mWindowAndroid;
    private ContentViewRenderView mContentView;
    private Profile mProfile;
    private BrowserController mBrowserController;

    @Override
    protected void onCreate(final Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Initializing the command line must occur before loading the library.
        if (!CommandLine.isInitialized()) {
            ((WebLayerShellApplication) getApplication()).initCommandLine();
        }

        DeviceUtils.addDeviceSpecificUserAgentSwitch();

        try {
            LibraryLoader.getInstance().ensureInitialized(LibraryProcessType.PROCESS_BROWSER);
        } catch (ProcessInitException e) {
            Log.e(TAG, "ContentView initialization failed.", e);
            // Since the library failed to initialize nothing in the application
            // can work, so kill the whole application not just the activity
            System.exit(-1);
            return;
        }

        mWindowAndroid = new ActivityWindowAndroid(this);
        mContentView = new ContentViewRenderView(this);
        setContentView(mContentView);
        mWindowAndroid.setAnimationPlaceholderView(mContentView.getSurfaceView());
        try {
            BrowserStartupController.get(LibraryProcessType.PROCESS_BROWSER)
                    .startBrowserProcessesAsync(
                            true, false, new BrowserStartupController.StartupCallback() {
                                @Override
                                public void onSuccess() {
                                    mContentView.onNativeLibraryLoaded(mWindowAndroid);
                                    mProfile = Profile.create("default");
                                    mBrowserController = BrowserController.create(mProfile);
                                    mBrowserController.initialize(mContentView, mWindowAndroid);
                                    mContentView.setCurrentWebContents(
                                            mBrowserController.getWebContents());
                                    mBrowserController.show();
                                    mBrowserController.navigate("https://www.google.com");
                                }

                                @Override
                                public void onFailure() {
                                    Log.e(TAG, "ContentView initialization failed.");
                                    finish();
                                }
                            });
        } catch (ProcessInitException e) {
            Log.e(TAG, "Unable to load native library.", e);
            System.exit(-1);
        }
    }

    @Override
    protected void onStart() {
        super.onStart();
    }
}
