// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.common;

import androidx.annotation.Nullable;

import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamToken;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.UiContext;

/**
 * This tracks the context of a mutation. When a request is made, this will track the reason for the
 * request and pass this information back with the response.
 */
public final class MutationContext {
    @Nullable
    private final StreamToken mContinuationToken;
    @Nullable
    private final String mRequestingSessionId;
    private final boolean mUserInitiated;

    /** Static used to represent an empty Mutation Context */
    public static final MutationContext EMPTY_CONTEXT =
            new MutationContext(null, null, UiContext.getDefaultInstance(), false);

    private final UiContext mUiContext;

    private MutationContext(@Nullable StreamToken continuationToken,
            @Nullable String requestingSessionId, UiContext uiContext, boolean userInitiated) {
        this.mContinuationToken = continuationToken;
        this.mRequestingSessionId = requestingSessionId;
        this.mUserInitiated = userInitiated;
        this.mUiContext = uiContext;
    }

    /** Returns the continuation token used to make the request. */
    @Nullable
    public StreamToken getContinuationToken() {
        return mContinuationToken;
    }

    /** Returns the session which made the request. */
    @Nullable
    public String getRequestingSessionId() {
        return mRequestingSessionId;
    }

    /**
     * Returns the {@link UiContext} which made the request or {@link
     * UiContext#getDefaultInstance()} if none was present.
     */
    public UiContext getUiContext() {
        return mUiContext;
    }

    /** Returns {@code true} if the mutation was user initiated. */
    public boolean isUserInitiated() {
        return mUserInitiated;
    }

    /**
     * Builder for creating a {@link
     * org.chromium.chrome.browser.feed.library.api.common.MutationContext
     */
    public static final class Builder {
        private StreamToken mContinuationToken;
        private String mRequestingSessionId;
        private UiContext mUiContext = UiContext.getDefaultInstance();
        private boolean mUserInitiated;

        public Builder() {}

        public Builder setContinuationToken(StreamToken continuationToken) {
            this.mContinuationToken = continuationToken;
            return this;
        }

        public Builder setRequestingSessionId(String requestingSessionId) {
            this.mRequestingSessionId = requestingSessionId;
            return this;
        }

        public Builder setUserInitiated(boolean userInitiated) {
            this.mUserInitiated = userInitiated;
            return this;
        }

        public Builder setUiContext(UiContext uiContext) {
            this.mUiContext = uiContext;
            return this;
        }

        public MutationContext build() {
            return new MutationContext(
                    mContinuationToken, mRequestingSessionId, mUiContext, mUserInitiated);
        }
    }
}
