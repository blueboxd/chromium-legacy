// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import androidx.annotation.MainThread;
import androidx.annotation.VisibleForTesting;
import androidx.core.util.AtomicFile;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.StreamUtil;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.tabmodel.TabbedModeTabPersistencePolicy;
import org.chromium.content_public.browser.UiThreadTaskTraits;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.LinkedList;
import java.util.Locale;
import java.util.Queue;

/**
 * {@link PersistedTabDataStorage} which uses a file for the storage
 */
public class FilePersistedTabDataStorage implements PersistedTabDataStorage {
    private static final String TAG = "FilePTDS";
    protected static final Callback<Integer> NO_OP_CALLBACK = new Callback<Integer>() {
        @Override
        public void onResult(Integer result) {}
    };
    private static final int DECREMENT_SEMAPHORE_VAL = 1;
    private Queue<StorageRequest> mQueue = new LinkedList<>();

    @MainThread
    @Override
    public void save(int tabId, String dataId, byte[] data) {
        save(tabId, dataId, data, NO_OP_CALLBACK);
    }

    // Callback used for test synchronization between save, restore and delete operations
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    protected void save(int tabId, String dataId, byte[] data, Callback<Integer> callback) {
        ThreadUtils.assertOnUiThread();
        // TODO(crbug.com/1059637) we should introduce a retry mechanisms
        // TODO(crbug.com/1068749) only execute the latest FileSaveRequest
        // for a tabId/dataId combination
        mQueue.add(new FileSaveRequest(tabId, dataId, data, callback));
        processNextItemOnQueue();
    }

    @MainThread
    @Override
    public void restore(int tabId, String dataId, Callback<byte[]> callback) {
        ThreadUtils.assertOnUiThread();
        mQueue.add(new FileRestoreRequest(tabId, dataId, callback));
        processNextItemOnQueue();
    }

    @MainThread
    @Override
    public void delete(int tabId, String dataId) {
        delete(tabId, dataId, NO_OP_CALLBACK);
    }

    // Callback used for test synchronization between save, restore and delete operations
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    protected void delete(int tabId, String dataId, Callback<Integer> callback) {
        ThreadUtils.assertOnUiThread();
        mQueue.add(new FileDeleteRequest(tabId, dataId, callback));
        processNextItemOnQueue();
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    protected File getFile(int tabId, String dataId) {
        return new File(TabbedModeTabPersistencePolicy.getOrCreateTabbedModeStateDirectory(),
                String.format(Locale.ENGLISH, "%d%s", tabId, dataId));
    }

    private enum RequestType { SAVE, DELETE, RESTORE }

    /**
     * Request for saving, restoring and deleting {@link PersistedTabData}
     */
    private abstract class StorageRequest {
        protected final int mTabId;
        protected final String mDataId;
        protected final RequestType mRequestType;
        protected final File mFile;

        /**
         * @param tabId identifier for the {@link Tab}
         * @param dataId identifier for the {@link PersistedTabData}
         * @param requestType - save, delete or restore
         */
        StorageRequest(int tabId, String dataId, RequestType requestType) {
            mTabId = tabId;
            mDataId = dataId;
            mRequestType = requestType;
            mFile = getFile(tabId, dataId);
        }

        /**
         * @return unique identifier for the StorageRequest
         */
        String getRequestId() {
            return String.format(Locale.ENGLISH, "%d_%s", mTabId, mDataId);
        }

        /**
         * AsyncTask to execute the StorageRequest
         */
        abstract AsyncTask getAsyncTask();
    }

    /**
     * Request to save {@link PersistedTabData}
     */
    private class FileSaveRequest extends StorageRequest {
        private byte[] mData;
        private Callback<Integer> mCallback;

        /**
         * @param tabId identifier for the {@link Tab}
         * @param dataId identifier for the {@link PersistedTabData}
         * @param data - data to be saved
         */
        FileSaveRequest(int tabId, String dataId, byte[] data, Callback<Integer> callback) {
            super(tabId, dataId, RequestType.SAVE);
            mData = data;
            mCallback = callback;
        }

        @Override
        public AsyncTask getAsyncTask() {
            return new AsyncTask<Void>() {
                @Override
                protected Void doInBackground() {
                    FileOutputStream outputStream = null;
                    try {
                        outputStream = new FileOutputStream(mFile);
                        outputStream.write(mData);
                    } catch (FileNotFoundException e) {
                        Log.e(TAG,
                                String.format(Locale.ENGLISH,
                                        "FileNotFoundException while attempting to save file %s "
                                                + "Details: %s",
                                        mFile, e.getMessage()));
                    } catch (IOException e) {
                        Log.e(TAG,
                                String.format(Locale.ENGLISH,
                                        "IOException while attempting to save for file %s. "
                                                + " Details: %s",
                                        mFile, e.getMessage()));
                    } finally {
                        StreamUtil.closeQuietly(outputStream);
                    }
                    return null;
                }

                @Override
                protected void onPostExecute(Void result) {
                    PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT,
                            () -> { mCallback.onResult(DECREMENT_SEMAPHORE_VAL); });
                    processNextItemOnQueue();
                }
            };
        }
    }

    /**
     * Request to delete a saved {@link PersistedTabData}
     */
    private class FileDeleteRequest extends StorageRequest {
        private byte[] mData;
        private Callback<Integer> mCallback;

        /**
         * @param tabId identifier for the {@link Tab}
         * @param dataId identifier for the {@link PersistedTabData}
         */
        FileDeleteRequest(int tabId, String dataId, Callback<Integer> callback) {
            super(tabId, dataId, RequestType.DELETE);
            mCallback = callback;
        }

        @Override
        public AsyncTask getAsyncTask() {
            return new AsyncTask<Void>() {
                @Override
                protected Void doInBackground() {
                    if (!mFile.exists()) {
                        return null;
                    }
                    boolean res = mFile.delete();
                    if (!res) {
                        Log.e(TAG, String.format(Locale.ENGLISH, "Error deleting file %s", mFile));
                    }
                    return null;
                }

                @Override
                protected void onPostExecute(Void result) {
                    PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT,
                            () -> { mCallback.onResult(DECREMENT_SEMAPHORE_VAL); });
                    processNextItemOnQueue();
                }
            };
        }
    }

    /**
     * Request to restore saved serialized {@link PersistedTabData}
     */
    private class FileRestoreRequest extends StorageRequest {
        private Callback<byte[]> mCallback;

        /**
         * @param tabId identifier for the {@link Tab}
         * @param dataId identifier for the {@link PersistedTabData}
         * @param callback - callback to return the retrieved serialized
         * {@link PersistedTabData} in
         */
        FileRestoreRequest(int tabId, String dataId, Callback<byte[]> callback) {
            super(tabId, dataId, RequestType.RESTORE);
            mCallback = callback;
        }

        @Override
        public AsyncTask getAsyncTask() {
            return new AsyncTask<byte[]>() {
                @Override
                protected byte[] doInBackground() {
                    try {
                        AtomicFile atomicFile = new AtomicFile(mFile);
                        return atomicFile.readFully();
                    } catch (FileNotFoundException e) {
                        Log.e(TAG,
                                String.format(Locale.ENGLISH,
                                        "FileNotFoundException while attempting to restore "
                                                + " %s. Details: %s",
                                        mFile, e.getMessage()));
                    } catch (IOException e) {
                        Log.e(TAG,
                                String.format(Locale.ENGLISH,
                                        "IOException while attempting to restore "
                                                + "%s. Details: %s",
                                        mFile, e.getMessage()));
                    }
                    return null;
                }

                @Override
                protected void onPostExecute(byte[] res) {
                    PostTask.runOrPostTask(
                            UiThreadTaskTraits.DEFAULT, () -> { mCallback.onResult(res); });
                    processNextItemOnQueue();
                }
            };
        }
    }

    private void processNextItemOnQueue() {
        if (mQueue.isEmpty()) return;
        mQueue.poll().getAsyncTask().executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }
}
