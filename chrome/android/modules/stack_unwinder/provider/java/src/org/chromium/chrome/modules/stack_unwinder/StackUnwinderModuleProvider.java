// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.modules.stack_unwinder;

import org.chromium.components.module_installer.engine.InstallListener;

/** Installs and loads the stack unwinder module. */
public class StackUnwinderModuleProvider {
    /** Returns true if the module is installed. */
    public static boolean isModuleInstalled() {
        return StackUnwinderModule.isInstalled();
    }

    /**
     * Installs the module.
     *
     * Can only be called if the module is not installed.
     *
     * @param listener Called when the install has finished.
     */
    public static void installModule(InstallListener listener) {
        StackUnwinderModule.install(listener);
    }

    /**
     * Returns the stack unwinder provider from inside the module.
     *
     * Can only be called if the module is installed. Maps native resources into memory on first
     * call.
     */
    public static StackUnwinderProvider getStackUnwinderProvider() {
        return StackUnwinderModule.getImpl();
    }
}
