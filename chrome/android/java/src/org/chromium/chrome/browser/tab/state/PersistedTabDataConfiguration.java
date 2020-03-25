// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import java.util.HashMap;
import java.util.Map;

/**
 * Contains configuration values such as data storage methods and unique identifiers
 * for {@link PersistedTabData}
 */
public enum PersistedTabDataConfiguration {
    // TODO(crbug.com/1059650) investigate should this go in the app code?
    // Also investigate if the storage instance should be shared.
    CRITICAL_PERSISTED_TAB_DATA("CPTD", new FilePersistedTabDataStorage()),
    ENCRYPTED_CRITICAL_PERSISTED_TAB_DATA("ECPTD", new EncryptedFilePersistedTabDataStorage());

    private static final Map<Class<? extends PersistedTabData>, PersistedTabDataConfiguration>
            sLookup = new HashMap<>();
    private static final Map<Class<? extends PersistedTabData>, PersistedTabDataConfiguration>
            sEncryptedLookup = new HashMap<>();

    static {
        // TODO(crbug.com/1060187) remove static initializer and initialization lazy
        sLookup.put(CriticalPersistedTabData.class, CRITICAL_PERSISTED_TAB_DATA);
        sEncryptedLookup.put(CriticalPersistedTabData.class, ENCRYPTED_CRITICAL_PERSISTED_TAB_DATA);
    }

    public final String id;
    public final PersistedTabDataStorage storage;

    /**
     * @param id identifier for {@link PersistedTabData}
     * @param storage {@link PersistedTabDataStorage} associated with {@link PersistedTabData}
     */
    PersistedTabDataConfiguration(String id, PersistedTabDataStorage storage) {
        this.id = id;
        this.storage = storage;
    }

    /**
     * Acquire {@link PersistedTabDataConfiguration} for a given {@link PersistedTabData} class
     */
    public static PersistedTabDataConfiguration get(
            Class<? extends PersistedTabData> clazz, boolean isEncrypted) {
        if (isEncrypted) {
            return sEncryptedLookup.get(clazz);
        }
        return sLookup.get(clazz);
    }
}
