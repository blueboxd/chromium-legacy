// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.dialogs;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Contains all the properties for the download later dialog {@link PropertyModel}.
 */
public class DownloadLaterDialogProperties {
    /** The selection to define when to start the download. */
    public static final PropertyModel.WritableIntPropertyKey DOWNLOAD_TIME_SELECTION =
            new PropertyModel.WritableIntPropertyKey();

    public static final PropertyKey[] ALL_KEYS = new PropertyKey[] {DOWNLOAD_TIME_SELECTION};
}
