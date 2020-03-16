// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import org.chromium.base.Callback;

/**
 * Storage for {@link PersistedTabData}
 */
public interface PersistedTabDataStorage {
    /**
     * @param tabId identifier for the {@link Tab}
     * @param isEncrypted true if the data should be encrypted (i.e. for incognito Tabs)
     * @param tabDataId unique identifier representing the type {@link PersistedTabData}
     * @param data serialized {@link PersistedTabData}
     */
    void save(int tabId, boolean isEncrypted, String tabDataId, byte[] data);

    /**
     * @param tabId identifier for the {@link Tab}
     * @param isEncrypted true if the stored data is encrypted (i.e. for incognito Tabs)
     * @param tabDataId unique identifier representing the type of {@link PersistedTabData}
     * @param callback to pass back the seraizliaed {@link PersistedTabData} in
     */
    void restore(int tabId, boolean isEncrypted, String tabDataId, Callback<byte[]> callback);

    /**
     * @param tabId identifier for the {@link Tab}
     * @param isEncrypted true if the stored data is encrypted (i.e. for incognito Tabs)
     * @param tabDataId unique identifier representing the type of {@link PersistedTabData}
     */
    void delete(int tabId, boolean isEncrypted, String tabDataId);
}
