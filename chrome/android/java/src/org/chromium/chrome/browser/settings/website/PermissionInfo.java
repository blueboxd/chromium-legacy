// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings.website;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.components.content_settings.ContentSettingsType;

import java.io.Serializable;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Permission information for a given origin.
 */
public class PermissionInfo implements Serializable {
    @IntDef({Type.AUGMENTED_REALITY, Type.CAMERA, Type.CLIPBOARD, Type.GEOLOCATION, Type.MICROPHONE,
            Type.MIDI, Type.NOTIFICATION, Type.PROTECTED_MEDIA_IDENTIFIER, Type.SENSORS,
            Type.VIRTUAL_REALITY})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Type {
        // Values used to address index - should be enumerated from 0 and can't have gaps.
        // All updates here must also be reflected in {@link #getContentSettingsType(int)
        // getContentSettingsType} and {@link SingleWebsiteSettings.PERMISSION_PREFERENCE_KEYS}.
        int AUGMENTED_REALITY = 0;
        int CAMERA = 1;
        int CLIPBOARD = 2;
        int GEOLOCATION = 3;
        int MICROPHONE = 4;
        int MIDI = 5;
        int NFC = 6;
        int NOTIFICATION = 7;
        int PROTECTED_MEDIA_IDENTIFIER = 8;
        int SENSORS = 9;
        int VIRTUAL_REALITY = 10;
        /**
         * Number of handled permissions used for example inside for loops.
         */
        int NUM_ENTRIES = 11;
    }

    private final boolean mIsIncognito;
    private final String mEmbedder;
    private final String mOrigin;
    private final @Type int mType;

    public PermissionInfo(@Type int type, String origin, String embedder, boolean isIncognito) {
        mOrigin = origin;
        mEmbedder = embedder;
        mIsIncognito = isIncognito;
        mType = type;
    }

    public @Type int getType() {
        return mType;
    }

    public String getOrigin() {
        return mOrigin;
    }

    public String getEmbedder() {
        return mEmbedder;
    }

    public boolean isIncognito() {
        return mIsIncognito;
    }

    public String getEmbedderSafe() {
        return mEmbedder != null ? mEmbedder : mOrigin;
    }

    /**
     * Returns the ContentSetting value for this origin.
     */
    public @ContentSettingValues @Nullable Integer getContentSetting() {
        switch (mType) {
            case Type.AUGMENTED_REALITY:
                return WebsitePreferenceBridgeJni.get().getArSettingForOrigin(
                        mOrigin, getEmbedderSafe(), mIsIncognito);
            case Type.CAMERA:
                return WebsitePreferenceBridgeJni.get().getCameraSettingForOrigin(
                        mOrigin, getEmbedderSafe(), mIsIncognito);
            case Type.CLIPBOARD:
                return WebsitePreferenceBridgeJni.get().getClipboardSettingForOrigin(
                        mOrigin, mIsIncognito);
            case Type.GEOLOCATION:
                return WebsitePreferenceBridgeJni.get().getGeolocationSettingForOrigin(
                        mOrigin, getEmbedderSafe(), mIsIncognito);
            case Type.MICROPHONE:
                return WebsitePreferenceBridgeJni.get().getMicrophoneSettingForOrigin(
                        mOrigin, getEmbedderSafe(), mIsIncognito);
            case Type.MIDI:
                return WebsitePreferenceBridgeJni.get().getMidiSettingForOrigin(
                        mOrigin, getEmbedderSafe(), mIsIncognito);
            case Type.NFC:
                return WebsitePreferenceBridgeJni.get().getNfcSettingForOrigin(
                        mOrigin, getEmbedderSafe(), mIsIncognito);
            case Type.NOTIFICATION:
                return WebsitePreferenceBridgeJni.get().getNotificationSettingForOrigin(
                        mOrigin, mIsIncognito);
            case Type.PROTECTED_MEDIA_IDENTIFIER:
                return WebsitePreferenceBridgeJni.get().getProtectedMediaIdentifierSettingForOrigin(
                        mOrigin, getEmbedderSafe(), mIsIncognito);
            case Type.SENSORS:
                return WebsitePreferenceBridgeJni.get().getSensorsSettingForOrigin(
                        mOrigin, getEmbedderSafe(), mIsIncognito);
            case Type.VIRTUAL_REALITY:
                return WebsitePreferenceBridgeJni.get().getVrSettingForOrigin(
                        mOrigin, getEmbedderSafe(), mIsIncognito);
            default:
                assert false;
                return null;
        }
    }

    /**
     * Sets the native ContentSetting value for this origin.
     */
    public void setContentSetting(@ContentSettingValues int value) {
        switch (mType) {
            case Type.AUGMENTED_REALITY:
                WebsitePreferenceBridgeJni.get().setArSettingForOrigin(
                        mOrigin, getEmbedderSafe(), value, mIsIncognito);
                break;
            case Type.CAMERA:
                WebsitePreferenceBridgeJni.get().setCameraSettingForOrigin(
                        mOrigin, value, mIsIncognito);
                break;
            case Type.CLIPBOARD:
                WebsitePreferenceBridgeJni.get().setClipboardSettingForOrigin(
                        mOrigin, value, mIsIncognito);
                break;
            case Type.GEOLOCATION:
                WebsitePreferenceBridgeJni.get().setGeolocationSettingForOrigin(
                        mOrigin, getEmbedderSafe(), value, mIsIncognito);
                break;
            case Type.MICROPHONE:
                WebsitePreferenceBridgeJni.get().setMicrophoneSettingForOrigin(
                        mOrigin, value, mIsIncognito);
                break;
            case Type.MIDI:
                WebsitePreferenceBridgeJni.get().setMidiSettingForOrigin(
                        mOrigin, getEmbedderSafe(), value, mIsIncognito);
                break;
            case Type.NFC:
                WebsitePreferenceBridgeJni.get().setNfcSettingForOrigin(
                        mOrigin, getEmbedderSafe(), value, mIsIncognito);
                break;
            case Type.NOTIFICATION:
                WebsitePreferenceBridgeJni.get().setNotificationSettingForOrigin(
                        mOrigin, value, mIsIncognito);
                break;
            case Type.PROTECTED_MEDIA_IDENTIFIER:
                WebsitePreferenceBridgeJni.get().setProtectedMediaIdentifierSettingForOrigin(
                        mOrigin, getEmbedderSafe(), value, mIsIncognito);
                break;
            case Type.SENSORS:
                WebsitePreferenceBridgeJni.get().setSensorsSettingForOrigin(
                        mOrigin, getEmbedderSafe(), value, mIsIncognito);
                break;
            case Type.VIRTUAL_REALITY:
                WebsitePreferenceBridgeJni.get().setVrSettingForOrigin(
                        mOrigin, getEmbedderSafe(), value, mIsIncognito);
                break;
            default:
                assert false;
        }
    }

    public static @ContentSettingsType int getContentSettingsType(@Type int type) {
        switch (type) {
            case Type.AUGMENTED_REALITY:
                return ContentSettingsType.AR;
            case Type.CAMERA:
                return ContentSettingsType.MEDIASTREAM_CAMERA;
            case Type.CLIPBOARD:
                return ContentSettingsType.CLIPBOARD_READ_WRITE;
            case Type.GEOLOCATION:
                return ContentSettingsType.GEOLOCATION;
            case Type.MICROPHONE:
                return ContentSettingsType.MEDIASTREAM_MIC;
            case Type.MIDI:
                return ContentSettingsType.MIDI_SYSEX;
            case Type.NFC:
                return ContentSettingsType.NFC;
            case Type.NOTIFICATION:
                return ContentSettingsType.NOTIFICATIONS;
            case Type.PROTECTED_MEDIA_IDENTIFIER:
                return ContentSettingsType.PROTECTED_MEDIA_IDENTIFIER;
            case Type.SENSORS:
                return ContentSettingsType.SENSORS;
            case Type.VIRTUAL_REALITY:
                return ContentSettingsType.VR;
            default:
                assert false;
                return ContentSettingsType.DEFAULT;
        }
    }
}
