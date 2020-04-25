// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.clipboard;

import android.content.Context;
import android.net.Uri;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.share.ShareImageFileUtils;
import org.chromium.ui.base.Clipboard;

/**
 * Implementation class for {@link Clipboard.ImageFileProvider}.
 */
public class ClipboardImageFileProvider implements Clipboard.ImageFileProvider {
    @Override
    public void storeImageAndGenerateUri(
            Context context, byte[] imageData, String fileExtension, Callback<Uri> callback) {
        ShareImageFileUtils.generateTemporaryUriFromData(
                context, imageData, fileExtension, callback);
    }
}
