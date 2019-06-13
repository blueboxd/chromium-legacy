// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.view.View;

import java.lang.ref.WeakReference;

/**
 * A {@link TabGridViewHolder} with a close button. This is used in the Grid Tab Switcher.
 */
class ClosableTabGridViewHolder extends TabGridViewHolder {
    private static WeakReference<Bitmap> sCloseButtonBitmapWeakRef;

    ClosableTabGridViewHolder(View itemView) {
        super(itemView);
        if (sCloseButtonBitmapWeakRef == null || sCloseButtonBitmapWeakRef.get() == null) {
            int closeButtonSize = (int) itemView.getResources().getDimension(
                    org.chromium.chrome.tab_ui.R.dimen.tab_grid_close_button_size);
            Bitmap bitmap = BitmapFactory.decodeResource(
                    itemView.getResources(), org.chromium.chrome.tab_ui.R.drawable.btn_close);
            sCloseButtonBitmapWeakRef = new WeakReference<>(
                    Bitmap.createScaledBitmap(bitmap, closeButtonSize, closeButtonSize, true));
            bitmap.recycle();
        }

        actionButton.setImageBitmap(sCloseButtonBitmapWeakRef.get());
    }
}
