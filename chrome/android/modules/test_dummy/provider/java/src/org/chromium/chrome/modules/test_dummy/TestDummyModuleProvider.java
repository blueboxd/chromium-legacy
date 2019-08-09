// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.modules.test_dummy;

import android.app.Activity;
import android.content.Intent;

/** Installs the test dummy module and launches the test dummy implementation. */
public class TestDummyModuleProvider {
    /**
     * Launches test dummy. Installs test dummy module if necessary.
     *
     * @param intent A test dummy intent encoding the desired test scenario.
     * @param activity The activity the test dummy will run in.
     */
    public static void launchTestDummy(Intent intent, Activity activity) {
        if (!TestDummyModule.isInstalled()) {
            installAndLaunchTestDummy(intent, activity);
            return;
        }
        TestDummyModule.getImpl().getTestDummy().launch(intent, activity);
    }

    private static void installAndLaunchTestDummy(Intent intent, Activity activity) {
        TestDummyModule.install((success) -> {
            if (!success) {
                throw new RuntimeException("Failed to install module.");
            }
            TestDummyModule.getImpl().getTestDummy().launch(intent, activity);
        });
    }
}
