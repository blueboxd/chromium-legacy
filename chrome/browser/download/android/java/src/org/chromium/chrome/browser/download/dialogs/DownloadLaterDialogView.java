// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.dialogs;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.LinearLayout;

import androidx.annotation.Nullable;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * The custom view in the download later dialog.
 */
public class DownloadLaterDialogView extends LinearLayout {
    /**
     * The view binder to propagate events from model to view.
     */
    public static class Binder {
        public static void bind(
                PropertyModel model, DownloadLaterDialogView view, PropertyKey propertyKey) {}
    }

    public DownloadLaterDialogView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }
}
