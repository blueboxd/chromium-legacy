// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.modules.image_editor;

import android.app.Activity;

import org.chromium.base.annotations.UsedByReflection;
import org.chromium.chrome.browser.image_editor.ImageEditorCoordinator;
import org.chromium.chrome.browser.image_editor.ImageEditorCoordinatorImpl;

/**
 * Upstream implementation for DFM module hook. Does nothing. Actual implementation lives
 * downstream.
 */
@UsedByReflection("ImageEditorModule")
public class ImageEditorProviderImpl implements ImageEditorProvider {
    @Override
    public ImageEditorCoordinator getImageEditorCoordinator(Activity activity) {
        return new ImageEditorCoordinatorImpl(activity);
    }
}
