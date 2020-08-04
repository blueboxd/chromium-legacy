// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.dialogs;

import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;

/**
 * Utility functions used in download dialogs.
 */
public class DownloadDialogUtils {
    /**
     * Returns a long value from property model, or a default value.
     * @param model The model that contains the data.
     * @param key The key of the data.
     * @param defaultValue The default value returned when the given property doesn't exist.
     */
    public static long getLong(
            PropertyModel model, ReadableObjectPropertyKey<Long> key, long defaultValue) {
        Long value = model.get(key);
        return (value != null) ? value : defaultValue;
    }

    private DownloadDialogUtils() {}
}
