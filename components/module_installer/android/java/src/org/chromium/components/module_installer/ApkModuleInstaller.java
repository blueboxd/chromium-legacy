// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.module_installer;

/** Module Installer for Apks. */
public class ApkModuleInstaller implements ModuleInstaller {
    /** A valid singleton instance is necessary for tests to swap it out. */
    private static ModuleInstaller sInstance = new ApkModuleInstaller();

    /** Returns the singleton instance. */
    public static ModuleInstaller getInstance() {
        return sInstance;
    }

    public static void setInstanceForTesting(ModuleInstaller moduleInstaller) {
        sInstance = moduleInstaller;
    }
}
