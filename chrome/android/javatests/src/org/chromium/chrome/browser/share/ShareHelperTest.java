// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import android.content.Context;
import android.net.Uri;
import android.support.test.filters.SmallTest;
import android.support.v4.content.FileProvider;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.ContentUriUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.ui.base.Clipboard;

import java.io.File;
import java.io.IOException;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * Tests of {@link ShareHelper}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ShareHelperTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private static final long WAIT_TIMEOUT_SECONDS = 5L;
    private static final byte[] TEST_IMAGE_DATA = new byte[] {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    private class FileProviderHelper implements ContentUriUtils.FileProviderUtil {
        private static final String API_AUTHORITY_SUFFIX = ".FileProvider";

        @Override
        public Uri getContentUriFromFile(File file) {
            Context appContext = ContextUtils.getApplicationContext();
            return FileProvider.getUriForFile(
                    appContext, appContext.getPackageName() + API_AUTHORITY_SUFFIX, file);
        }
    }

    private class GenerateUriCallback extends CallbackHelper implements Callback<Uri> {
        private Uri mImageUri;

        public Uri getImageUri() {
            return mImageUri;
        }

        @Override
        public void onResult(Uri uri) {
            mImageUri = uri;
            notifyCalled();
        }
    }

    private class AsyncTaskRunnableHelper extends CallbackHelper implements Runnable {
        @Override
        public void run() {
            notifyCalled();
        }
    }

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityFromLauncher();
        ContentUriUtils.setFileProviderUtil(new FileProviderHelper());
    }

    @After
    public void tearDown() throws TimeoutException {
        Clipboard.getInstance().setText("");
        clearSharedImages();
    }

    private int fileCount(File file) {
        if (file.isFile()) {
            return 1;
        }

        int count = 0;
        if (file.isDirectory()) {
            for (File f : file.listFiles()) count += fileCount(f);
        }
        return count;
    }

    private boolean filepathExists(File file, String filepath) {
        if (file.isFile() && filepath.endsWith(file.getName())) {
            return true;
        }

        if (file.isDirectory()) {
            for (File f : file.listFiles()) {
                if (filepathExists(f, filepath)) return true;
            }
        }
        return false;
    }

    private Uri generateAnImageToClipboard() throws TimeoutException {
        GenerateUriCallback imageCallback = new GenerateUriCallback();
        ShareHelper.generateUriFromData(
                mActivityTestRule.getActivity(), TEST_IMAGE_DATA, imageCallback);
        imageCallback.waitForCallback(0, 1, WAIT_TIMEOUT_SECONDS, TimeUnit.SECONDS);
        Clipboard.getInstance().setImage(imageCallback.getImageUri());
        return imageCallback.getImageUri();
    }

    private void clearSharedImages() throws TimeoutException {
        ShareHelper.clearSharedImages();

        // ShareHelper::clearSharedImages uses AsyncTask.SERIAL_EXECUTOR to schedule a clearing the
        // shared folder job, so schedule a new job and wait for the new job finished to make sure
        // ShareHelper::clearSharedImages's clearing folder job finished.
        AsyncTaskRunnableHelper runnableHelper = new AsyncTaskRunnableHelper();
        AsyncTask.SERIAL_EXECUTOR.execute(runnableHelper);
        runnableHelper.waitForCallback(0, 1, WAIT_TIMEOUT_SECONDS, TimeUnit.SECONDS);
    }

    private int fileCountInShareDirectory() throws IOException {
        return fileCount(ShareHelper.getSharedFilesDirectory());
    }

    private boolean fileExistsInShareDirectory(Uri fileUri) throws IOException {
        return filepathExists(ShareHelper.getSharedFilesDirectory(), fileUri.getPath());
    }

    @Test
    @SmallTest
    public void clipboardUriDoNotClearTest() throws TimeoutException, IOException {
        generateAnImageToClipboard();
        generateAnImageToClipboard();
        Uri clipboardUri = generateAnImageToClipboard();
        Assert.assertEquals(3, fileCountInShareDirectory());

        clearSharedImages();
        Assert.assertEquals(1, fileCountInShareDirectory());
        Assert.assertTrue(fileExistsInShareDirectory(clipboardUri));
    }

    @Test
    @SmallTest
    public void clearEverythingIfNoClipboardImageTest() throws TimeoutException, IOException {
        generateAnImageToClipboard();
        generateAnImageToClipboard();
        generateAnImageToClipboard();
        Assert.assertEquals(3, fileCountInShareDirectory());

        Clipboard.getInstance().setText("");
        clearSharedImages();
        Assert.assertEquals(0, fileCountInShareDirectory());
    }
}
