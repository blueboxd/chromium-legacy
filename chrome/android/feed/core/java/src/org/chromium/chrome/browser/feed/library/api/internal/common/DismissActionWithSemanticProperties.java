// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.internal.common;

import org.chromium.components.feed.core.proto.wire.ContentIdProto.ContentId;

import java.util.Arrays;

/**
 * Represents the content needed for a dismiss action internally. Holds the content ID of the
 * dismissed content, and any semantic properties associated with it.
 */
public final class DismissActionWithSemanticProperties {
    private final ContentId mContentId;
    private final byte /*@Nullable*/[] mSemanticProperties;

    public DismissActionWithSemanticProperties(
            ContentId contentId, byte /*@Nullable*/[] semanticProperties) {
        this.mContentId = contentId;
        this.mSemanticProperties = semanticProperties;
    }

    public ContentId getContentId() {
        return mContentId;
    }

    public byte /*@Nullable*/[] getSemanticProperties() {
        return mSemanticProperties;
    }

    @Override
    public boolean equals(/*@Nullable*/ Object o) {
        if (this == o) {
            return true;
        }
        if (o == null || getClass() != o.getClass()) {
            return false;
        }

        DismissActionWithSemanticProperties that = (DismissActionWithSemanticProperties) o;

        if (!mContentId.equals(that.mContentId)) {
            return false;
        }
        return Arrays.equals(mSemanticProperties, that.mSemanticProperties);
    }

    @Override
    public int hashCode() {
        int result = mContentId.hashCode();
        result = 31 * result + Arrays.hashCode(mSemanticProperties);
        return result;
    }
}
