// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.UserData;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.tab.Tab;

/**
 * PersistedTabData is Tab data persisted across restarts
 * A constructor of taking a Tab and a byte[] (serialized
 * {@link PersistedTabData}, PersistedTabDataStorage and
 * PersistedTabDataID (identifier for {@link PersistedTabData}
 * in storage) is required as reflection is used to build
 * the object after acquiring the serialized object from storage.
 */
public abstract class PersistedTabData implements UserData {
    private static final String TAG = "PTD";
    protected final Tab mTab;
    private final PersistedTabDataStorage mPersistedTabDataStorage;
    private final String mPersistedTabDataId;

    /**
     * @param tab {@link Tab} {@link PersistedTabData} is being stored for
     * @param data serialized {@link Tab} metadata
     * @param persistedTabDataStorage storage for {@link PersistedTabData}
     * @param persistedTabDataId identifier for {@link PersistedTabData} in storage
     */
    PersistedTabData(Tab tab, byte[] data, PersistedTabDataStorage persistedTabDataStorage,
            String persistedTabDataId) {
        this(tab, persistedTabDataStorage, persistedTabDataId);
        deserialize(data);
    }

    /**
     * @param tab {@link Tab} {@link PersistedTabData} is being stored for
     * @param persistedTabDataStorage storage for {@link PersistedTabData}
     * @param persistedTabDataId identifier for {@link PersistedTabData} in storage
     */
    PersistedTabData(
            Tab tab, PersistedTabDataStorage persistedTabDataStorage, String persistedTabDataId) {
        mTab = tab;
        mPersistedTabDataStorage = persistedTabDataStorage;
        mPersistedTabDataId = persistedTabDataId;
    }

    /**
     * Asynchronously acquire a {@link PersistedTabData}
     * for a {@link Tab}
     * @param tab {@link Tab} {@link PersistedTabData} is being acquired for.
     * At a minimum, a frozen tab with an identifier and isIncognito fields set
     * is required.
     * @param factory {@link PersistedTabDataFactory} which will create {@link PersistedTabData}
     * @param supplier for constructing a {@link PersistedTabData} from a
     * {@link Tab}. This will be used as a fallback in the event that the {@link PersistedTabData}
     * cannot be found in storage.
     * @param clazz class of the {@link PersistedTabData}
     * @param callback callback to pass the {@link PersistedTabData} in
     * @return {@link PersistedTabData} from storage
     */
    protected static <T extends PersistedTabData> void from(Tab tab,
            PersistedTabDataFactory<T> factory, Supplier<T> supplier, Class<T> clazz,
            Callback<T> callback) {
        // TODO(crbug.com/1059602) cache callbacks
        T persistedTabDataFromTab = getUserData(tab, clazz);
        if (persistedTabDataFromTab != null) {
            // TODO(crbug.com/1060181) post the task
            callback.onResult(persistedTabDataFromTab);
            return;
        }
        PersistedTabDataConfiguration config =
                PersistedTabDataConfiguration.get(clazz, tab.isIncognito());
        config.storage.restore(tab.getId(), config.id, (data) -> {
            T persistedTabData;
            if (data == null) {
                persistedTabData = supplier.get();
            } else {
                persistedTabData = factory.create(data, config.storage, config.id);
            }
            if (persistedTabData != null) {
                setUserData(tab, clazz, persistedTabData);
            }
            callback.onResult(persistedTabData);
        });
    }

    /**
     * Acquire {@link PersistedTabData} stored in {@link UserData} on a {@link Tab}
     * @param tab the {@link Tab}
     * @param clazz {@link PersistedTabData} class
     */
    private static <T extends PersistedTabData> T getUserData(Tab tab, Class<T> clazz) {
        return clazz.cast(tab.getUserDataHost().getUserData(clazz));
    }

    /**
     * Set {@link PersistedTabData} on a {@link Tab} object using {@link UserDataHost}
     * @param tab the {@link Tab}
     * @param clazz {@link PersistedTabData} class - they key for {@link UserDataHost}
     * @param persistedTabData {@link PersistedTabData} stored on the {@link UserDataHost}
     * associated with the {@link Tab}
     */
    private static <T extends PersistedTabData> T setUserData(
            Tab tab, Class<T> clazz, T persistedTabData) {
        return tab.getUserDataHost().setUserData(clazz, persistedTabData);
    }

    /**
     * Save {@link PersistedTabData} to storage
     * @param callback callback indicating success/failure
     */
    @VisibleForTesting
    protected void save() {
        mPersistedTabDataStorage.save(mTab.getId(), mPersistedTabDataId, serialize());
    }

    /**
     * @return {@link PersistedTabData} in serialized form.
     */
    abstract byte[] serialize();

    /**
     * Deserialize serialized {@link PersistedTabData} and
     * assign to fields in {@link PersistedTabData}
     * @param bytes serialized PersistedTabData
     */
    abstract void deserialize(@Nullable byte[] bytes);

    /**
     * Delete {@link PersistedTabData} for a tab id
     * @param tabId tab identifier
     * @param profile profile
     */
    protected void delete() {
        mPersistedTabDataStorage.delete(mTab.getId(), mPersistedTabDataId);
    }

    /**
     * Destroy the object. This will clean up any {@link PersistedTabData}
     * in memory. It will not delete the stored data on a file or database.
     */
    @Override
    public abstract void destroy();
}
