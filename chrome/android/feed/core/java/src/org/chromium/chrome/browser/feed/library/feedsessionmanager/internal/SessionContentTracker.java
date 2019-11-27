// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedsessionmanager.internal;

import org.chromium.chrome.browser.feed.library.common.logging.Dumpable;
import org.chromium.chrome.browser.feed.library.common.logging.Dumper;
import org.chromium.chrome.browser.feed.library.common.logging.Logger;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamStructure;

import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** Class tracking the content IDs associated to a session. */
public class SessionContentTracker implements Dumpable {
    private static final String TAG = "SessionContentTracker";

    private final boolean mSupportsClearAll;
    private final Set<String> mContentIds = new HashSet<>();

    SessionContentTracker(boolean supportsClearAll) {
        this.mSupportsClearAll = supportsClearAll;
    }

    public boolean isEmpty() {
        return mContentIds.isEmpty();
    }

    public void clear() {
        mContentIds.clear();
    }

    public boolean contains(String contentId) {
        return mContentIds.contains(contentId);
    }

    public Set<String> getContentIds() {
        return new HashSet<>(mContentIds);
    }

    public void update(StreamStructure streamStructure) {
        String contentId = streamStructure.getContentId();
        switch (streamStructure.getOperation()) {
            case UPDATE_OR_APPEND: // Fall-through
            case REQUIRED_CONTENT:
                mContentIds.add(contentId);
                break;
            case REMOVE:
                mContentIds.remove(contentId);
                break;
            case CLEAR_ALL:
                if (mSupportsClearAll) {
                    mContentIds.clear();
                } else {
                    Logger.i(TAG, "CLEAR_ALL not supported.");
                }
                break;
            default:
                Logger.e(TAG, "unsupported operation: %s", streamStructure.getOperation());
        }
    }

    public void update(List<StreamStructure> streamStructures) {
        for (StreamStructure streamStructure : streamStructures) {
            update(streamStructure);
        }
    }

    @Override
    public void dump(Dumper dumper) {
        dumper.title(TAG);
        dumper.forKey("contentIds").value(mContentIds.size());
    }
}
