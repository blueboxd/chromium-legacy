// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import android.annotation.TargetApi;
import android.os.Build;
import android.view.accessibility.AccessibilityNodeInfo;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.content_public.browser.WebContents;

/**
 * Subclass of WebContentsAccessibility for P
 */
@JNINamespace("content")
@TargetApi(Build.VERSION_CODES.P)
public class PieWebContentsAccessibility extends OWebContentsAccessibility {
    PieWebContentsAccessibility(WebContents webContents) {
        super(webContents);
    }

    @Override
    protected void setAccessibilityNodeInfoPaneTitle(AccessibilityNodeInfo node, String title) {
        node.setPaneTitle(title);
    }
}
