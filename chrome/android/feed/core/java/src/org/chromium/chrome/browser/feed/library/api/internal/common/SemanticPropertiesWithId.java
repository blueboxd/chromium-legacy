// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.internal.common;

import androidx.annotation.Nullable;

import java.util.Arrays;

/**
 * Structure style class which binds a String contentId with the semantic properties data for that
 * contentId. The class is immutable and provides access to the fields directly.
 */
public final class SemanticPropertiesWithId {
    public final String contentId;
    public final byte[] semanticData;

    public SemanticPropertiesWithId(String contentId, byte[] semanticData) {
        this.contentId = contentId;
        this.semanticData = semanticData;
    }

    @Override
    public boolean equals(@Nullable Object o) {
        if (this == o) {
            return true;
        }
        if (o == null || getClass() != o.getClass()) {
            return false;
        }

        SemanticPropertiesWithId that = (SemanticPropertiesWithId) o;

        if (!contentId.equals(that.contentId)) {
            return false;
        }
        return Arrays.equals(semanticData, that.semanticData);
    }

    @Override
    public int hashCode() {
        int result = contentId.hashCode();
        result = 31 * result + Arrays.hashCode(semanticData);
        return result;
    }
}
