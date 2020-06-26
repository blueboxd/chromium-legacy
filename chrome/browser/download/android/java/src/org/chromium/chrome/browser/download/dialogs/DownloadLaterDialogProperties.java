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
    public static final PropertyModel
            .ReadableObjectPropertyKey<DownloadLaterDialogView.Controller> CONTROLLER =
            new PropertyModel.ReadableObjectPropertyKey();

    /** The initial selection to define when to start the download. */
    public static final PropertyModel.ReadableIntPropertyKey DOWNLOAD_TIME_INITIAL_SELECTION =
            new PropertyModel.ReadableIntPropertyKey();

    /** The initial selection to define the don't show again checkbox. */
    public static final PropertyModel.ReadableIntPropertyKey DONT_SHOW_AGAIN_SELECTION =
            new PropertyModel.ReadableIntPropertyKey();

    /**
     * The string representing the download location. If null, no download location edit text will
     * be shown.
     */
    public static final PropertyModel.WritableObjectPropertyKey<String> LOCATION_TEXT =
            new PropertyModel.WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS = new PropertyKey[] {
            CONTROLLER, DOWNLOAD_TIME_INITIAL_SELECTION, DONT_SHOW_AGAIN_SELECTION, LOCATION_TEXT};
}
