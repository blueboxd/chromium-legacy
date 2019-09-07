// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.website;

import org.chromium.base.Callback;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.ContentSettingsType;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;

/**
 * Utility class that interacts with native to retrieve and set website settings.
 */
public class WebsitePreferenceBridge {
    /**
     * Interface for an object that listens to storage info is cleared callback.
     */
    public interface StorageInfoClearedCallback {
        @CalledByNative("StorageInfoClearedCallback")
        public void onStorageInfoCleared();
    }

    /**
     * @return the list of all origins that have permissions in non-incognito mode.
     */
    @SuppressWarnings("unchecked")
    public List<PermissionInfo> getPermissionInfo(@PermissionInfo.Type int type) {
        ArrayList<PermissionInfo> list = new ArrayList<PermissionInfo>();
        // Camera, Location & Microphone can be managed by the custodian
        // of a supervised account or by enterprise policy.
        if (type == PermissionInfo.Type.CAMERA) {
            boolean managedOnly = !PrefServiceBridge.getInstance().isCameraUserModifiable();
            WebsitePreferenceBridgeJni.get().getCameraOrigins(list, managedOnly);
        } else if (type == PermissionInfo.Type.CLIPBOARD) {
            WebsitePreferenceBridgeJni.get().getClipboardOrigins(list);
        } else if (type == PermissionInfo.Type.GEOLOCATION) {
            boolean managedOnly = !PrefServiceBridge.getInstance().isAllowLocationUserModifiable();
            WebsitePreferenceBridgeJni.get().getGeolocationOrigins(list, managedOnly);
        } else if (type == PermissionInfo.Type.MICROPHONE) {
            boolean managedOnly = !PrefServiceBridge.getInstance().isMicUserModifiable();
            WebsitePreferenceBridgeJni.get().getMicrophoneOrigins(list, managedOnly);
        } else if (type == PermissionInfo.Type.MIDI) {
            WebsitePreferenceBridgeJni.get().getMidiOrigins(list);
        } else if (type == PermissionInfo.Type.NOTIFICATION) {
            WebsitePreferenceBridgeJni.get().getNotificationOrigins(list);
        } else if (type == PermissionInfo.Type.PROTECTED_MEDIA_IDENTIFIER) {
            WebsitePreferenceBridgeJni.get().getProtectedMediaIdentifierOrigins(list);
        } else if (type == PermissionInfo.Type.SENSORS) {
            WebsitePreferenceBridgeJni.get().getSensorsOrigins(list);
        } else {
            assert false;
        }
        return list;
    }

    private static void insertInfoIntoList(@PermissionInfo.Type int type,
            ArrayList<PermissionInfo> list, String origin, String embedder) {
        if (type == PermissionInfo.Type.CAMERA || type == PermissionInfo.Type.MICROPHONE) {
            for (PermissionInfo info : list) {
                if (info.getOrigin().equals(origin) && info.getEmbedder().equals(embedder)) {
                    return;
                }
            }
        }
        list.add(new PermissionInfo(type, origin, embedder, false));
    }

    @CalledByNative
    private static void insertCameraInfoIntoList(
            ArrayList<PermissionInfo> list, String origin, String embedder) {
        insertInfoIntoList(PermissionInfo.Type.CAMERA, list, origin, embedder);
    }

    @CalledByNative
    private static void insertClipboardInfoIntoList(
            ArrayList<PermissionInfo> list, String origin, String embedder) {
        insertInfoIntoList(PermissionInfo.Type.CLIPBOARD, list, origin, embedder);
    }

    @CalledByNative
    private static void insertGeolocationInfoIntoList(
            ArrayList<PermissionInfo> list, String origin, String embedder) {
        insertInfoIntoList(PermissionInfo.Type.GEOLOCATION, list, origin, embedder);
    }

    @CalledByNative
    private static void insertMicrophoneInfoIntoList(
            ArrayList<PermissionInfo> list, String origin, String embedder) {
        insertInfoIntoList(PermissionInfo.Type.MICROPHONE, list, origin, embedder);
    }

    @CalledByNative
    private static void insertMidiInfoIntoList(
            ArrayList<PermissionInfo> list, String origin, String embedder) {
        insertInfoIntoList(PermissionInfo.Type.MIDI, list, origin, embedder);
    }

    @CalledByNative
    private static void insertNotificationIntoList(
            ArrayList<PermissionInfo> list, String origin, String embedder) {
        insertInfoIntoList(PermissionInfo.Type.NOTIFICATION, list, origin, embedder);
    }

    @CalledByNative
    private static void insertProtectedMediaIdentifierInfoIntoList(
            ArrayList<PermissionInfo> list, String origin, String embedder) {
        insertInfoIntoList(PermissionInfo.Type.PROTECTED_MEDIA_IDENTIFIER, list, origin, embedder);
    }

    @CalledByNative
    private static void insertSensorsInfoIntoList(
            ArrayList<PermissionInfo> list, String origin, String embedder) {
        insertInfoIntoList(PermissionInfo.Type.SENSORS, list, origin, embedder);
    }

    @CalledByNative
    private static void insertStorageInfoIntoList(
            ArrayList<StorageInfo> list, String host, int type, long size) {
        list.add(new StorageInfo(host, type, size));
    }

    @CalledByNative
    private static Object createStorageInfoList() {
        return new ArrayList<StorageInfo>();
    }

    @CalledByNative
    private static Object createLocalStorageInfoMap() {
        return new HashMap<String, LocalStorageInfo>();
    }

    @SuppressWarnings("unchecked")
    @CalledByNative
    private static void insertLocalStorageInfoIntoMap(
            HashMap map, String origin, long size, boolean important) {
        ((HashMap<String, LocalStorageInfo>) map)
                .put(origin, new LocalStorageInfo(origin, size, important));
    }

    public List<ContentSettingException> getContentSettingsExceptions(
            @ContentSettingsType int contentSettingsType) {
        List<ContentSettingException> exceptions =
                PrefServiceBridge.getInstance().getContentSettingsExceptions(
                        contentSettingsType);
        if (!PrefServiceBridge.getInstance().isContentSettingManaged(contentSettingsType)) {
            return exceptions;
        }

        List<ContentSettingException> managedExceptions =
                new ArrayList<ContentSettingException>();
        for (ContentSettingException exception : exceptions) {
            if (exception.getSource().equals("policy")) {
                managedExceptions.add(exception);
            }
        }
        return managedExceptions;
    }

    public void fetchLocalStorageInfo(Callback<HashMap> callback, boolean fetchImportant) {
        WebsitePreferenceBridgeJni.get().fetchLocalStorageInfo(callback, fetchImportant);
    }

    public void fetchStorageInfo(Callback<ArrayList> callback) {
        WebsitePreferenceBridgeJni.get().fetchStorageInfo(callback);
    }

    /**
     * Returns the list of all chosen object permissions for the given ContentSettingsType.
     *
     * There will be one ChosenObjectInfo instance for each granted permission. That means that if
     * two origin/embedder pairs have permission for the same object there will be two
     * ChosenObjectInfo instances.
     */
    public List<ChosenObjectInfo> getChosenObjectInfo(
            @ContentSettingsType int contentSettingsType) {
        ArrayList<ChosenObjectInfo> list = new ArrayList<ChosenObjectInfo>();
        WebsitePreferenceBridgeJni.get().getChosenObjects(contentSettingsType, list);
        return list;
    }

    /**
     * Inserts a ChosenObjectInfo into a list.
     */
    @CalledByNative
    private static void insertChosenObjectInfoIntoList(ArrayList<ChosenObjectInfo> list,
            int contentSettingsType, String origin, String embedder, String name, String object,
            boolean isManaged) {
        list.add(new ChosenObjectInfo(
                contentSettingsType, origin, embedder, name, object, isManaged));
    }

    /**
     * Returns whether the DSE (Default Search Engine) controls the given permission the given
     * origin.
     */
    public static boolean isPermissionControlledByDSE(
            @ContentSettingsType int contentSettingsType, String origin, boolean isIncognito) {
        return WebsitePreferenceBridgeJni.get().isPermissionControlledByDSE(
                contentSettingsType, origin, isIncognito);
    }

    /**
     * Returns whether this origin is activated for ad blocking, and will have resources blocked
     * unless they are explicitly allowed via a permission.
     */
    public static boolean getAdBlockingActivated(String origin) {
        return WebsitePreferenceBridgeJni.get().getAdBlockingActivated(origin);
    }

    @VisibleForTesting
    @NativeMethods
    public interface Natives {
        void getCameraOrigins(Object list, boolean managedOnly);
        void getClipboardOrigins(Object list);
        void getGeolocationOrigins(Object list, boolean managedOnly);
        void getMicrophoneOrigins(Object list, boolean managedOnly);
        void getMidiOrigins(Object list);
        void getNotificationOrigins(Object list);
        void getProtectedMediaIdentifierOrigins(Object list);
        void getSensorsOrigins(Object list);
        int getCameraSettingForOrigin(String origin, String embedder, boolean isIncognito);
        int getClipboardSettingForOrigin(String origin, boolean isIncognito);
        int getGeolocationSettingForOrigin(String origin, String embedder, boolean isIncognito);
        int getMicrophoneSettingForOrigin(String origin, String embedder, boolean isIncognito);
        int getMidiSettingForOrigin(String origin, String embedder, boolean isIncognito);
        int getNotificationSettingForOrigin(String origin, boolean isIncognito);
        int getProtectedMediaIdentifierSettingForOrigin(
                String origin, String embedder, boolean isIncognito);
        int getSensorsSettingForOrigin(String origin, String embedder, boolean isIncognito);
        void setCameraSettingForOrigin(String origin, int value, boolean isIncognito);
        void setClipboardSettingForOrigin(String origin, int value, boolean isIncognito);
        void setGeolocationSettingForOrigin(
                String origin, String embedder, int value, boolean isIncognito);
        void setMicrophoneSettingForOrigin(String origin, int value, boolean isIncognito);
        void setMidiSettingForOrigin(
                String origin, String embedder, int value, boolean isIncognito);
        void setNotificationSettingForOrigin(String origin, int value, boolean isIncognito);
        void reportNotificationRevokedForOrigin(
                String origin, int newSettingValue, boolean isIncognito);
        void setProtectedMediaIdentifierSettingForOrigin(
                String origin, String embedder, int value, boolean isIncognito);
        void setSensorsSettingForOrigin(
                String origin, String embedder, int value, boolean isIncognito);
        void clearBannerData(String origin);
        void clearMediaLicenses(String origin);
        void clearCookieData(String path);
        void clearLocalStorageData(String path, Object callback);
        void clearStorageData(String origin, int type, Object callback);
        void getChosenObjects(@ContentSettingsType int type, Object list);
        void resetNotificationsSettingsForTest();
        void revokeObjectPermission(
                @ContentSettingsType int type, String origin, String embedder, String object);
        boolean isContentSettingsPatternValid(String pattern);
        boolean urlMatchesContentSettingsPattern(String url, String pattern);
        void fetchStorageInfo(Object callback);
        void fetchLocalStorageInfo(Object callback, boolean includeImportant);
        boolean isPermissionControlledByDSE(
                @ContentSettingsType int contentSettingsType, String origin, boolean isIncognito);
        boolean getAdBlockingActivated(String origin);
    }
}
