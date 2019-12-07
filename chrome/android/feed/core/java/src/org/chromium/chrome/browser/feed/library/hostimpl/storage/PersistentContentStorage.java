// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.hostimpl.storage;

import static org.chromium.chrome.browser.feed.library.api.host.storage.ContentOperation.Type.DELETE;
import static org.chromium.chrome.browser.feed.library.api.host.storage.ContentOperation.Type.DELETE_ALL;
import static org.chromium.chrome.browser.feed.library.api.host.storage.ContentOperation.Type.DELETE_BY_PREFIX;
import static org.chromium.chrome.browser.feed.library.api.host.storage.ContentOperation.Type.UPSERT;

import android.content.Context;

import org.chromium.base.Consumer;
import org.chromium.chrome.browser.feed.library.api.host.storage.CommitResult;
import org.chromium.chrome.browser.feed.library.api.host.storage.ContentMutation;
import org.chromium.chrome.browser.feed.library.api.host.storage.ContentOperation;
import org.chromium.chrome.browser.feed.library.api.host.storage.ContentOperation.Delete;
import org.chromium.chrome.browser.feed.library.api.host.storage.ContentOperation.DeleteByPrefix;
import org.chromium.chrome.browser.feed.library.api.host.storage.ContentOperation.Upsert;
import org.chromium.chrome.browser.feed.library.api.host.storage.ContentStorage;
import org.chromium.chrome.browser.feed.library.api.host.storage.ContentStorageDirect;
import org.chromium.chrome.browser.feed.library.api.internal.common.ThreadUtils;
import org.chromium.chrome.browser.feed.library.common.Result;
import org.chromium.chrome.browser.feed.library.common.logging.Logger;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.Executor;

/** Implementation of {@link ContentStorage} that persists data to disk. */
public final class PersistentContentStorage implements ContentStorage, ContentStorageDirect {
    private static final String TAG = "PersistentContent";
    private static final String CONTENT_DIR = "content";

    private final Context mContext;
    private final ThreadUtils mThreadUtils;
    private final Executor mExecutor;

    private File mContentDir;

    public PersistentContentStorage(Context context, Executor executor, ThreadUtils threadUtils) {
        this.mContext = context;
        this.mExecutor = executor;
        this.mThreadUtils = threadUtils;
    }

    @Override
    public void get(List<String> keys, Consumer<Result<Map<String, byte[]>>> consumer) {
        mThreadUtils.checkMainThread();
        mExecutor.execute(() -> consumer.accept(get(keys)));
    }

    @Override
    public Result<Map<String, byte[]>> get(List<String> keys) {
        initializeContentDir();

        Map<String, byte[]> valueMap = new HashMap<>(keys.size());
        for (String key : keys) {
            File keyFile = new File(mContentDir, key);
            if (keyFile.exists()) {
                try {
                    valueMap.put(key, getFileContents(keyFile));
                } catch (IOException e) {
                    Logger.e(TAG, e, "Error getting file contents");
                    return Result.failure();
                }
            }
        }
        return Result.success(valueMap);
    }

    @Override
    public void getAll(String prefix, Consumer<Result<Map<String, byte[]>>> consumer) {
        mThreadUtils.checkMainThread();
        mExecutor.execute(() -> consumer.accept(getAll(prefix)));
    }

    @Override
    public Result<Map<String, byte[]>> getAll(String prefix) {
        initializeContentDir();

        File[] files = mContentDir.listFiles();

        Map<String, byte[]> allFilesMap = new HashMap<>();

        try {
            if (files != null) {
                for (File file : files) {
                    if (file.getName().startsWith(prefix)) {
                        allFilesMap.put(file.getName(), getFileContents(file));
                    }
                }
            }

            return Result.success(allFilesMap);
        } catch (IOException e) {
            Logger.e(TAG, e, "Error getting file contents");
            return Result.failure();
        }
    }

    private byte[] getFileContents(File keyFile) throws IOException {
        mThreadUtils.checkNotMainThread();

        ByteArrayOutputStream outputStream = new ByteArrayOutputStream();

        if (keyFile.exists()) {
            try (FileInputStream fileInputStream = new FileInputStream(keyFile)) {
                while (fileInputStream.available() > 0) {
                    outputStream.write(fileInputStream.read());
                }
                return outputStream.toByteArray();
            } catch (IOException e) {
                Logger.e(TAG, "Error reading file", e);
                throw new IOException("Error reading file", e);
            }
        }
        return new byte[0];
    }

    @Override
    public void commit(ContentMutation mutation, Consumer<CommitResult> consumer) {
        mThreadUtils.checkMainThread();
        mExecutor.execute(() -> consumer.accept(commit(mutation)));
    }

    @Override
    public CommitResult commit(ContentMutation mutation) {
        initializeContentDir();

        for (ContentOperation operation : mutation.getOperations()) {
            if (operation.getType() == UPSERT) {
                if (!upsert((Upsert) operation)) {
                    return CommitResult.FAILURE;
                }
            } else if (operation.getType() == DELETE) {
                if (!delete((Delete) operation)) {
                    return CommitResult.FAILURE;
                }
            } else if (operation.getType() == DELETE_BY_PREFIX) {
                if (!deleteByPrefix((DeleteByPrefix) operation)) {
                    return CommitResult.FAILURE;
                }
            } else if (operation.getType() == DELETE_ALL) {
                if (!deleteAll()) {
                    return CommitResult.FAILURE;
                }
            } else {
                Logger.e(TAG, "Unrecognized ContentOperation type: %s", operation.getType());
            }
        }

        return CommitResult.SUCCESS;
    }

    @Override
    public void getAllKeys(Consumer<Result<List<String>>> consumer) {
        mThreadUtils.checkMainThread();
        consumer.accept(getAllKeys());
    }

    @Override
    public Result<List<String>> getAllKeys() {
        String[] filenames = mContentDir.list();
        if (filenames != null) {
            return Result.success(Arrays.asList(filenames));
        } else {
            return Result.failure();
        }
    }

    private boolean deleteAll() {
        mThreadUtils.checkNotMainThread();

        boolean success = true;

        File[] files = mContentDir.listFiles();
        if (files != null) {
            // Delete all files in the content directory
            for (File file : files) {
                if (!file.delete()) {
                    Logger.e(TAG, "Error deleting file when deleting all for file %s",
                            file.getName());
                    success = false;
                }
            }
        }
        return mContentDir.delete() && success;
    }

    private boolean deleteByPrefix(DeleteByPrefix operation) {
        mThreadUtils.checkNotMainThread();

        File[] files = mContentDir.listFiles();

        if (files != null) {
            for (File file : files) {
                if (file.getName().startsWith(operation.getPrefix())) {
                    // Try to delete
                    if (!file.delete()) {
                        Logger.e(TAG, "Error deleting by prefix %s", operation.getPrefix());
                        return false;
                    }
                }
            }
            return true;
        }

        return false;
    }

    private boolean delete(Delete operation) {
        mThreadUtils.checkNotMainThread();

        File file = new File(mContentDir, operation.getKey());
        boolean result = file.delete();
        if (!result) {
            Logger.e(TAG, "Error deleting key %s", operation.getKey());
        }
        return result;
    }

    private boolean upsert(Upsert operation) {
        mThreadUtils.checkNotMainThread();

        File file = new File(mContentDir, operation.getKey());
        try (FileOutputStream fos = new FileOutputStream(file)) {
            fos.write(operation.getValue());
            return true;
        } catch (IOException e) {
            Logger.e(TAG, "Error upserting key %s with value %s", operation.getKey(),
                    operation.getValue());
            return false;
        }
    }

    private void initializeContentDir() {
        mThreadUtils.checkNotMainThread();

        if (mContentDir == null) {
            mContentDir = mContext.getDir(CONTENT_DIR, Context.MODE_PRIVATE);
        }
        if (!mContentDir.exists()) {
            mContentDir.mkdir();
        }
    }
}
