// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.paintpreview.browser.PaintPreviewBaseService;

/**
 * A simple implementation of {@link PaintPreviewBaseService} used in tests.
 */
@JNINamespace("paint_preview")
public class TestImplementerService extends PaintPreviewBaseService {
    public TestImplementerService(String storagePath) {
        super(TestImplementerServiceJni.get().getInstance(storagePath));
    }

    @NativeMethods
    interface Natives {
        long getInstance(String storagePath);
    }
}
