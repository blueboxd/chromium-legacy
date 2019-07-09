// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.photo_picker;

import android.content.ContentResolver;
import android.content.res.AssetFileDescriptor;
import android.graphics.Bitmap;
import android.media.MediaMetadataRetriever;
import android.net.Uri;
import android.util.Pair;

import org.chromium.base.ThreadUtils;
import org.chromium.base.task.AsyncTask;

import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.List;
import java.util.Locale;

/**
 * A worker task to decode video and extract information from it off of the UI thread.
 */
class DecodeVideoTask extends AsyncTask<Pair<List<Bitmap>, String>> {
    /**
     * An interface to use to communicate back the results to the client.
     */
    public interface VideoDecodingCallback {
        /**
         * A callback to define to receive the list of all images on disk.
         * @param uri The uri of the video decoded.
         * @param bitmaps An array of thumbnails extracted from the video.
         * @param duration The duration of the video.
         */
        void videoDecodedCallback(Uri uri, List<Bitmap> bitmaps, String duration);
    }

    // The callback to use to communicate the results.
    private VideoDecodingCallback mCallback;

    // The URI of the video to decode.
    private Uri mUri;

    // The desired width and height (in pixels) of the returned thumbnail from the video.
    int mSize;

    // The number of frames to extract.
    int mFrames;

    // The interval between frames (in milliseconds).
    long mIntervalMs;

    // The ContentResolver to use to retrieve image metadata from disk.
    private ContentResolver mContentResolver;

    // A metadata retriever, used to decode the video, and extract a thumbnail frame.
    private MediaMetadataRetriever mRetriever = new MediaMetadataRetriever();

    /**
     * A DecodeVideoTask constructor.
     * @param callback The callback to use to communicate back the results.
     * @param contentResolver The ContentResolver to use to retrieve image metadata from disk.
     * @param uri The URI of the video to decode.
     * @param size The desired width and height (in pixels) of the returned thumbnail from the
     *             video.
     * @param frames The number of frames to extract.
     * @param intervalMs The interval between frames (in milliseconds).
     */
    public DecodeVideoTask(VideoDecodingCallback callback, ContentResolver contentResolver, Uri uri,
            int size, int frames, long intervalMs) {
        mCallback = callback;
        mContentResolver = contentResolver;
        mUri = uri;
        mSize = size;
        mFrames = frames;
        mIntervalMs = intervalMs;
    }

    /**
     * Converts a duration string in ms to a human-readable form.
     * @param durationMs The duration in milliseconds.
     * @return The duration in human-readable form.
     */
    private String formatDuration(String durationMs) {
        if (durationMs == null) return null;

        long duration = Long.parseLong(durationMs) / 1000;
        long hours = duration / 3600;
        duration -= hours * 3600;
        long minutes = duration / 60;
        duration -= minutes * 60;
        long seconds = duration;
        if (hours > 0) {
            return String.format(Locale.US, "%02d:%02d:%02d", hours, minutes, seconds);
        } else {
            return String.format(Locale.US, "%02d:%02d", minutes, seconds);
        }
    }

    /**
     * Decodes a video and extracts metadata and a thumbnail. Called on a non-UI thread
     * @return A pair of bitmap (video thumbnail) and the duration of the video.
     */
    @Override
    protected Pair<List<Bitmap>, String> doInBackground() {
        assert !ThreadUtils.runningOnUiThread();

        if (isCancelled()) return null;

        AssetFileDescriptor afd = null;
        try {
            afd = mContentResolver.openAssetFileDescriptor(mUri, "r");
            mRetriever.setDataSource(afd.getFileDescriptor());
            String duration =
                    mRetriever.extractMetadata(MediaMetadataRetriever.METADATA_KEY_DURATION);
            if (duration != null) {
                // Adjust to a shorter video, if the frame requests exceed the length of the video.
                long durationMs = Long.parseLong(duration);
                if (mFrames > 1 && mFrames * mIntervalMs > durationMs) {
                    mIntervalMs = durationMs / mFrames;
                }
                duration = formatDuration(duration);
            }
            List<Bitmap> bitmaps = BitmapUtils.decodeVideoFromFileDescriptor(
                    mRetriever, afd.getFileDescriptor(), mSize, mFrames, mIntervalMs);

            return new Pair<List<Bitmap>, String>(bitmaps, duration);
        } catch (FileNotFoundException exception) {
            return null;
        } catch (RuntimeException exception) {
            return null;
        } finally {
            try {
                if (afd != null) afd.close();
            } catch (IOException exception) {
                return null;
            }
        }
    }

    /**
     * Communicates the results back to the client. Called on the UI thread.
     * @param results A pair of bitmap (video thumbnail) and the duration of the video.
     */
    @Override
    protected void onPostExecute(Pair<List<Bitmap>, String> results) {
        if (isCancelled()) {
            return;
        }

        if (results == null) {
            mCallback.videoDecodedCallback(mUri, null, "");
            return;
        }

        mCallback.videoDecodedCallback(mUri, results.first, results.second);
    }
}
