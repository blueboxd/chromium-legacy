// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import android.content.Context;
import android.graphics.Bitmap;
import android.net.Uri;
import android.os.Environment;
import android.text.TextUtils;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.Callback;
import org.chromium.base.ContentUriUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.StreamUtil;
import org.chromium.base.task.AsyncTask;
import org.chromium.content_public.browser.RenderWidgetHostView;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.Clipboard;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.Locale;

/**
 * Utility class for file operations for image data.
 */
public class ShareImageFileUtils {
    private static final String TAG = "share";

    /**
     * Directory name for shared images.
     *
     * Named "screenshot" for historical reasons as we only initially shared screenshot images.
     * TODO(crbug.com/1055886): consider changing the directory name.
     */
    private static final String SHARE_IMAGES_DIRECTORY_NAME = "screenshot";
    private static final String JPEG_EXTENSION = ".jpg";
    private static final String FILE_NUMBER_FORMAT = " (%d)";

    /**
     * Delete the |file|, if the |file| is a directory, delete the files and directories in the
     * directory recursively.
     *
     * @param file The {@link File} or directory to be deleted.
     * @param reservedFilepath The filepath should not to be deleted.
     * @return Whether the |folder| has file to keep/reserve.
     */
    private static boolean deleteFiles(File file, @Nullable String reservedFilepath) {
        if (!file.exists()) return false;
        if (reservedFilepath != null && file.isFile()
                && file.getPath().endsWith(reservedFilepath)) {
            return true;
        }

        boolean anyChildKept = false;
        if (file.isDirectory()) {
            File[] file_list = file.listFiles();
            if (file_list != null) {
                for (File child : file.listFiles()) {
                    anyChildKept |= deleteFiles(child, reservedFilepath);
                }
            }
        }

        // file.delete() will fail if |file| is a directory and has a file need to keep. In this
        // case, the log should not been recorded since it is correct.
        if (!anyChildKept && !file.delete()) {
            Log.w(TAG, "Failed to delete share image file: %s", file.getAbsolutePath());
            return true;
        }
        return anyChildKept;
    }

    /**
     * Check if the file related to |fileUri| is in the |folder|.
     *
     * @param fileUri The {@link Uri} related to the file to be checked.
     * @param folder The folder that may contain the |fileUrl|.
     * @return Whether the |fileUri| is in the |folder|.
     */
    private static boolean isUriInDirectory(Uri fileUri, File folder) {
        if (fileUri == null) return false;

        Uri chromeUriPrefix = ContentUriUtils.getContentUriFromFile(folder);
        if (chromeUriPrefix == null) return false;

        return fileUri.toString().startsWith(chromeUriPrefix.toString());
    }

    /**
     * Check if the system clipboard contains a Uri that comes from Chrome. If yes, return the file
     * name from the Uri, otherwise return null.
     *
     * @return The file name if system clipboard contains a Uri from Chrome, otherwise return null.
     */
    private static String getClipboardCurrentFilepath() throws IOException {
        Uri clipboardUri = Clipboard.getInstance().getImageUri();
        if (isUriInDirectory(clipboardUri, getSharedFilesDirectory())) {
            return clipboardUri.getPath();
        }
        return null;
    }

    /**
     * Returns the directory where temporary files are stored to be shared with external
     * applications. These files are deleted on startup and when there are no longer any active
     * Activities.
     *
     * @return The directory where shared files are stored.
     */
    public static File getSharedFilesDirectory() throws IOException {
        File imagePath = UiUtils.getDirectoryForImageCapture(ContextUtils.getApplicationContext());
        return new File(imagePath, SHARE_IMAGES_DIRECTORY_NAME);
    }

    /**
     * Clears all shared image files.
     */
    public static void clearSharedImages() {
        AsyncTask.SERIAL_EXECUTOR.execute(() -> {
            try {
                deleteFiles(getSharedFilesDirectory(), getClipboardCurrentFilepath());
            } catch (IOException ie) {
                // Ignore exception.
            }
        });
    }

    /**
     * Temporarily saves the given set of JPEG bytes and provides that URI to a callback for
     * sharing.
     *
     * @param context The context used to trigger the share action.
     * @param jpegImageData The image data to be shared in jpeg format.
     * @param callback A provided callback function which will act on the generated URI.
     */
    public static void generateTemporaryUriFromData(
            final Context context, final byte[] jpegImageData, Callback<Uri> callback) {
        if (jpegImageData.length == 0) {
            Log.w(TAG, "Share failed -- Received image contains no data.");
            return;
        }
        OnImageSaveListener listener = new OnImageSaveListener() {
            @Override
            public void onImageSaved(File imageFile) {
                callback.onResult(ContentUriUtils.getContentUriFromFile(imageFile));
            }
            @Override
            public void onImageSaveError() {}
        };

        String fileName = String.valueOf(System.currentTimeMillis());
        saveImage(fileName, "", listener, (fos) -> { writeImageData(fos, jpegImageData); }, true);
    }

    /**
     * Saves bitmap to external storage directory.
     *
     * @param context The Context to use for determining download location.
     * @param filename The filename without extension.
     * @param bitmap The Bitmap to download.
     * @param listener The OnImageSaveListener to notify the download results.
     */
    public static void saveBitmapToExternalStorage(
            final Context context, String fileName, Bitmap bitmap, OnImageSaveListener listener) {
        String filePath = context.getExternalFilesDir(Environment.DIRECTORY_DOWNLOADS).getPath();
        saveImage(fileName, filePath, listener, (fos) -> { writeBitmap(fos, bitmap); }, false);
    }

    /**
     * Interface for notifying bitmap download result.
     */
    public interface OnImageSaveListener {
        void onImageSaved(File imageFile);
        void onImageSaveError();
    }

    /**
     * Interface for writing image information to a output stream.
     */
    private interface FileOutputStreamWriter {
        void write(FileOutputStream fos) throws IOException;
    }

    /**
     * Saves image to the given file.
     *
     * @param fileName The File instance of a destination file.
     * @param filePath The File instance of a destination file.
     * @param listener The OnImageSaveListener to notify the download results.
     * @param writer The FileOutputStreamWriter that writes to given stream.
     * @param isTemporary Indicates whether image should be save to a temporary file.
     */
    private static void saveImage(String fileName, String filePath, OnImageSaveListener listener,
            FileOutputStreamWriter writer, boolean isTemporary) {
        new AsyncTask<File>() {
            @Override
            protected File doInBackground() {
                FileOutputStream fOut = null;
                File destFile = null;
                try {
                    destFile = createFile(fileName, filePath, isTemporary);
                    if (destFile != null && destFile.exists()) {
                        fOut = new FileOutputStream(destFile);
                        writer.write(fOut);
                    } else {
                        Log.w(TAG,
                                "Share failed -- Unable to create or write to destination file.");
                    }
                } catch (IOException ie) {
                    cancel(true);
                } finally {
                    StreamUtil.closeQuietly(fOut);
                }

                return destFile;
            }

            @Override
            protected void onCancelled() {
                listener.onImageSaveError();
            }

            @Override
            protected void onPostExecute(File imageFile) {
                if (imageFile == null) {
                    listener.onImageSaveError();
                    return;
                }

                if (ApplicationStatus.getStateForApplication()
                        == ApplicationState.HAS_DESTROYED_ACTIVITIES) {
                    return;
                }

                listener.onImageSaved(imageFile);
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    /**
     * Creates file with specified path, name and extension.
     *
     * @param filePath The file path a destination file.
     * @param fileName The file name a destination file.
     * @param isTemporary Indicates whether image should be save to a temporary file.
     *
     * @return The new File object.
     */
    private static File createFile(String fileName, String filePath, boolean isTemporary)
            throws IOException {
        File path;
        if (filePath.isEmpty()) {
            path = getSharedFilesDirectory();
        } else {
            path = new File(filePath);
        }

        File newFile = null;
        if (path.exists() || path.mkdir()) {
            if (isTemporary) {
                newFile = File.createTempFile(fileName, JPEG_EXTENSION, path);
            } else {
                newFile = getNextAvailableFile(filePath, fileName, JPEG_EXTENSION);
            }
        }

        return newFile;
    }

    /**
     * Returns next available file for the given fileName.
     *
     * @param filePath The file path a destination file.
     * @param fileName The file name a destination file.
     * @param extension The extension a destination file.
     *
     * @return The new File object.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static File getNextAvailableFile(String filePath, String fileName, String extension)
            throws IOException {
        File destFile = new File(filePath, fileName + extension);
        int num = 0;
        while (destFile.exists()) {
            destFile = new File(filePath,
                    fileName + String.format(Locale.getDefault(), FILE_NUMBER_FORMAT, ++num)
                            + extension);
        }
        destFile.createNewFile();

        return destFile;
    }

    /**
     * Writes given bitmap to into the given fos.
     *
     * @param fos The FileOutputStream to write to.
     * @param bitmap The Bitmap to write.
     */
    private static void writeBitmap(FileOutputStream fos, Bitmap bitmap) throws IOException {
        bitmap.compress(Bitmap.CompressFormat.JPEG, 100, fos);
    }

    /**
     * Writes given data to into the given fos.
     *
     * @param fos The FileOutputStream to write to.
     * @param byte[] The byte[] to write.
     */
    private static void writeImageData(FileOutputStream fos, final byte[] data) throws IOException {
        fos.write(data);
    }

    /**
     * Captures a screenshot for the provided web contents, persists it and notifies the file
     * provider that the file is ready to be accessed by the client.
     *
     * The screenshot is compressed to JPEG before being written to the file.
     *
     * @param contents The WebContents instance for which to capture a screenshot.
     * @param width    The desired width of the resulting screenshot, or 0 for "auto."
     * @param height   The desired height of the resulting screenshot, or 0 for "auto."
     * @param callback The callback that will be called once the screenshot is saved.
     */
    public static void captureScreenshotForContents(
            WebContents contents, int width, int height, Callback<Uri> callback) {
        RenderWidgetHostView rwhv = contents.getRenderWidgetHostView();
        if (rwhv == null) {
            callback.onResult(null);
            return;
        }
        try {
            String path = UiUtils.getDirectoryForImageCapture(ContextUtils.getApplicationContext())
                    + File.separator + SHARE_IMAGES_DIRECTORY_NAME;
            rwhv.writeContentBitmapToDiskAsync(
                    width, height, path, new ExternallyVisibleUriCallback(callback));
        } catch (IOException e) {
            Log.e(TAG, "Error getting content bitmap: ", e);
            callback.onResult(null);
        }
    }

    private static class ExternallyVisibleUriCallback implements Callback<String> {
        private Callback<Uri> mComposedCallback;
        ExternallyVisibleUriCallback(Callback<Uri> cb) {
            mComposedCallback = cb;
        }

        @Override
        public void onResult(final String path) {
            if (TextUtils.isEmpty(path)) {
                mComposedCallback.onResult(null);
                return;
            }

            new AsyncTask<Uri>() {
                @Override
                protected Uri doInBackground() {
                    return ContentUriUtils.getContentUriFromFile(new File(path));
                }

                @Override
                protected void onPostExecute(Uri uri) {
                    mComposedCallback.onResult(uri);
                }
            }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        }
    }
}
