// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.internal.common.testing;

import org.chromium.components.feed.core.proto.wire.ContentIdProto.ContentId;

/** Test support class which creates Jardin content ids. */
public class ContentIdGenerators {
    private static final String FEATURE = "feature";
    private static final String ROOT_DOMAIN = "stream_root";
    private static final ContentId FEATURE_CONTENT_ID =
            ContentId.newBuilder().setContentDomain(FEATURE).setId(0).setTable(FEATURE).build();
    private static final ContentId TOKEN_ID =
            ContentId.newBuilder().setContentDomain("token").setId(0).setTable(FEATURE).build();
    private static final ContentId SHARED_STATE_CONTENT_ID =
            ContentId.newBuilder()
                    .setContentDomain("shared-state")
                    .setId(0)
                    .setTable(FEATURE)
                    .build();

    public static final String ROOT_PREFIX = FEATURE + "::" + ROOT_DOMAIN;

    public String createFeatureContentId(long id) {
        return createContentId(FEATURE_CONTENT_ID.toBuilder().setId(id).build());
    }

    public String createTokenContentId(long id) {
        return createContentId(TOKEN_ID.toBuilder().setId(id).build());
    }

    public String createSharedStateContentId(long id) {
        return createContentId(SHARED_STATE_CONTENT_ID.toBuilder().setId(id).build());
    }

    public String createRootContentId(int id) {
        return createContentId(ContentId.newBuilder()
                                       .setContentDomain(ROOT_DOMAIN)
                                       .setId(id)
                                       .setTable(FEATURE)
                                       .build());
    }

    public String createContentId(ContentId contentId) {
        // Using String concat for performance reasons.  This is called a lot for large feed
        // responses.
        return contentId.getTable() + "::" + contentId.getContentDomain()
                + "::" + contentId.getId();
    }
}
