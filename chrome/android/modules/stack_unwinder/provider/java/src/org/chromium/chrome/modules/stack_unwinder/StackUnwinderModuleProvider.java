// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.modules.stack_unwinder;

import org.chromium.base.annotations.CalledByNative;

/** Installs and loads the stack unwinder module. */
public class StackUnwinderModuleProvider {
    /** Returns true if the module is installed. */
    @CalledByNative
    public static boolean isModuleInstalled() {
        return StackUnwinderModule.isInstalled();
    }

    /**
     * Installs the module asynchronously.
     *
     * Can only be called if the module is not installed.
     */
    @CalledByNative
    public static void installModule() {
        StackUnwinderModule.install((boolean success) -> {});
    }
}
