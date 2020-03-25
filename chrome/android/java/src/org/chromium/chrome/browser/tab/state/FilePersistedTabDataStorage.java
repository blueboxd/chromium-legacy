// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import android.support.v4.util.AtomicFile;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.StreamUtil;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.tabmodel.TabbedModeTabPersistencePolicy;
import org.chromium.content_public.browser.UiThreadTaskTraits;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.Locale;

/**
 * {@link PersistedTabDataStorage} which uses a file for the storage
 */
public class FilePersistedTabDataStorage implements PersistedTabDataStorage {
    private static final String TAG = "FilePTDS";

    @Override
    public void save(int tabId, String dataId, byte[] data) {
        // TODO(crbug.com/1059636) these should be queued and executed on a background thread
        // TODO(crbug.com/1059637) we should introduce a retry mechanisms
        FileOutputStream outputStream = null;
        try {
            outputStream = new FileOutputStream(getFile(tabId, dataId));
            outputStream.write(data);
        } catch (FileNotFoundException e) {
            Log.e(TAG,
                    String.format(Locale.ENGLISH,
                            "FileNotFoundException while attempting to save for Tab %d "
                                    + "Details: %s",
                            tabId, e.getMessage()));
        } catch (IOException e) {
            Log.e(TAG,
                    String.format(Locale.ENGLISH,
                            "IOException while attempting to save for Tab %d. "
                                    + " Details: %s",
                            tabId, e.getMessage()));
        } finally {
            StreamUtil.closeQuietly(outputStream);
        }
    }

    @Override
    public void restore(int tabId, String dataId, Callback<byte[]> callback) {
        File file = getFile(tabId, dataId);
        try {
            AtomicFile atomicFile = new AtomicFile(file);
            byte[] res = atomicFile.readFully();
            PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> { callback.onResult(res); });
        } catch (FileNotFoundException e) {
            Log.e(TAG,
                    String.format(Locale.ENGLISH,
                            "FileNotFoundException while attempting to restore "
                                    + " for Tab %d. Details: %s",
                            tabId, e.getMessage()));
            PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> { callback.onResult(null); });
        } catch (IOException e) {
            Log.e(TAG,
                    String.format(Locale.ENGLISH,
                            "IOException while attempting to restore for Tab "
                                    + "%d. Details: %s",
                            tabId, e.getMessage()));
            PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> { callback.onResult(null); });
        }
    }

    @Override
    public void delete(int tabId, String dataId) {
        File file = getFile(tabId, dataId);
        if (!file.exists()) {
            return;
        }
        boolean res = file.delete();
        if (!res) {
            Log.e(TAG, String.format(Locale.ENGLISH, "Error deleting file %s", file));
        }
    }

    @VisibleForTesting
    protected File getFile(int tabId, String dataId) {
        return new File(TabbedModeTabPersistencePolicy.getOrCreateTabbedModeStateDirectory(),
                String.format(Locale.ENGLISH, "%d%s", tabId, dataId));
    }
}
