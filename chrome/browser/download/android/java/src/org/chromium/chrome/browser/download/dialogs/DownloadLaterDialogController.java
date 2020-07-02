// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.dialogs;

import org.chromium.chrome.browser.download.DownloadLaterPromptStatus;

/**
 * Receives events from download later dialog.
 */
public interface DownloadLaterDialogController {
    /**
     * Called when the selection changed in the download later dialog.
     * @param choice The selection of the download time in the download later dialog.
     * @param promptStatus The prompt status about the "don't show again" checkbox.
     */
    void onDownloadLaterDialogComplete(
            @DownloadLaterDialogChoice int choice, @DownloadLaterPromptStatus int promptStatus);

    /**
     * Called when the user cancels or dismisses the download location dialog.
     */
    void onDownloadLaterDialogCanceled();

    /**
     * Called when the user clicks the edit location text.
     */
    void onEditLocationClicked();
}
