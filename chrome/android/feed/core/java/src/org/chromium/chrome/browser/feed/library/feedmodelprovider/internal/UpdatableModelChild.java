// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedmodelprovider.internal;

import android.text.TextUtils;

import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelChild;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelFeature;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelToken;
import org.chromium.chrome.browser.feed.library.common.logging.Logger;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamPayload;

/** */
public final class UpdatableModelChild implements ModelChild {
    private static final String TAG = "UpdatableModelChild";

    private final String mContentId;
    /*@Nullable*/ private final String mParentContentId;

    private @Type int mType = Type.UNBOUND;
    private UpdatableModelFeature mModelFeature;
    private UpdatableModelToken mModelToken;

    public UpdatableModelChild(String contentId, /*@Nullable*/ String parentContentId) {
        this.mContentId = contentId;
        this.mParentContentId = parentContentId;
    }

    void bindFeature(UpdatableModelFeature modelFeature) {
        validateType(Type.UNBOUND);
        this.mModelFeature = modelFeature;
        mType = Type.FEATURE;
    }

    public void bindToken(UpdatableModelToken modelToken) {
        validateType(Type.UNBOUND);
        this.mModelToken = modelToken;
        mType = Type.TOKEN;
    }

    @Override
    public ModelFeature getModelFeature() {
        validateType(Type.FEATURE);
        return mModelFeature;
    }

    @Override
    public ModelToken getModelToken() {
        validateType(Type.TOKEN);
        return mModelToken;
    }

    public UpdatableModelToken getUpdatableModelToken() {
        validateType(Type.TOKEN);
        return mModelToken;
    }

    void updateFeature(StreamPayload payload) {
        switch (mType) {
            case Type.FEATURE:
                if (payload.hasStreamFeature()) {
                    mModelFeature.setFeatureValue(payload.getStreamFeature());
                } else {
                    Logger.e(
                            TAG, "Attempting to update a ModelFeature without providing a feature");
                }
                break;
            case Type.TOKEN:
                Logger.e(TAG, "Update called for TOKEN is unsupported");
                break;
            case Type.UNBOUND:
                Logger.e(TAG, "updateFeature called on UNBOUND child");
                break;
            default:
                Logger.e(TAG, "Update called for unsupported type: %s", mType);
        }
    }

    @Override
    public @Type int getType() {
        return mType;
    }

    @Override
    public String getContentId() {
        return mContentId;
    }

    /*@Nullable*/
    @Override
    public String getParentId() {
        return mParentContentId;
    }

    @Override
    public boolean hasParentId() {
        return !TextUtils.isEmpty(mParentContentId);
    }

    private void validateType(@Type int type) {
        if (this.mType != type) {
            throw new IllegalStateException(String.format(
                    "ModelChild type error - Type %s, expected %s", this.mType, type));
        }
    }
}
