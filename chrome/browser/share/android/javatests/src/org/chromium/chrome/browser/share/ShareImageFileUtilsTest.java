// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.support.test.filters.SmallTest;

import androidx.core.content.FileProvider;

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
import org.chromium.base.test.util.DisableIf;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.ui.base.Clipboard;

import java.io.File;
import java.io.IOException;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * Tests of {@link ShareImageFileUtils}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ShareImageFileUtilsTest {
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
        clearExternalStorageDir();
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
        ShareImageFileUtils.generateTemporaryUriFromData(
                mActivityTestRule.getActivity(), TEST_IMAGE_DATA, imageCallback);
        imageCallback.waitForCallback(0, 1, WAIT_TIMEOUT_SECONDS, TimeUnit.SECONDS);
        Clipboard.getInstance().setImageUri(imageCallback.getImageUri());
        return imageCallback.getImageUri();
    }

    private void clearSharedImages() throws TimeoutException {
        ShareImageFileUtils.clearSharedImages();

        // ShareImageFileUtils::clearSharedImages uses AsyncTask.SERIAL_EXECUTOR to schedule a
        // clearing the shared folder job, so schedule a new job and wait for the new job finished
        // to make sure ShareImageFileUtils::clearSharedImages's clearing folder job finished.
        AsyncTaskRunnableHelper runnableHelper = new AsyncTaskRunnableHelper();
        AsyncTask.SERIAL_EXECUTOR.execute(runnableHelper);
        runnableHelper.waitForCallback(0, 1, WAIT_TIMEOUT_SECONDS, TimeUnit.SECONDS);
    }

    public void clearExternalStorageDir() {
        AsyncTask.SERIAL_EXECUTOR.execute(() -> {
            File externalStorageDir = mActivityTestRule.getActivity().getExternalFilesDir(
                    Environment.DIRECTORY_DOWNLOADS);
            String[] children = externalStorageDir.list();
            for (int i = 0; i < children.length; i++) {
                new File(externalStorageDir, children[i]).delete();
            }
        });
    }

    private int fileCountInShareDirectory() throws IOException {
        return fileCount(ShareImageFileUtils.getSharedFilesDirectory());
    }

    private boolean fileExistsInShareDirectory(Uri fileUri) throws IOException {
        return filepathExists(ShareImageFileUtils.getSharedFilesDirectory(), fileUri.getPath());
    }

    private Bitmap getTestBitmap() {
        int size = 10;
        Bitmap bitmap = Bitmap.createBitmap(size, size, Bitmap.Config.ARGB_8888);

        Canvas canvas = new Canvas(bitmap);
        Paint paint = new Paint();
        paint.setColor(android.graphics.Color.GREEN);
        canvas.drawRect(0F, 0F, (float) size, (float) size, paint);
        return bitmap;
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

    @Test
    @SmallTest
    @DisableIf.Build(sdk_is_less_than = Build.VERSION_CODES.LOLLIPOP, message = "crbug.com/1056059")
    public void testSaveBitmap() throws IOException {
        String fileName = "chrome-test-bitmap";
        ShareImageFileUtils.OnImageSaveListener listener =
                new ShareImageFileUtils.OnImageSaveListener() {
                    @Override
                    public void onImageSaved(File imageFile) {
                        AsyncTask.SERIAL_EXECUTOR.execute(() -> {
                            Assert.assertTrue(imageFile.exists());
                            Assert.assertTrue(imageFile.isFile());
                            Assert.assertTrue(imageFile.getPath().contains(fileName));
                        });
                    }

                    @Override
                    public void onImageSaveError() {
                        Assert.fail();
                    }
                };
        ShareImageFileUtils.saveBitmapToExternalStorage(
                mActivityTestRule.getActivity(), fileName, getTestBitmap(), listener);
    }

    @Test
    @SmallTest
    public void testGetNextAvailableFile() throws IOException {
        String filename = "chrome-test-bitmap";
        String extension = ".jpg";

        File externalStorageDir = mActivityTestRule.getActivity().getExternalFilesDir(
                Environment.DIRECTORY_DOWNLOADS);
        File imageFile = ShareImageFileUtils.getNextAvailableFile(
                externalStorageDir.getPath(), filename, extension);
        Assert.assertTrue(imageFile.exists());

        File imageFile2 = ShareImageFileUtils.getNextAvailableFile(
                externalStorageDir.getPath(), filename, extension);
        Assert.assertTrue(imageFile2.exists());
        Assert.assertNotEquals(imageFile.getPath(), imageFile2.getPath());
    }
}
