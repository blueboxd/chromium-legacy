// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.shell;

import android.app.Application;
import android.content.Context;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.BuildConfig;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.PathUtils;
import org.chromium.base.multidex.ChromiumMultiDexInstaller;
import org.chromium.ui.base.ResourceBundle;

/**
 * Entry point for the demo shell application.
 */
public class WebLayerShellApplication extends Application {
    public static final String COMMAND_LINE_FILE = "/data/local/tmp/weblayer-shell-command-line";
    private static final String PRIVATE_DATA_DIRECTORY_SUFFIX = "weblayer_shell";

    @Override
    protected void attachBaseContext(Context base) {
        super.attachBaseContext(base);
        boolean isBrowserProcess = !ContextUtils.getProcessName().contains(":");
        ContextUtils.initApplicationContext(this);
        ResourceBundle.setNoAvailableLocalePaks();
        if (isBrowserProcess) {
            if (BuildConfig.IS_MULTIDEX_ENABLED) {
                ChromiumMultiDexInstaller.install(this);
            }
            PathUtils.setPrivateDataDirectorySuffix(PRIVATE_DATA_DIRECTORY_SUFFIX);
        }
        ApplicationStatus.initialize(this);
    }

    public void initCommandLine() {
        if (!CommandLine.isInitialized()) {
            CommandLine.initFromFile(COMMAND_LINE_FILE);
        }
    }
}
