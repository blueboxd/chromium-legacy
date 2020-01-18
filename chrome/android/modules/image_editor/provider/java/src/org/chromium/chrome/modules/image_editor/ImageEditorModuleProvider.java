// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.modules.image_editor;

import org.chromium.components.module_installer.engine.InstallListener;

/**
 * Installs and loads the module.
 */
public class ImageEditorModuleProvider {
    /**
     * Returns true if the module is installed.
     */
    public static boolean isModuleInstalled() {
        return ImageEditorModule.isInstalled();
    }

    /**
     * Installs the module.
     *
     * @param listener Called when the install has finished.
     */
    public static void installModule(InstallListener listener) {
        if (isModuleInstalled()) return;
        // TODO(crbug/1024586): Implement installation logic.
    }

    /**
     * Returns the image editor provider from inside the module.
     *
     * Can only be called if the module is installed. Maps native resources into memory on first
     * call.
     *
     * TODO(crbug/1024586): Return fallback editor?
     */
    public static ImageEditorProvider getImageEditorProvider() {
        return ImageEditorModule.getImpl();
    }
}
