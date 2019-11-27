// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedactionparser.internal;

import org.chromium.chrome.browser.feed.library.common.logging.Logger;
import org.chromium.components.feed.core.proto.ui.action.FeedActionPayloadProto.FeedActionPayload;
import org.chromium.components.feed.core.proto.ui.action.PietExtensionsProto.PietFeedActionPayload;
import org.chromium.components.feed.core.proto.ui.piet.ActionsProto.Action;

/** Class which is able to retrieve FeedActions from Piet actions */
public final class PietFeedActionPayloadRetriever {
    private static final String TAG = "PietFAPRetriever";

    /**
     * Gets the feed action from a Piet Action.
     *
     * @param action the Piet Action to pull the feed action metadata out of.
     */
    /*@Nullable*/
    public FeedActionPayload getFeedActionPayload(Action action) {
        /*@Nullable*/
        PietFeedActionPayload feedActionPayloadExtension =
                action.getExtension(PietFeedActionPayload.pietFeedActionPayloadExtension);
        if (feedActionPayloadExtension != null
                && feedActionPayloadExtension.hasFeedActionPayload()) {
            return feedActionPayloadExtension.getFeedActionPayload();
        }
        Logger.e(TAG, "FeedActionExtension was null or did not contain a feed action payload");
        return null;
    }
}
