// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import androidx.annotation.VisibleForTesting;

import com.google.protobuf.ByteString;
import com.google.protobuf.InvalidProtocolBufferException;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabAssociatedApp;
import org.chromium.chrome.browser.tab.TabImpl;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.TabThemeColorHelper;
import org.chromium.chrome.browser.tab.proto.CriticalPersistedTabData.CriticalPersistedTabDataProto;

import java.util.Locale;

/**
 * Data which is core to the app and must be retrieved as quickly as possible on startup.
 */
public class CriticalPersistedTabData extends PersistedTabData {
    private static final String TAG = "CriticalPTD";
    private int mParentId;
    private int mRootId;
    private long mTimestampMillis;
    private byte[] mContentStateBytes;
    private int mContentStateVersion;
    private String mOpenerAppId;
    private int mThemeColor;
    private @TabLaunchType Integer mTabLaunchTypeAtCreation;

    /**
     * @param tab {@link Tab} {@link CriticalPersistedTabData} is being stored for
     * @param parentId parent identiifer for the {@link Tab}
     * @param rootId root identifier for the {@link Tab}
     * @param timestampMillis creation timestamp for the {@link Tab}
     * @param contentStateBytes content state bytes for the {@link Tab}
     * @param contentStateVersion content state version for the {@link Tab}
     * @param openerAppId identifier for app opener
     * @param themeColor theme color
     * @param launchTypeAtCreation launch type at creation
     * @param persistedTabDataStorage storage for {@link PersistedTabData}
     * @param persistedTabDataId identifier for {@link PersistedTabData} in storage
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    CriticalPersistedTabData(Tab tab, int parentId, int rootId, long timestampMillis,
            byte[] contentStateBytes, int contentStateVersion, String openerAppId, int themeColor,
            int launchTypeAtCreation, PersistedTabDataStorage persistedTabDataStorage,
            String persistedTabDataId) {
        super(tab, persistedTabDataStorage, persistedTabDataId);
        mParentId = parentId;
        mRootId = rootId;
        mTimestampMillis = timestampMillis;
        mContentStateBytes = contentStateBytes;
        mContentStateVersion = contentStateVersion;
        mOpenerAppId = openerAppId;
        mThemeColor = themeColor;
        mTabLaunchTypeAtCreation = launchTypeAtCreation;
    }

    /**
     * @param tab {@link Tab} {@link CriticalPersistedTabData} is being stored for
     * @param data serialized {@link CriticalPersistedTabData}
     * @param storage {@link PersistedTabDataStorage} for {@link PersistedTabData}
     * @param persistedTabDataId unique identifier for {@link PersistedTabData} in
     * storage
     * This constructor is public because that is needed for the reflection
     * used in PersistedTabData.java
     */
    private CriticalPersistedTabData(
            Tab tab, byte[] data, PersistedTabDataStorage storage, String persistedTabDataId) {
        super(tab, data, storage, persistedTabDataId);
    }

    /**
     * Acquire {@link CriticalPersistedTabData} from storage
     * @param tab {@link Tab} {@link CriticalPersistedTabData} is being stored for.
     *        At a minimum this needs to be a frozen {@link Tab} with an identifier
     *        and isIncognito values set.
     * @param callback callback to pass {@link PersistedTabData} back in
     */
    public static void from(Tab tab, Callback<CriticalPersistedTabData> callback) {
        PersistedTabData.from(tab,
                (data, storage, id)
                        -> { return new CriticalPersistedTabData(tab, data, storage, id); },
                ()
                        -> { return CriticalPersistedTabData.build(tab); },
                CriticalPersistedTabData.class, callback);
    }

    private static CriticalPersistedTabData build(Tab tab) {
        TabImpl tabImpl = (TabImpl) tab;
        // TODO(crbug.com/1059634) this can be removed once we access all fields
        // from CriticalPersistedTabData
        // The following is only needed as an interim measure until all
        // fields are migrated to {@link CriticalPersistedTabData}. At that point
        // This function will only be used to acquire the {@link CriticalPersistedTabData}
        // from the {@link Tab}.
        if (tabImpl.isInitialized()) {
            TabState.WebContentsState webContentsState = TabState.getWebContentsState(tabImpl);
            PersistedTabDataConfiguration config = PersistedTabDataConfiguration.get(
                    CriticalPersistedTabData.class, tab.isIncognito());
            CriticalPersistedTabData criticalPersistedTabData = new CriticalPersistedTabData(tab,
                    tab.getParentId(), tabImpl.getRootId(), tab.getTimestampMillis(),
                    webContentsState != null
                            ? TabState.getContentStateByteArray(webContentsState.buffer())
                            : null,
                    TabState.CONTENTS_STATE_CURRENT_VERSION, TabAssociatedApp.getAppId(tab),
                    TabThemeColorHelper.isUsingColorFromTabContents(tab)
                            ? TabThemeColorHelper.getColor(tab)
                            : TabState.UNSPECIFIED_THEME_COLOR,
                    tab.getLaunchTypeAtInitialTabCreation(), config.storage, config.id);
            return criticalPersistedTabData;
        }
        return null;
    }

    @Override
    void deserialize(byte[] bytes) {
        try {
            CriticalPersistedTabDataProto criticalPersistedTabDataProto =
                    CriticalPersistedTabDataProto.parseFrom(bytes);
            mParentId = criticalPersistedTabDataProto.getParentId();
            mRootId = criticalPersistedTabDataProto.getRootId();
            mTimestampMillis = criticalPersistedTabDataProto.getTimestampMillis();
            mContentStateBytes = criticalPersistedTabDataProto.getContentStateBytes().toByteArray();
            mContentStateVersion = criticalPersistedTabDataProto.getContentStateVersion();
            mOpenerAppId = criticalPersistedTabDataProto.getOpenerAppId();
            mThemeColor = criticalPersistedTabDataProto.getThemeColor();
            mTabLaunchTypeAtCreation =
                    getLaunchType(criticalPersistedTabDataProto.getLaunchTypeAtCreation());
        } catch (InvalidProtocolBufferException e) {
            // TODO(crbug.com/1059635) add in metrics
            Log.e(TAG,
                    String.format(Locale.ENGLISH,
                            "There was a problem deserializing Tab %d. Details: %s", mTab.getId(),
                            e.getMessage()));
        }
    }

    private static @TabLaunchType int getLaunchType(
            CriticalPersistedTabDataProto.LaunchTypeAtCreation protoLaunchType) {
        switch (protoLaunchType) {
            case FROM_LINK:
                return TabLaunchType.FROM_LINK;
            case FROM_EXTERNAL_APP:
                return TabLaunchType.FROM_EXTERNAL_APP;
            case FROM_CHROME_UI:
                return TabLaunchType.FROM_CHROME_UI;
            case FROM_RESTORE:
                return TabLaunchType.FROM_RESTORE;
            case FROM_LONGPRESS_FOREGROUND:
                return TabLaunchType.FROM_LONGPRESS_FOREGROUND;
            case FROM_LONGPRESS_BACKGROUND:
                return TabLaunchType.FROM_LONGPRESS_BACKGROUND;
            case FROM_REPARENTING:
                return TabLaunchType.FROM_REPARENTING;
            case FROM_LAUNCHER_SHORTCUT:
                return TabLaunchType.FROM_LAUNCHER_SHORTCUT;
            case FROM_SPECULATIVE_BACKGROUND_CREATION:
                return TabLaunchType.FROM_SPECULATIVE_BACKGROUND_CREATION;
            case FROM_BROWSER_ACTIONS:
                return TabLaunchType.FROM_BROWSER_ACTIONS;
            case FROM_LAUNCH_NEW_INCOGNITO_TAB:
                return TabLaunchType.FROM_LAUNCH_NEW_INCOGNITO_TAB;
            case FROM_STARTUP:
                return TabLaunchType.FROM_STARTUP;
            case FROM_START_SURFACE:
                return TabLaunchType.FROM_START_SURFACE;
            case SIZE:
                return TabLaunchType.SIZE;
            default:
                assert false : "Unexpected deserialization of LaunchAtCreationType: "
                               + protoLaunchType;
                // shouldn't happen
                return -1;
        }
    }

    private static CriticalPersistedTabDataProto.LaunchTypeAtCreation getLaunchType(
            @TabLaunchType int protoLaunchType) {
        switch (protoLaunchType) {
            case TabLaunchType.FROM_LINK:
                return CriticalPersistedTabDataProto.LaunchTypeAtCreation.FROM_LINK;
            case TabLaunchType.FROM_EXTERNAL_APP:
                return CriticalPersistedTabDataProto.LaunchTypeAtCreation.FROM_EXTERNAL_APP;
            case TabLaunchType.FROM_CHROME_UI:
                return CriticalPersistedTabDataProto.LaunchTypeAtCreation.FROM_CHROME_UI;
            case TabLaunchType.FROM_RESTORE:
                return CriticalPersistedTabDataProto.LaunchTypeAtCreation.FROM_RESTORE;
            case TabLaunchType.FROM_LONGPRESS_FOREGROUND:
                return CriticalPersistedTabDataProto.LaunchTypeAtCreation.FROM_LONGPRESS_FOREGROUND;
            case TabLaunchType.FROM_LONGPRESS_BACKGROUND:
                return CriticalPersistedTabDataProto.LaunchTypeAtCreation.FROM_LONGPRESS_BACKGROUND;
            case TabLaunchType.FROM_REPARENTING:
                return CriticalPersistedTabDataProto.LaunchTypeAtCreation.FROM_REPARENTING;
            case TabLaunchType.FROM_LAUNCHER_SHORTCUT:
                return CriticalPersistedTabDataProto.LaunchTypeAtCreation.FROM_LAUNCHER_SHORTCUT;
            case TabLaunchType.FROM_SPECULATIVE_BACKGROUND_CREATION:
                return CriticalPersistedTabDataProto.LaunchTypeAtCreation
                        .FROM_SPECULATIVE_BACKGROUND_CREATION;
            case TabLaunchType.FROM_BROWSER_ACTIONS:
                return CriticalPersistedTabDataProto.LaunchTypeAtCreation.FROM_BROWSER_ACTIONS;
            case TabLaunchType.FROM_LAUNCH_NEW_INCOGNITO_TAB:
                return CriticalPersistedTabDataProto.LaunchTypeAtCreation
                        .FROM_LAUNCH_NEW_INCOGNITO_TAB;
            case TabLaunchType.FROM_STARTUP:
                return CriticalPersistedTabDataProto.LaunchTypeAtCreation.FROM_STARTUP;
            case TabLaunchType.FROM_START_SURFACE:
                return CriticalPersistedTabDataProto.LaunchTypeAtCreation.FROM_START_SURFACE;
            case TabLaunchType.SIZE:
                return CriticalPersistedTabDataProto.LaunchTypeAtCreation.SIZE;
            default:
                assert false : "Unexpected serialization of LaunchAtCreationType: "
                               + protoLaunchType;
                // shouldn't happen
                return CriticalPersistedTabDataProto.LaunchTypeAtCreation.UNKNOWN;
        }
    }

    @Override
    byte[] serialize() {
        return CriticalPersistedTabDataProto.newBuilder()
                .setParentId(mParentId)
                .setRootId(mRootId)
                .setTimestampMillis(mTimestampMillis)
                .setContentStateBytes(ByteString.copyFrom(mContentStateBytes))
                .setContentStateVersion(mContentStateVersion)
                .setOpenerAppId(mOpenerAppId)
                .setThemeColor(mThemeColor)
                .setLaunchTypeAtCreation(getLaunchType(mTabLaunchTypeAtCreation))
                .build()
                .toByteArray();
    }

    @Override
    public void destroy() {}

    /**
     * @return identifier for the {@link Tab}
     */
    public int getTabId() {
        return mTab.getId();
    }

    /**
     * @return root identifier for the {@link Tab}
     */
    public int getRootId() {
        return mRootId;
    }

    /**
     * Set root id
     */
    public void setRootId(int rootId) {
        // TODO(crbug.com/1059640) add in setters for all mutable fields
        mRootId = rootId;
        save();
    }

    /**
     * @return parent identifier for the {@link Tab}
     */
    public int getParentId() {
        return mParentId;
    }

    /**
     * @return timestamp in milliseconds for the {@link Tab}
     */
    public long getTimestampMillis() {
        return mTimestampMillis;
    }

    /**
     * @return content state bytes for the {@link Tab}
     */
    public byte[] getContentStateBytes() {
        return mContentStateBytes;
    }

    /**
     * @return content state version for the {@link Tab}
     */
    public int getContentStateVersion() {
        return mContentStateVersion;
    }

    /**
     * @return opener app id
     */
    public String getOpenerAppId() {
        return mOpenerAppId;
    }

    /**
     * @return theme color
     */
    public int getThemeColor() {
        return mThemeColor;
    }

    /**
     * @return launch type at creation
     */
    public @TabLaunchType int getTabLaunchTypeAtCreation() {
        return mTabLaunchTypeAtCreation;
    }
}
