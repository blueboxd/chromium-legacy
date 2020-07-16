// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.site_settings;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.site_settings.SingleWebsiteSettings;
import org.chromium.components.content_settings.ContentSettingsType;

/**
 * Tests the functionality of the SingleWebsiteSettings.java page.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class SingleWebsiteSettingsTest {
    private @ContentSettingsType int getCorrectContentSettingsTypeForPreferenceKey(
            String preferenceKey) {
        switch (preferenceKey) {
            case "ads_permission_list":
                return ContentSettingsType.ADS;
            case "automatic_downloads_permission_list":
                return ContentSettingsType.AUTOMATIC_DOWNLOADS;
            case "background_sync_permission_list":
                return ContentSettingsType.BACKGROUND_SYNC;
            case "bluetooth_scanning_permission_list":
                return ContentSettingsType.BLUETOOTH_SCANNING;
            case "cookies_permission_list":
                return ContentSettingsType.COOKIES;
            case "javascript_permission_list":
                return ContentSettingsType.JAVASCRIPT;
            case "popup_permission_list":
                return ContentSettingsType.POPUPS;
            case "sound_permission_list":
                return ContentSettingsType.SOUND;
            case "ar_permission_list":
                return ContentSettingsType.AR;
            case "camera_permission_list":
                return ContentSettingsType.MEDIASTREAM_CAMERA;
            case "clipboard_permission_list":
                return ContentSettingsType.CLIPBOARD_READ_WRITE;
            case "location_access_list":
                return ContentSettingsType.GEOLOCATION;
            case "microphone_permission_list":
                return ContentSettingsType.MEDIASTREAM_MIC;
            case "midi_sysex_permission_list":
                return ContentSettingsType.MIDI_SYSEX;
            case "nfc_permission_list":
                return ContentSettingsType.NFC;
            case "push_notifications_list":
                return ContentSettingsType.NOTIFICATIONS;
            case "protected_media_identifier_permission_list":
                return ContentSettingsType.PROTECTED_MEDIA_IDENTIFIER;
            case "sensors_permission_list":
                return ContentSettingsType.SENSORS;
            case "vr_permission_list":
                return ContentSettingsType.VR;
            default:
                Assert.fail("Preference key not in list.");
                return ContentSettingsType.DEFAULT;
        }
    }

    /**
     * Tests that the order of SingleWebsiteSettings.PERMISSION_PREFERENCE_KEYS matches the enums it
     * comes from.
     */
    @Test
    @SmallTest
    public void testCorrectMapOfPreferenceKeyToContentSettingsType() {
        for (String key : SingleWebsiteSettings.PERMISSION_PREFERENCE_KEYS) {
            Assert.assertEquals(SingleWebsiteSettings.getContentSettingsTypeFromPreferenceKey(key),
                    getCorrectContentSettingsTypeForPreferenceKey(key));
        }
    }
}