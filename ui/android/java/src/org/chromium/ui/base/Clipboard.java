// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.content.ClipData;
import android.content.ClipDescription;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.net.Uri;
import android.os.Build;
import android.provider.MediaStore;
import android.text.Html;
import android.text.Spanned;
import android.text.style.CharacterStyle;
import android.text.style.ParagraphStyle;
import android.text.style.UpdateAppearance;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.BuildInfo;
import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.compat.ApiHelperForO;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.ui.R;
import org.chromium.ui.widget.Toast;

import java.io.IOException;

/**
 * Simple proxy that provides C++ code with an access pathway to the Android clipboard.
 */
@JNINamespace("ui")
public class Clipboard implements ClipboardManager.OnPrimaryClipChangedListener {
    @SuppressLint("StaticFieldLeak")
    private static Clipboard sInstance;

    // Necessary for coercing clipboard contents to text if they require
    // access to network resources, etceteras (e.g., URI in clipboard)
    private final Context mContext;

    private final ClipboardManager mClipboardManager;

    private long mNativeClipboard;

    private ImageFileProvider mImageFileProvider;

    /**
     * Interface to be implemented for sharing image through FileProvider.
     */
    public interface ImageFileProvider {
        /**
         * Saves the given set of image bytes and provides that URI to a callback for
         * sharing the image.
         *
         * @param context The context used to trigger the action.
         * @param imageData The image data to be shared in |fileExtension| format.
         * @param fileExtension File extension which |imageData| encoded to.
         * @param callback A provided callback function which will act on the generated URI.
         */
        void storeImageAndGenerateUri(final Context context, final byte[] imageData,
                String fileExtension, Callback<Uri> callback);
    }

    /**
     * Get the singleton Clipboard instance (creating it if needed).
     */
    @CalledByNative
    public static Clipboard getInstance() {
        if (sInstance == null) {
            sInstance = new Clipboard();
        }
        return sInstance;
    }

    private Clipboard() {
        mContext = ContextUtils.getApplicationContext();
        mClipboardManager =
                (ClipboardManager) ContextUtils.getApplicationContext().getSystemService(
                        Context.CLIPBOARD_SERVICE);
        mClipboardManager.addPrimaryClipChangedListener(this);
    }

    /**
     * Emulates the behavior of the now-deprecated
     * {@link android.text.ClipboardManager#getText()} by invoking
     * {@link android.content.ClipData.Item#coerceToText(Context)} on the first
     * item in the clipboard (if any) and returning the result as a string.
     * <p>
     * This is quite different than simply calling {@link Object#toString()} on
     * the clip; consumers of this API should familiarize themselves with the
     * process described in
     * {@link android.content.ClipData.Item#coerceToText(Context)} before using
     * this method.
     *
     * @return a string representation of the first item on the clipboard, if
     *         the clipboard currently has an item and coercion of the item into
     *         a string is possible; otherwise, <code>null</code>
     */
    @SuppressWarnings("javadoc")
    @CalledByNative
    private String getCoercedText() {
        // getPrimaryClip() has been observed to throw unexpected exceptions for some devices (see
        // crbug.com/654802 and b/31501780)
        try {
            return mClipboardManager.getPrimaryClip()
                    .getItemAt(0)
                    .coerceToText(mContext)
                    .toString();
        } catch (Exception e) {
            return null;
        }
    }

    private boolean hasStyleSpan(Spanned spanned) {
        Class<?>[] styleClasses = {
                CharacterStyle.class, ParagraphStyle.class, UpdateAppearance.class};
        for (Class<?> clazz : styleClasses) {
            if (spanned.nextSpanTransition(-1, spanned.length(), clazz) < spanned.length()) {
                return true;
            }
        }
        return false;
    }

    public String clipDataToHtmlText(ClipData clipData) {
        ClipDescription description = clipData.getDescription();
        if (description.hasMimeType(ClipDescription.MIMETYPE_TEXT_HTML)) {
            return clipData.getItemAt(0).getHtmlText();
        }

        if (description.hasMimeType(ClipDescription.MIMETYPE_TEXT_PLAIN)) {
            CharSequence text = clipData.getItemAt(0).getText();
            if (!(text instanceof Spanned)) return null;
            Spanned spanned = (Spanned) text;
            if (hasStyleSpan(spanned)) {
                return ApiCompatibilityUtils.toHtml(
                        spanned, Html.TO_HTML_PARAGRAPH_LINES_CONSECUTIVE);
            }
        }
        return null;
    }

    /**
     * Gets the HTML text of top item on the primary clip on the Android clipboard.
     *
     * @return a Java string with the html text if any, or null if there is no html
     *         text or no entries on the primary clip.
     */
    @CalledByNative
    private String getHTMLText() {
        // getPrimaryClip() has been observed to throw unexpected exceptions for some devices (see
        // crbug/654802 and b/31501780)
        try {
            ClipData clipData = mClipboardManager.getPrimaryClip();
            return clipDataToHtmlText(clipData);
        } catch (Exception e) {
            return null;
        }
    }

    /**
     * Gets the Uri of top item on the primary clip on the Android clipboard if the mime type is
     * image.
     *
     * @return an Uri if mime type is image type, or null if there is no Uri or no entries on the
     *         primary clip.
     */
    public @Nullable Uri getImageUri() {
        // getPrimaryClip() has been observed to throw unexpected exceptions for some devices (see
        // crbug.com/654802).
        try {
            ClipData clipData = mClipboardManager.getPrimaryClip();
            if (clipData == null || clipData.getItemCount() == 0) return null;

            ClipDescription description = clipData.getDescription();
            if (description == null || !description.hasMimeType("image/*")) {
                return null;
            }

            return clipData.getItemAt(0).getUri();
        } catch (Exception e) {
            return null;
        }
    }

    @CalledByNative
    private String getImageUriString() {
        Uri uri = getImageUri();
        return uri == null ? null : uri.toString();
    }

    /**
     * Reads the Uri of top item on the primary clip on the Android clipboard, and try to get the
     * {@link Bitmap}. for that Uri.
     * Fetching images can result in I/O, so should not be called on UI thread.
     *
     * @return an {@link Bitmap} if available, otherwise null.
     */
    @CalledByNative
    public Bitmap getImage() {
        ThreadUtils.assertOnBackgroundThread();
        try {
            Uri uri = getImageUri();
            if (uri == null) return null;

            // TODO(crbug.com/1065914): Use ImageDecoder.decodeBitmap for API level 29 and up.
            return MediaStore.Images.Media.getBitmap(
                    ContextUtils.getApplicationContext().getContentResolver(), uri);
        } catch (IOException | SecurityException e) {
            return null;
        }
    }

    /**
     * Emulates the behavior of the now-deprecated
     * {@link android.text.ClipboardManager#setText(CharSequence)}, setting the
     * clipboard's current primary clip to a plain-text clip that consists of
     * the specified string.
     * @param text  will become the content of the clipboard's primary clip
     */
    @CalledByNative
    public void setText(final String text) {
        setPrimaryClipNoException(ClipData.newPlainText("text", text));
    }

    /**
     * Writes HTML to the clipboard, together with a plain-text representation
     * of that very data.
     *
     * @param html  The HTML content to be pasted to the clipboard.
     * @param text  Plain-text representation of the HTML content.
     */
    @CalledByNative
    private void setHTMLText(final String html, final String text) {
        setPrimaryClipNoException(ClipData.newHtmlText("html", text, html));
    }

    /**
     * Setting the clipboard's current primary clip to an image.
     * @param Uri The {@link Uri} will become the content of the clipboard's primary clip.
     */
    public void setImageUri(final Uri uri) {
        if (uri == null) {
            showCopyToClipboardFailureMessage();
            return;
        }

        ContextUtils.getApplicationContext().grantUriPermission(
                ClipboardManager.class.getCanonicalName(), uri,
                Intent.FLAG_GRANT_READ_URI_PERMISSION);
        ClipData clip = ClipData.newUri(
                ContextUtils.getApplicationContext().getContentResolver(), "image", uri);
        setPrimaryClipNoException(clip);
    }

    /**
     * Setting the clipboard's current primary clip to an image.
     * @param imageData The image data to be shared in |extension| format.
     * @param extension Image file extension which |imageData| encoded to.
     */
    @CalledByNative
    @VisibleForTesting
    public void setImage(final byte[] imageData, final String extension) {
        if (mImageFileProvider == null) {
            // Since |mImageFileProvider| is set on very early on during process init, and if
            // setImage is called before the file provider is set, we can just drop it on the floor.
            return;
        }

        mImageFileProvider.storeImageAndGenerateUri(
                mContext, imageData, extension, (Uri uri) -> { setImageUri(uri); });
    }

    /**
     * Clears the Clipboard Primary clip.
     *
     */
    @CalledByNative
    private void clear() {
        setPrimaryClipNoException(ClipData.newPlainText(null, null));
    }

    public void setPrimaryClipNoException(ClipData clip) {
        try {
            mClipboardManager.setPrimaryClip(clip);
        } catch (Exception ex) {
            // Ignore any exceptions here as certain devices have bugs and will fail.
            showCopyToClipboardFailureMessage();
        }
    }

    private void showCopyToClipboardFailureMessage() {
        String text = mContext.getString(R.string.copy_to_clipboard_failure_message);
        Toast.makeText(mContext, text, Toast.LENGTH_SHORT).show();
    }

    @CalledByNative
    private void setNativePtr(long nativeClipboard) {
        mNativeClipboard = nativeClipboard;
    }

    /**
     * Set {@link ImageFileProvider} for sharing image.
     * @param imageFileProvider The implementation of {@link ImageFileProvider}.
     */
    public void setImageFileProvider(ImageFileProvider imageFileProvider) {
        mImageFileProvider = imageFileProvider;
    }

    /**
     * Tells the C++ Clipboard that the clipboard has changed.
     *
     * Implements OnPrimaryClipChangedListener to listen for clipboard updates.
     */
    @Override
    public void onPrimaryClipChanged() {
        RecordUserAction.record("MobileClipboardChanged");
        if (mNativeClipboard != 0) {
            ClipboardJni.get().onPrimaryClipChanged(mNativeClipboard, Clipboard.this);
        }
    }

    /**
     * Copy the specified URL to the clipboard and show a toast indicating the action occurred.
     * @param url The URL to copy to the clipboard.
     */
    public void copyUrlToClipboard(String url) {
        ClipData clip = ClipData.newPlainText("url", url);
        mClipboardManager.setPrimaryClip(clip);
        Toast.makeText(mContext, R.string.url_copied, Toast.LENGTH_SHORT).show();
    }

    /**
     * Because Android may not notify apps in the background that the content of clipboard has
     * changed, this method proactively considers clipboard invalidated, when the app loses focus.
     * @param hasFocus Whether or not {@code activity} gained or lost focus.
     */
    public void onWindowFocusChanged(boolean hasFocus) {
        if (mNativeClipboard == 0 || !hasFocus || !BuildInfo.isAtLeastQ()) return;
        onPrimaryClipTimestampInvalidated();
    }

    @TargetApi(Build.VERSION_CODES.O)
    private void onPrimaryClipTimestampInvalidated() {
        ClipDescription clipDescription = mClipboardManager.getPrimaryClipDescription();
        if (clipDescription == null) return;

        long timestamp = ApiHelperForO.getTimestamp(clipDescription);
        ClipboardJni.get().onPrimaryClipTimestampInvalidated(
                mNativeClipboard, Clipboard.this, timestamp);
    }

    /**
     * Gets the last modified timestamp observed by the native side ClipboardAndroid, not the
     * Android framework.
     *
     * @return the last modified time in millisecond.
     */
    public long getLastModifiedTimeMs() {
        if (mNativeClipboard == 0) return 0;
        return ClipboardJni.get().getLastModifiedTimeToJavaTime(mNativeClipboard);
    }

    @NativeMethods
    interface Natives {
        void onPrimaryClipChanged(long nativeClipboardAndroid, Clipboard caller);
        void onPrimaryClipTimestampInvalidated(
                long nativeClipboardAndroid, Clipboard caller, long timestamp);
        long getLastModifiedTimeToJavaTime(long nativeClipboardAndroid);
    }
}
