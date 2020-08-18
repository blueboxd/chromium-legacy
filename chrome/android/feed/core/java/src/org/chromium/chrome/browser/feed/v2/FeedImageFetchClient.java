// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.v2;

import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.xsurface.ImageFetchClient;

/**
 * Implementation of xsurface's ImageFetchClient. Calls through to the native network stack.
 */
@JNINamespace("feed")
public class FeedImageFetchClient implements ImageFetchClient {
    private static class HttpResponseImpl implements ImageFetchClient.HttpResponse {
        private int mStatus;
        private byte[] mBody;

        public HttpResponseImpl(int status, byte[] body) {
            mStatus = status;
            mBody = body;
        }

        @Override
        public int status() {
            return mStatus;
        }

        @Override
        public byte[] body() {
            return mBody;
        }
    }

    @Override
    public void sendRequest(String url, ImageFetchClient.HttpResponseConsumer responseConsumer) {
        assert ThreadUtils.runningOnUiThread();
        FeedImageFetchClientJni.get().sendRequest(url, responseConsumer);
    }

    @CalledByNative
    public static void onHttpResponse(
            ImageFetchClient.HttpResponseConsumer responseConsumer, int status, byte[] body) {
        responseConsumer.requestComplete(new HttpResponseImpl(status, body));
    }

    @NativeMethods
    interface Natives {
        void sendRequest(String url, ImageFetchClient.HttpResponseConsumer responseConsumer);
    }
}
