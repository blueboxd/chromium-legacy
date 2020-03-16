// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import android.support.v4.util.AtomicFile;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.StreamUtil;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.tabmodel.TabbedModeTabPersistencePolicy;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.Locale;

import javax.crypto.BadPaddingException;
import javax.crypto.Cipher;
import javax.crypto.IllegalBlockSizeException;

/**
 * {@link PersistedTabDataStorage} which uses a file for the storage
 */
public class FilePersistedTabDataStorage implements PersistedTabDataStorage {
    private static final String TAG = "FilePTDS";

    @Override
    public void save(int tabId, boolean isEncrypted, String dataId, byte[] data) {
        // TODO(crbug.com/1059636) these should be queued and executed on a background thread
        // TODO(crbug.com/1059637) we should introduce a retry mechanisms
        // TODO(crbug.com/1059638) abstract out encrypt/decrypt
        if (isEncrypted) {
            Cipher cipher = CipherFactory.getInstance().getCipher(Cipher.ENCRYPT_MODE);
            try {
                data = cipher.doFinal(data);
            } catch (BadPaddingException | IllegalBlockSizeException e) {
                Log.e(TAG,
                        String.format(Locale.ENGLISH, "Problem encrypting data. Details: %s",
                                e.getMessage()));
                return;
            }
        }
        FileOutputStream outputStream = null;
        try {
            outputStream = new FileOutputStream(getFile(tabId, isEncrypted, dataId));
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
    public void restore(int tabId, boolean isEncrypted, String dataId, Callback<byte[]> callback) {
        File file = getFile(tabId, isEncrypted, dataId);
        try {
            AtomicFile atomicFile = new AtomicFile(file);
            byte[] res = atomicFile.readFully();
            if (isEncrypted) {
                Cipher cipher = CipherFactory.getInstance().getCipher(Cipher.DECRYPT_MODE);
                res = cipher.doFinal(res);
            }
            callback.onResult(res);
        } catch (FileNotFoundException e) {
            Log.e(TAG,
                    String.format(Locale.ENGLISH,
                            "FileNotFoundException while attempting to restore "
                                    + " for Tab %d. Details: %s",
                            tabId, e.getMessage()));
            callback.onResult(null);
        } catch (IOException e) {
            Log.e(TAG,
                    String.format(Locale.ENGLISH,
                            "IOException while attempting to restore for Tab "
                                    + "%d. Details: %s",
                            tabId, e.getMessage()));
            callback.onResult(null);
        } catch (BadPaddingException | IllegalBlockSizeException e) {
            Log.e(TAG,
                    String.format(
                            Locale.ENGLISH, "Error encrypting data. Details: %s", e.getMessage()));
            callback.onResult(null);
        }
    }

    @Override
    public void delete(int tabId, boolean isEncrypted, String dataId) {
        File file = getFile(tabId, isEncrypted, dataId);
        if (!file.exists()) {
            return;
        }
        boolean res = file.delete();
        if (!res) {
            Log.e(TAG, String.format(Locale.ENGLISH, "Error deleting file %s", file));
        }
    }

    @VisibleForTesting
    protected File getFile(int tabId, boolean isEncrypted, String dataId) {
        String encryptedId = isEncrypted ? "Encrypted" : "NotEncrypted";
        return new File(TabbedModeTabPersistencePolicy.getOrCreateTabbedModeStateDirectory(),
                String.format(Locale.ENGLISH, "%d%s%s", tabId, encryptedId, dataId));
    }
}
