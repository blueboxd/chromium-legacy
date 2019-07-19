// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.module_installer;

import org.chromium.base.annotations.MainDex;

/** Dummy fallback of ModuleInstaller for APK builds. */
@MainDex
public class ModuleInstallerImpl implements ModuleInstaller {
    /** A valid singleton instance is necessary for tests to swap it out. */
    private static ModuleInstaller sInstance = new ModuleInstallerImpl();

    /** Returns the singleton instance. */
    public static ModuleInstaller getInstance() {
        return sInstance;
    }

    public static void setInstanceForTesting(ModuleInstaller moduleInstaller) {
        sInstance = moduleInstaller;
    }

    @Override
    public void install(String moduleName, OnModuleInstallFinishedListener onFinishedListener) {
        throw new UnsupportedOperationException("Cannot install module if APK");
    }

    @Override
    public void installDeferred(String moduleName) {
        throw new UnsupportedOperationException("Cannot deferred install module if APK");
    }

    private ModuleInstallerImpl() {}
}
