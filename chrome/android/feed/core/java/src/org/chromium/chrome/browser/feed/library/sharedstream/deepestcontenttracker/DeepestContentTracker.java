// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.sharedstream.deepestcontenttracker;

import android.support.annotation.VisibleForTesting;

import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider.ViewDepthProvider;

import java.util.ArrayList;
import java.util.List;

/** Tracks the {@code contentId} of the deepest viewed content. */
public class DeepestContentTracker implements ViewDepthProvider {
    private static final String TAG = "DeepestContentTracker";

    private final List</*@Nullable*/ String> mContentIds = new ArrayList<>();

    public void updateDeepestContentTracker(int contentPosition, /*@Nullable*/ String contentId) {
        // Fill content ids to size of content position. This is needed in-case we programmatically
        // set scroll position of the recycler view. Add one to contentPosition size in order to
        // more easily perform a set below.
        while (mContentIds.size() < contentPosition + 1) {
            mContentIds.add(null);
        }

        // Just update the content id of the item in the list.
        mContentIds.set(contentPosition, contentId);
    }

    public void removeContentId(int contentPosition) {
        if (contentPosition >= mContentIds.size()) {
            return;
        }

        mContentIds.remove(contentPosition);
    }

    @VisibleForTesting
    /*@Nullable*/
    String getContentItAtPosition(int position) {
        if (position >= mContentIds.size() || position < 0) {
            return null;
        }

        return mContentIds.get(position);
    }

    public void reset() {
        mContentIds.clear();
    }

    @Override
    /*@Nullable*/
    public String getChildViewDepth() {
        if (mContentIds.isEmpty()) {
            return null;
        }

        int i = mContentIds.size() - 1;
        while (mContentIds.get(i) == null && i > 0) {
            i--;
        }

        return mContentIds.get(i);
    }
}
