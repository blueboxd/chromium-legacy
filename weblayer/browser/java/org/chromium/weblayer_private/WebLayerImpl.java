// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.app.Application;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.res.Resources;
import android.os.IBinder;
import android.util.AndroidRuntimeException;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.PathUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.content_public.browser.ChildProcessCreationParams;
import org.chromium.content_public.browser.DeviceUtils;
import org.chromium.ui.base.ResourceBundle;
import org.chromium.weblayer_private.aidl.IProfile;
import org.chromium.weblayer_private.aidl.IWebLayer;

public final class WebLayerImpl extends IWebLayer.Stub {
    // TODO: should there be one tag for all this code?
    private static final String TAG = "WebLayer";
    private static final String PRIVATE_DATA_DIRECTORY_SUFFIX = "weblayer";
    // TODO: Configure this from the client.
    private static final String COMMAND_LINE_FILE = "/data/local/tmp/weblayer-command-line";

    public static IBinder create(Application application, Context implContext) {
        return new WebLayerImpl(application, implContext);
    }

    @Override
    public IProfile createProfile(String path) {
        return new ProfileImpl(path);
    }

    private WebLayerImpl(Application application, Context implContext) {
        ContextUtils.initApplicationContext(new ContextWrapper(application) {
            @Override
            public Resources getResources() {
                // Always use resources from the implementation APK.
                return implContext.getResources();
            }
        });
        ResourceBundle.setNoAvailableLocalePaks();
        PathUtils.setPrivateDataDirectorySuffix(PRIVATE_DATA_DIRECTORY_SUFFIX);

        ChildProcessCreationParams.set(implContext.getPackageName(), true /* isExternalService */,
                LibraryProcessType.PROCESS_CHILD, true /* bindToCaller */,
                false /* ignoreVisibilityForImportance */);

        if (!CommandLine.isInitialized()) {
            CommandLine.initFromFile(COMMAND_LINE_FILE);
        }

        DeviceUtils.addDeviceSpecificUserAgentSwitch();

        try {
            LibraryLoader.getInstance().ensureInitialized(LibraryProcessType.PROCESS_BROWSER);
        } catch (ProcessInitException e) {
            Log.e(TAG, "ContentView initialization failed.", e);
            throw new AndroidRuntimeException(e);
        }

        try {
            BrowserStartupController.get(LibraryProcessType.PROCESS_BROWSER)
                    .startBrowserProcessesSync(
                            /* singleProcess*/ false);
        } catch (ProcessInitException e) {
            Log.e(TAG, "Unable to load native library.", e);
            throw new AndroidRuntimeException(e);
        }
    }
}
