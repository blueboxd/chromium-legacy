// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_browsertests_apk;

import android.os.Bundle;
import android.view.Window;
import android.view.WindowManager;

import org.chromium.base.Log;
import org.chromium.base.StrictModeContext;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.init.BrowserParts;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.init.EmptyBrowserParts;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.native_test.NativeBrowserTestActivity;

import java.io.File;

/**
 * Android activity for running chrome browser tests.
 */
public class ChromeBrowserTestsActivity extends NativeBrowserTestActivity {
    private static final String TAG = "cr_browser_test";

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        appendCommandLineFlags(
                "--remote-debugging-socket-name android_browsertests_devtools_remote");
    }

    @Override
    protected void initializeBrowserProcess() {
        // TODO(ajwong): This is forked from ContentShellBrowserTestActivity. Should it be pulled
        // out into a static?
        try (StrictModeContext unused = StrictModeContext.allowDiskReads()) {
            LibraryLoader.getInstance().ensureInitialized(LibraryProcessType.PROCESS_BROWSER);
        } catch (ProcessInitException e) {
            Log.e(TAG, "Cannot load android_browsertests.", e);
            System.exit(-1);
        }

        // Don't use the production BrowserStartupController, as we want to replace it with
        // one that doesn't actually run ContentMain(). The BrowserTestBase class does the
        // work of ContentMain() itself once we pass control to C++ to run tests. That occurs
        // after this current method runs.
        // This replacement startup controller will be used when the C++ code calls into
        // ChromeBrowserTestsActivity.handlePostNativeStartup() to modify that behaviour.
        BrowserStartupController startupController = new BrowserStartupController() {
            @Override
            public void startBrowserProcessesAsync(boolean startGpuProcess,
                    boolean startServiceManagerOnly, final StartupCallback callback) {
                assert false; // Browser tests do a sync startup.
            }

            @Override
            public void startBrowserProcessesSync(boolean singleProcess) {
                ThreadUtils.assertOnUiThread();
                mStartupCompleted = true;
                // Runs the stuff that BrowserStartupController wants to do, without actually
                // running a chrome process.
                BrowserStartupController.get(LibraryProcessType.PROCESS_BROWSER)
                        .initChromiumBrowserProcessForTests();
            }

            @Override
            public boolean isStartupSuccessfullyCompleted() {
                ThreadUtils.assertOnUiThread();
                // Technically C++ code should call through and set this after starting the browser
                // process but it will have done so by the time it goes through
                // handlePostNativeStartupSynchronously() which will call
                // startBrowserProcessesSync() in this class.
                return mStartupCompleted;
            }

            @Override
            public boolean isServiceManagerSuccessfullyStarted() {
                ThreadUtils.assertOnUiThread();
                // Technically C++ code should call through and set this after starting the service
                // manager but it will have done so by the time it goes through
                // handlePostNativeStartupSynchronously() which will call
                // startBrowserProcessesSync() in this class.
                return mStartupCompleted;
            }

            @Override
            public void addStartupCompletedObserver(StartupCallback callback) {
                ThreadUtils.assertOnUiThread();
                // Pass the callback through to the "real" BrowserStartupController because many
                // pieces of code will do the same, and we want them all in one place. The
                // initChromiumBrowserProcessForTests() will run them all so that none are missed.
                BrowserStartupController.get(LibraryProcessType.PROCESS_BROWSER)
                        .addStartupCompletedObserver(callback);
            }

            @Override
            public void initChromiumBrowserProcessForTests() {
                assert false;
            }

            private boolean mStartupCompleted;
        };
        ChromeBrowserInitializer.setBrowserStartupControllerForTesting(startupController);

        // This does the pre-native stuff before handing control to C++ with runTests. Then
        // C++ will call back through handlePostNativeStartupSynchronously() to do the
        // post-native startup in ChromeBrowserInitializer.
        final BrowserParts parts = new EmptyBrowserParts() {};
        ChromeBrowserInitializer.getInstance().handlePreNativeStartup(parts);

        Window wind = this.getWindow();
        wind.addFlags(WindowManager.LayoutParams.FLAG_DISMISS_KEYGUARD);
        wind.addFlags(WindowManager.LayoutParams.FLAG_SHOW_WHEN_LOCKED);
        wind.addFlags(WindowManager.LayoutParams.FLAG_TURN_SCREEN_ON);
    }

    @Override
    protected File getPrivateDataDirectory() {
        // TODO(agrieve): We should not be touching the side-loaded test data directory.
        //     https://crbug.com/617734
        return new File(UrlUtils.getIsolatedTestRoot(),
                ChromeBrowserTestsApplication.PRIVATE_DATA_DIRECTORY_SUFFIX);
    }
}
