// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings.website;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.settings.NfcSystemLevelSetting;

/**
 * A class for dealing with the NFC category.
 */
public class NfcCategory extends SiteSettingsCategory {
    public NfcCategory() {
        // As NFC is not a per-app permission, passing an empty string means the NFC permission is
        // always enabled for Chrome.
        super(Type.NFC, "" /* androidPermission*/);
    }

    @Override
    protected boolean enabledGlobally() {
        return NfcSystemLevelSetting.isNfcSystemLevelSettingEnabled();
    }

    @Override
    protected Intent getIntentToEnableOsGlobalPermission(Context context) {
        return NfcSystemLevelSetting.getNfcSystemLevelSettingIntent();
    }

    @Override
    protected String getMessageForEnablingOsGlobalPermission(Activity activity) {
        return activity.getResources().getString(R.string.android_nfc_off_globally);
    }
}
