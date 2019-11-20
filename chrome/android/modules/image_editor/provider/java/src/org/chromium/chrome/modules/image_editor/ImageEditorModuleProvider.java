// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.modules.image_editor;

import org.chromium.components.module_installer.engine.InstallListener;

/**
 * Installs and loads the module.
 * TODO(crbug.com/1024586): Add install logic and downstream compatibility.
 */
public class ImageEditorModuleProvider {
    public static boolean isModuleInstalled() {
        return false;
    }

    public static void installModule(InstallListener listener) {}

    public static ImageEditorProvider getImageEditorProvider() {
        return null;
    }
}
