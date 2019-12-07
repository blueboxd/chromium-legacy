// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.testing.modelprovider;

import static org.chromium.chrome.browser.feed.library.common.Validators.checkNotNull;
import static org.chromium.chrome.browser.feed.library.common.Validators.checkState;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelChild;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelFeature;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelToken;

/**
 * Fake for {@link org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelChild}.
 */
public class FakeModelChild implements ModelChild {
    private final String mContentId;

    private final @Nullable ModelFeature mModelFeature;
    private final @Nullable ModelToken mModelToken;
    private final @Nullable String mParentId;

    public static Builder newBuilder() {
        return new Builder();
    }

    private FakeModelChild(String contentId, @Nullable ModelFeature modelFeature,
            @Nullable ModelToken modelToken, @Nullable String parentId) {
        // A ModelChild can't represent both a ModelFeature and a ModelToken.
        checkState(modelFeature == null || modelToken == null);
        this.mContentId = contentId;
        this.mModelFeature = modelFeature;
        this.mModelToken = modelToken;
        this.mParentId = parentId;
    }

    @Override
    public @Type int getType() {
        if (mModelFeature != null) {
            return Type.FEATURE;
        }

        if (mModelToken != null) {
            return Type.TOKEN;
        }

        return Type.UNBOUND;
    }

    @Override
    public String getContentId() {
        return mContentId;
    }

    @Override
    public boolean hasParentId() {
        return mParentId != null;
    }

    @Override
    public @Nullable String getParentId() {
        return mParentId;
    }

    @Override
    public ModelFeature getModelFeature() {
        checkState(mModelFeature != null,
                "Must call setModelFeature on builder to have a ModelFeature");

        // checkNotNull for nullness checker, if modelFeature is null, the checkState above will
        // fail.
        return checkNotNull(mModelFeature);
    }

    @Override
    public ModelToken getModelToken() {
        checkState(mModelToken != null, "Must call setModelToken on builder to have a ModelToken");

        // checkNotNull for nullness checker, if modelToken is null, the checkState above will fail.
        return checkNotNull(mModelToken);
    }

    public static class Builder {
        private String mContentId = "";
        private @Nullable ModelFeature mModelFeature;
        private @Nullable ModelToken mModelToken;
        private @Nullable String mParentId;

        private Builder() {}

        public Builder setContentId(String contentId) {
            this.mContentId = contentId;
            return this;
        }

        public Builder setParentId(@Nullable String parentId) {
            this.mParentId = parentId;
            return this;
        }

        public Builder setModelFeature(ModelFeature modelFeature) {
            checkState(mModelToken == null);
            this.mModelFeature = modelFeature;
            return this;
        }

        public Builder setModelToken(ModelToken modelToken) {
            checkState(mModelFeature == null);
            this.mModelToken = modelToken;
            return this;
        }

        public FakeModelChild build() {
            return new FakeModelChild(mContentId, mModelFeature, mModelToken, mParentId);
        }
    }
}
