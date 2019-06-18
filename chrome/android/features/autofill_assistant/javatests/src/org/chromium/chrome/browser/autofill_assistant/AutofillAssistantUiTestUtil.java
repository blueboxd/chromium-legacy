// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.graphics.Bitmap;
import android.support.annotation.Nullable;
import android.view.View;
import android.widget.TextView;

import org.hamcrest.Description;
import org.hamcrest.TypeSafeMatcher;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.image_fetcher.ImageFetcher;
import org.chromium.chrome.browser.image_fetcher.ImageFetcherConfig;

import jp.tomorrowkey.android.gifplayer.BaseGifImage;

/**
 * Contains utilities for testing Autofill Assistant.
 */
class AutofillAssistantUiTestUtil {
    /** Image fetcher which synchronously returns a preset image. */
    static class MockImageFetcher extends ImageFetcher {
        private final Bitmap mBitmapToFetch;
        private final BaseGifImage mGifToFetch;

        MockImageFetcher(@Nullable Bitmap bitmapToFetch, @Nullable BaseGifImage gifToFetch) {
            mBitmapToFetch = bitmapToFetch;
            mGifToFetch = gifToFetch;
        }

        @Override
        public void fetchGif(String url, String clientName, Callback<BaseGifImage> callback) {
            callback.onResult(mGifToFetch);
        }

        @Override
        public void fetchImage(
                String url, String clientName, int width, int height, Callback<Bitmap> callback) {
            callback.onResult(mBitmapToFetch);
        }

        @Override
        public void clear() {}

        @Override
        public @ImageFetcherConfig int getConfig() {
            return ImageFetcherConfig.IN_MEMORY_ONLY;
        }

        @Override
        public void destroy() {}
    }

    /** Checks that a text view has a specific maximum number of lines to display. */
    public static TypeSafeMatcher<View> isTextMaxLines(int maxLines) {
        return new TypeSafeMatcher<View>() {
            @Override
            protected boolean matchesSafely(View item) {
                return ((TextView) item).getMaxLines() == maxLines;
            }

            @Override
            public void describeTo(Description description) {
                description.appendText("isTextMaxLines");
            }
        };
    }
}
