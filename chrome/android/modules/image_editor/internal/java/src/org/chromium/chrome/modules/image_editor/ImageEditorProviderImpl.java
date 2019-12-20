// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.modules.image_editor;

import org.chromium.base.annotations.UsedByReflection;
import org.chromium.chrome.browser.image_editor.ImageEditorDialogCoordinator;
import org.chromium.chrome.browser.image_editor.ImageEditorDialogCoordinatorImpl;

/**
 * Upstream implementation for DFM module hook. Does nothing. Actual implementation lives
 * downstream.
 */
@UsedByReflection("ImageEditorModule")
public class ImageEditorProviderImpl implements ImageEditorProvider {
    @Override
    public ImageEditorDialogCoordinator getImageEditorDialogCoordinator() {
        return new ImageEditorDialogCoordinatorImpl();
    }
}
