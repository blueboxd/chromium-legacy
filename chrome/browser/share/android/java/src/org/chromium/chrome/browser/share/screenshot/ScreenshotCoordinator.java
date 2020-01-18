// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.screenshot;

import android.app.Activity;

import org.chromium.chrome.browser.image_editor.ImageEditorDialogCoordinator;
import org.chromium.chrome.modules.image_editor.ImageEditorModuleProvider;
import org.chromium.chrome.modules.image_editor.ImageEditorProvider;

/**
 * Handles the screenshot action in the Sharing Hub and launches the screenshot editor.
 */
public class ScreenshotCoordinator {
    private final Activity mActivity;
    private ImageEditorProvider mEditorProvider;

    public ScreenshotCoordinator(Activity activity) {
        mActivity = activity;
    }

    public void takeScreenshot() {
        // TODO(kristipark): Implement screenshot and installation logic.
        launchEditor();
    }

    private void launchEditor() {
        if (!ImageEditorModuleProvider.isModuleInstalled()) return;

        mEditorProvider = ImageEditorModuleProvider.getImageEditorProvider();
        ImageEditorDialogCoordinator editor = mEditorProvider.getImageEditorDialogCoordinator();
        editor.launchEditor(mActivity);
    }

    private void maybeInstallEditor() {}
}
