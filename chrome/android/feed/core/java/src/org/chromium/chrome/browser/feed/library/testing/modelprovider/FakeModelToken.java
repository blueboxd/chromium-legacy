// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.testing.modelprovider;

import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelToken;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.TokenCompletedObserver;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamToken;

import java.util.HashSet;

/** Fake for {@link ModelToken}. */
public class FakeModelToken implements ModelToken {
    private final StreamToken mStreamToken;
    private final boolean mIsSynthetic;
    private final HashSet<TokenCompletedObserver> mObservers = new HashSet<>();

    private FakeModelToken(StreamToken streamToken, boolean isSynthetic) {
        this.mStreamToken = streamToken;
        this.mIsSynthetic = isSynthetic;
    }

    public static Builder newBuilder() {
        return new Builder();
    }

    public HashSet<TokenCompletedObserver> getObservers() {
        return mObservers;
    }

    @Override
    public StreamToken getStreamToken() {
        return mStreamToken;
    }

    @Override
    public void registerObserver(TokenCompletedObserver observer) {
        mObservers.add(observer);
    }

    @Override
    public void unregisterObserver(TokenCompletedObserver observer) {
        mObservers.remove(observer);
    }

    @Override
    public boolean isSynthetic() {
        return mIsSynthetic;
    }

    public static class Builder {
        private StreamToken mStreamToken = StreamToken.getDefaultInstance();
        private boolean mIsSynthetic;

        private Builder() {}

        public Builder setStreamToken(StreamToken streamToken) {
            this.mStreamToken = streamToken;
            return this;
        }

        public Builder setIsSynthetic(boolean isSynthetic) {
            this.mIsSynthetic = isSynthetic;
            return this;
        }

        public FakeModelToken build() {
            return new FakeModelToken(mStreamToken, mIsSynthetic);
        }
    }
}
