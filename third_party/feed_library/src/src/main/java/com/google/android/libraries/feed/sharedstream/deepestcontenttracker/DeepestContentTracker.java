// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.sharedstream.deepestcontenttracker;

import android.support.annotation.VisibleForTesting;

import com.google.android.libraries.feed.api.internal.modelprovider.ModelProvider.ViewDepthProvider;

import java.util.ArrayList;
import java.util.List;

/** Tracks the {@code contentId} of the deepest viewed content. */
public class DeepestContentTracker implements ViewDepthProvider {
    private static final String TAG = "DeepestContentTracker";

    private final List</*@Nullable*/ String> contentIds = new ArrayList<>();

    public void updateDeepestContentTracker(int contentPosition, /*@Nullable*/ String contentId) {
        // Fill content ids to size of content position. This is needed in-case we programmatically
        // set scroll position of the recycler view. Add one to contentPosition size in order to
        // more easily perform a set below.
        while (contentIds.size() < contentPosition + 1) {
            contentIds.add(null);
        }

        // Just update the content id of the item in the list.
        contentIds.set(contentPosition, contentId);
    }

    public void removeContentId(int contentPosition) {
        if (contentPosition >= contentIds.size()) {
            return;
        }

        contentIds.remove(contentPosition);
    }

    @VisibleForTesting
    /*@Nullable*/
    String getContentItAtPosition(int position) {
        if (position >= contentIds.size() || position < 0) {
            return null;
        }

        return contentIds.get(position);
    }

    public void reset() {
        contentIds.clear();
    }

    @Override
    /*@Nullable*/
    public String getChildViewDepth() {
        if (contentIds.isEmpty()) {
            return null;
        }

        int i = contentIds.size() - 1;
        while (contentIds.get(i) == null && i > 0) {
            i--;
        }

        return contentIds.get(i);
    }
}
