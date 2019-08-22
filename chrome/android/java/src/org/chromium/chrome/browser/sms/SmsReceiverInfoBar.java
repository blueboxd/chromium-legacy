// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sms;

import android.content.Context;

import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ResourceId;
import org.chromium.chrome.browser.infobar.ConfirmInfoBar;
import org.chromium.chrome.browser.infobar.InfoBarControlLayout;
import org.chromium.chrome.browser.infobar.InfoBarLayout;

/**
 * An InfoBar that asks for the user's permission to share the SMS with the page.
 */
public class SmsReceiverInfoBar extends ConfirmInfoBar {
    private static final String TAG = "SmsReceiverInfoBar";
    private static final boolean DEBUG = false;
    private String mMessage;

    @VisibleForTesting
    @CalledByNative
    static SmsReceiverInfoBar create(
            int enumeratedIconId, String title, String message, String okButtonLabel) {
        if (DEBUG) Log.d(TAG, "SmsReceiverInfoBar.create()");
        return new SmsReceiverInfoBar(enumeratedIconId, title, message, okButtonLabel);
    }

    private SmsReceiverInfoBar(
            int enumeratedIconId, String title, String message, String okButtonLabel) {
        super(ResourceId.mapToDrawableId(enumeratedIconId), R.color.infobar_icon_drawable_color,
                /*iconBitmap=*/null, /*message=*/title, /*linkText=*/null, okButtonLabel,
                /*secondaryButtonText=*/null);
        mMessage = message;
    }

    @Override
    public void createContent(InfoBarLayout layout) {
        super.createContent(layout);
        Context context = layout.getContext();
        InfoBarControlLayout control = layout.addControlLayout();
        control.addDescription(mMessage);
    }
}
