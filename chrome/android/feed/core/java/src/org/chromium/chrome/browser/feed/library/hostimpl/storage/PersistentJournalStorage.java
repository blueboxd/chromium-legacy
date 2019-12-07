// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.hostimpl.storage;

import static org.chromium.chrome.browser.feed.library.api.host.storage.JournalOperation.Type.APPEND;
import static org.chromium.chrome.browser.feed.library.api.host.storage.JournalOperation.Type.COPY;
import static org.chromium.chrome.browser.feed.library.api.host.storage.JournalOperation.Type.DELETE;

import android.content.Context;
import android.support.annotation.VisibleForTesting;

import org.chromium.base.Consumer;
import org.chromium.chrome.browser.feed.library.api.host.storage.CommitResult;
import org.chromium.chrome.browser.feed.library.api.host.storage.JournalMutation;
import org.chromium.chrome.browser.feed.library.api.host.storage.JournalOperation;
import org.chromium.chrome.browser.feed.library.api.host.storage.JournalOperation.Append;
import org.chromium.chrome.browser.feed.library.api.host.storage.JournalOperation.Copy;
import org.chromium.chrome.browser.feed.library.api.host.storage.JournalStorage;
import org.chromium.chrome.browser.feed.library.api.host.storage.JournalStorageDirect;
import org.chromium.chrome.browser.feed.library.api.internal.common.ThreadUtils;
import org.chromium.chrome.browser.feed.library.common.Result;
import org.chromium.chrome.browser.feed.library.common.logging.Logger;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.UnsupportedEncodingException;
import java.net.URLDecoder;
import java.net.URLEncoder;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.Executor;

/**
 * Implementation of {@link JournalStorage} that persists data to disk.
 *
 * <p>Data is stored in files, with each append consisting of first the size of the bytes to write,
 * followed by the bytes themselves. For example, for a byte array of size 4, a total of 8 bytes
 * will be written: the 4 bytes representing the integer 4 (size), followed by the 4 bytes for the
 * append.
 */
public final class PersistentJournalStorage implements JournalStorage, JournalStorageDirect {
    private static final String TAG = "PersistentJournal";
    /** The schema version currently in use. */
    private static final int SCHEMA_VERSION = 1;

    private static final String SHARED_PREFERENCES = "JOURNAL_SP";
    private static final String SCHEMA_KEY = "JOURNAL_SCHEMA";

    // Optional Content directory - By default this is not used
    private static final String JOURNAL_DIR = "journal";
    private static final String ENCODING = "UTF-8";
    private static final int INTEGER_BYTE_SIZE = 4;
    private static final String ASTERISK = "_ATK_";
    private static final int MAX_BYTE_SIZE = 1000000;

    private final Context mContext;
    private final ThreadUtils mThreadUtils;
    private final Executor mExecutor;
    /*@Nullable*/ private final String mPersistenceDir;
    private File mJournalDir;

    /**
     * The schema of existing content. If this does not match {@code SCHEMA_VERSION}, all existing
     * content will be wiped so there are no version mismatches where data cannot be read / written
     * correctly.
     */
    private int mExistingSchema;

    public PersistentJournalStorage(Context context, Executor executorService,
            ThreadUtils threadUtils,
            /*@Nullable*/ String persistenceDir) {
        this.mContext = context;
        this.mExecutor = executorService;
        this.mThreadUtils = threadUtils;
        // TODO: See https://goto.google.com/tiktok-conformance-violations/SHARED_PREFS
        this.mExistingSchema =
                context.getSharedPreferences(SHARED_PREFERENCES, Context.MODE_PRIVATE)
                        .getInt(SCHEMA_KEY, 0);
        this.mPersistenceDir = persistenceDir;
    }

    @Override
    public void read(String journalName, Consumer<Result<List<byte[]>>> consumer) {
        mThreadUtils.checkMainThread();
        mExecutor.execute(() -> consumer.accept(read(journalName)));
    }

    @Override
    public Result<List<byte[]>> read(String journalName) {
        initializeJournalDir();

        String sanitizedJournalName = sanitize(journalName);
        if (!sanitizedJournalName.isEmpty()) {
            File journal = new File(mJournalDir, sanitizedJournalName);
            try {
                return Result.success(getJournalContents(journal));
            } catch (IOException e) {
                Logger.e(TAG, "Error occured reading journal %s", journalName);
                return Result.failure();
            }
        }
        return Result.failure();
    }

    private List<byte[]> getJournalContents(File journal) throws IOException {
        mThreadUtils.checkNotMainThread();

        List<byte[]> journalContents = new ArrayList<>();
        if (journal.exists()) {
            // Read byte size & bytes for each entry in the journal. See class comment for more info
            // on format.
            try (FileInputStream inputStream = new FileInputStream(journal)) {
                byte[] lengthBytes = new byte[INTEGER_BYTE_SIZE];
                while (inputStream.available() > 0) {
                    readBytes(inputStream, lengthBytes, INTEGER_BYTE_SIZE);
                    int size = ByteBuffer.wrap(lengthBytes).getInt();
                    if (size > MAX_BYTE_SIZE || size < 0) {
                        throw new IOException(
                                String.format(Locale.US, "Unexpected byte size %d", size));
                    }

                    byte[] contentBytes = new byte[size];
                    readBytes(inputStream, contentBytes, size);
                    journalContents.add(contentBytes);
                }
            } catch (IOException e) {
                Logger.e(TAG, "Error reading file", e);
                throw new IOException("Error reading journal file", e);
            }
        }
        return journalContents;
    }

    private void readBytes(FileInputStream inputStream, byte[] dest, int size) throws IOException {
        int bytesRead = inputStream.read(dest, 0, size);
        if (bytesRead != size) {
            throw new IOException(String.format(
                    Locale.US, "Expected to read %d bytes, but read %d bytes", size, bytesRead));
        }
    }

    @Override
    public void commit(JournalMutation mutation, Consumer<CommitResult> consumer) {
        mThreadUtils.checkMainThread();
        mExecutor.execute(() -> consumer.accept(commit(mutation)));
    }

    @Override
    public CommitResult commit(JournalMutation mutation) {
        initializeJournalDir();

        String sanitizedJournalName = sanitize(mutation.getJournalName());
        if (!sanitizedJournalName.isEmpty()) {
            File journal = new File(mJournalDir, sanitizedJournalName);

            for (JournalOperation operation : mutation.getOperations()) {
                if (operation.getType() == APPEND) {
                    if (!append((Append) operation, journal)) {
                        return CommitResult.FAILURE;
                    }
                } else if (operation.getType() == COPY) {
                    if (!copy((Copy) operation, journal)) {
                        return CommitResult.FAILURE;
                    }
                } else if (operation.getType() == DELETE) {
                    if (!delete(journal)) {
                        return CommitResult.FAILURE;
                    }
                } else {
                    Logger.e(TAG, "Unrecognized journal operation type %s", operation.getType());
                }
            }

            return CommitResult.SUCCESS;
        }
        return CommitResult.FAILURE;
    }

    @Override
    public void deleteAll(Consumer<CommitResult> consumer) {
        mThreadUtils.checkMainThread();

        mExecutor.execute(() -> consumer.accept(deleteAllInitialized()));
    }

    @Override
    public CommitResult deleteAll() {
        initializeJournalDir();
        return deleteAllInitialized();
    }

    private CommitResult deleteAllInitialized() {
        boolean success = true;

        File[] files = mJournalDir.listFiles();
        if (files != null) {
            // Delete all files in the journal directory
            for (File file : files) {
                if (!file.delete()) {
                    Logger.e(TAG, "Error deleting file when deleting all journals for file %s",
                            file.getName());
                    success = false;
                }
            }
        }
        success &= mJournalDir.delete();
        return success ? CommitResult.SUCCESS : CommitResult.FAILURE;
    }

    private boolean delete(File journal) {
        mThreadUtils.checkNotMainThread();

        if (!journal.exists()) {
            // If the file doesn't exist, let's call it deleted.
            return true;
        }
        boolean result = journal.delete();
        if (!result) {
            Logger.e(TAG, "Error deleting journal %s", journal.getName());
        }
        return result;
    }

    private boolean copy(Copy operation, File journal) {
        mThreadUtils.checkNotMainThread();

        try {
            if (!journal.exists()) {
                Logger.w(TAG, "Journal file %s does not exist, creating empty version",
                        journal.getName());
                if (!journal.createNewFile()) {
                    Logger.e(TAG, "Journal file %s exists while trying to create it",
                            journal.getName());
                }
            }
            String sanitizedDestJournalName = sanitize(operation.getToJournalName());
            if (!sanitizedDestJournalName.isEmpty()) {
                copyFile(journal, sanitizedDestJournalName);
                return true;
            }
        } catch (IOException e) {
            Logger.e(TAG, e, "Error copying journal %s to %s", journal.getName(),
                    operation.getToJournalName());
        }
        return false;
    }

    private void copyFile(File journal, String destinationFileName) throws IOException {
        InputStream inputStream = null;
        OutputStream outputStream = null;
        try {
            File destination = new File(mJournalDir, destinationFileName);
            inputStream = new FileInputStream(journal);
            outputStream = new FileOutputStream(destination);
            byte[] bytes = new byte[512];
            int length;
            while ((length = inputStream.read(bytes)) > 0) {
                outputStream.write(bytes, 0, length);
            }
        } finally {
            if (inputStream != null) {
                inputStream.close();
            }
            if (outputStream != null) {
                outputStream.close();
            }
        }
    }

    private boolean append(Append operation, File journal) {
        mThreadUtils.checkNotMainThread();

        if (!journal.exists()) {
            try {
                journal.createNewFile();
            } catch (IOException e) {
                Logger.e(TAG, "Could not create file to append to for journal %s.",
                        journal.getName());
                return false;
            }
        }

        byte[] journalBytes = operation.getValue();
        byte[] sizeBytes =
                ByteBuffer.allocate(INTEGER_BYTE_SIZE).putInt(journalBytes.length).array();
        return writeBytes(journal, sizeBytes, journalBytes);
    }

    /** Write byte size & bytes into the journal. See class comment for more info on format. */
    private boolean writeBytes(File journal, byte[] sizeBytes, byte[] journalBytes) {
        try (FileOutputStream out = new FileOutputStream(journal, /* append= */ true)) {
            out.write(sizeBytes);
            out.write(journalBytes);
            return true;
        } catch (IOException e) {
            Logger.e(TAG, "Error appending byte[] %s (size byte[] %s) for journal %s", journalBytes,
                    sizeBytes, journal.getName());
            return false;
        }
    }

    @Override
    public void exists(String journalName, Consumer<Result<Boolean>> consumer) {
        mThreadUtils.checkMainThread();
        mExecutor.execute(() -> consumer.accept(exists(journalName)));
    }

    @Override
    public Result<Boolean> exists(String journalName) {
        initializeJournalDir();

        String sanitizedJournalName = sanitize(journalName);
        if (!sanitizedJournalName.isEmpty()) {
            File journal = new File(mJournalDir, sanitizedJournalName);
            return Result.success(journal.exists());
        }
        return Result.failure();
    }

    @Override
    public void getAllJournals(Consumer<Result<List<String>>> consumer) {
        mThreadUtils.checkMainThread();
        mExecutor.execute(() -> consumer.accept(getAllJournals()));
    }

    @Override
    public Result<List<String>> getAllJournals() {
        initializeJournalDir();

        File[] files = mJournalDir.listFiles();
        List<String> journals = new ArrayList<>();
        if (files != null) {
            for (File file : files) {
                String desanitizedFileName = desanitize(file.getName());
                if (!desanitizedFileName.isEmpty()) {
                    journals.add(desanitizedFileName);
                }
            }
        }
        return Result.success(journals);
    }

    private void initializeJournalDir() {
        // if we've set the journalDir then just verify that it exists
        if (mJournalDir != null) {
            if (!mJournalDir.exists()) {
                if (!mJournalDir.mkdir()) {
                    Logger.w(TAG, "Jardin journal directory already exists");
                }
            }
            return;
        }

        // Create the root directory persistent files
        if (mPersistenceDir != null) {
            File persistenceRoot = mContext.getDir(mPersistenceDir, Context.MODE_PRIVATE);
            if (!persistenceRoot.exists()) {
                if (!persistenceRoot.mkdir()) {
                    Logger.w(TAG, "persistenceDir directory already exists");
                }
            }
            mJournalDir = new File(persistenceRoot, JOURNAL_DIR);
        } else {
            mJournalDir = mContext.getDir(JOURNAL_DIR, Context.MODE_PRIVATE);
        }
        if (mExistingSchema != SCHEMA_VERSION) {
            // For schema mismatch, delete everything.
            CommitResult result = deleteAllInitialized();
            if (result == CommitResult.SUCCESS
                    && mContext.getSharedPreferences(SHARED_PREFERENCES, Context.MODE_PRIVATE)
                               .edit()
                               .putInt(SCHEMA_KEY, SCHEMA_VERSION)
                               .commit()) {
                mExistingSchema = SCHEMA_VERSION;
            }
        }
        if (!mJournalDir.exists()) {
            if (!mJournalDir.mkdir()) {
                Logger.w(TAG, "journal directory already exists");
            }
        }
    }

    @VisibleForTesting
    String sanitize(String journalName) {
        try {
            // * is not replaced by URL encoder
            String sanitized = URLEncoder.encode(journalName, ENCODING);
            return sanitized.replace("*", ASTERISK);
        } catch (UnsupportedEncodingException e) {
            // Should not happen
            Logger.e(TAG, "Error sanitizing journal name %s", journalName);
            return "";
        }
    }

    @VisibleForTesting
    String desanitize(String sanitizedJournalName) {
        try {
            // * is not replaced by URL encoder
            String desanitized = URLDecoder.decode(sanitizedJournalName, ENCODING);
            return desanitized.replace(ASTERISK, "*");
        } catch (UnsupportedEncodingException e) {
            // Should not happen
            Logger.e(TAG, "Error desanitizing journal name %s", sanitizedJournalName);
            return "";
        }
    }
}
