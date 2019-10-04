// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.modules.test_dummy;

import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.module_installer.engine.InstallListener;

/** Installs and loads the test dummy module. */
@JNINamespace("test_dummy")
public class TestDummyModuleProvider {
    private static boolean sIsModuleLoaded;

    /** Returns true if the module is installed. */
    public static boolean isModuleInstalled() {
        ThreadUtils.assertOnUiThread();
        return TestDummyModule.isInstalled();
    }

    /**
     * Installs the module.
     *
     * Can only be called if the module is not installed.
     *
     * @param onFinished Called when the install has finished.
     */
    public static void installModule(InstallListener onFinished) {
        ThreadUtils.assertOnUiThread();
        TestDummyModule.install(onFinished);
    }

    /**
     * Returns the test dummy provider from inside the module.
     *
     * Can only be called if the module is installed. Maps native resources into memory on first
     * call.
     */
    public static TestDummyProvider getTestDummyProvider() {
        ThreadUtils.assertOnUiThread();
        assert isModuleInstalled();
        if (!sIsModuleLoaded) {
            TestDummyModuleProviderJni.get().loadNative();
            sIsModuleLoaded = true;
        }
        return TestDummyModule.getImpl();
    }

    @NativeMethods
    interface Natives {
        void loadNative();
    }
}
