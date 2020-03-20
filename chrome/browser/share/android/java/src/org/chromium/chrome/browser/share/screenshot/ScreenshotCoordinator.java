// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.screenshot;

import android.app.Activity;
import android.graphics.Bitmap;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.image_editor.ImageEditorDialogCoordinator;
import org.chromium.chrome.browser.modules.ModuleInstallUi;
import org.chromium.chrome.browser.screenshot.EditorScreenshotTask;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.modules.image_editor.ImageEditorModuleProvider;

/**
 * Handles the screenshot action in the Sharing Hub and launches the screenshot editor.
 */
public class ScreenshotCoordinator {
    // Maximum number of attempts to install the DFM per session.
    private static final int MAX_INSTALL_ATTEMPTS = 5;
    private static int sInstallAttempts;

    private final Activity mActivity;
    private final Tab mTab;

    private EditorScreenshotTask mScreenshotTask;
    private Bitmap mScreenshot;

    public ScreenshotCoordinator(Activity activity, Tab tab) {
        mActivity = activity;
        mTab = tab;
    }

    /**
     * Takes a screenshot of the current tab and attempts to launch the screenshot image editor.
     */
    public void captureScreenshot() {
        mScreenshotTask = new EditorScreenshotTask(mActivity);
        mScreenshotTask.capture(() -> {
            mScreenshot = mScreenshotTask.getScreenshot();
            if (mScreenshot == null) {
                // TODO(crbug/1024586): Show error message
            } else {
                handleScreenshot();
            }
        });
    }

    /**
     * Opens the editor with the captured screenshot if the editor is installed. Otherwise, attempts
     * to install the DFM a set amount of times per session. If MAX_INSTALL_ATTEMPTS is reached,
     * directly opens the screenshot sharesheet instead.
     */
    private void handleScreenshot() {
        if (ImageEditorModuleProvider.isModuleInstalled()) {
            launchEditor();
        } else if (sInstallAttempts < MAX_INSTALL_ATTEMPTS) {
            sInstallAttempts++;
            installEditor();
        } else {
            launchSharesheet();
        }
    }

    /**
     * Launches the screenshot image editor.
     */
    private void launchEditor() {
        ImageEditorDialogCoordinator editor = ImageEditorModuleProvider.getImageEditorProvider()
                                                      .getImageEditorDialogCoordinator();
        editor.launchEditor(mActivity, mScreenshot);
        mScreenshot = null;
    }

    /**
     * Opens the screenshot sharesheet.
     */
    private void launchSharesheet() {
        // TODO(crbug/1024586): Open screenshot sharesheet.
    }

    /**
     * Installs the DFM and shows UI (i.e. toasts and a retry dialog) informing the user of the
     * installation status.
     */
    private void installEditor() {
        ModuleInstallUi ui = new ModuleInstallUi(
                mTab, R.string.image_editor_module_title, new ModuleInstallUi.FailureUiListener() {
                    @Override
                    public void onFailureUiResponse(boolean retry) {
                        if (retry) {
                            // User initiated retries are not counted toward the maximum number
                            // of install attempts per session.
                            installEditor();
                        }
                    }
                });
        ui.showInstallStartUi();

        ImageEditorModuleProvider.maybeInstallModule((success) -> {
            if (success) {
                ui.showInstallSuccessUi();
                launchEditor();
            } else {
                ui.showInstallFailureUi();
            }
        });
    }
}
