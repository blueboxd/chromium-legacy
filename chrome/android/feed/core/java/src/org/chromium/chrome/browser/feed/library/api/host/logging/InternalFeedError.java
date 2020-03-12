// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.host.logging;

import androidx.annotation.IntDef;

/**
 * Represents internal errors of the Feed libraries.
 *
 * <p>When adding new values, the value of {@link InternalFeedError#NEXT_VALUE} should be used and
 * incremented. When removing values, {@link InternalFeedError#NEXT_VALUE} should not be changed,
 * and those values should not be reused.
 */
@IntDef({InternalFeedError.SWITCH_TO_EPHEMERAL, InternalFeedError.NO_URL_FOR_OPEN,
        InternalFeedError.FAILED_TO_RESTORE, InternalFeedError.NO_ROOT_FEATURE,
        InternalFeedError.TOP_LEVEL_UNBOUND_CHILD, InternalFeedError.TOP_LEVEL_INVALID_FEATURE_TYPE,
        InternalFeedError.CLUSTER_CHILD_MISSING_FEATURE, InternalFeedError.CLUSTER_CHILD_NOT_CARD,
        InternalFeedError.CARD_CHILD_MISSING_FEATURE, InternalFeedError.NULL_SHARED_STATES,
        InternalFeedError.FAILED_TO_CREATE_LEAF, InternalFeedError.UNHANDLED_TOKEN,
        InternalFeedError.TASK_QUEUE_STARVATION, InternalFeedError.CONTENT_STORAGE_MISSING_ITEM,
        InternalFeedError.ITEM_NOT_PARSED, InternalFeedError.STORAGE_MISS_BEYOND_THRESHOLD,
        InternalFeedError.CONTENT_MUTATION_FAILED, InternalFeedError.ROOT_NOT_BOUND_TO_FEATURE,
        InternalFeedError.NEXT_VALUE})
public @interface InternalFeedError {
    /** Represents when memory fails so the Feed libraries switch to ephemeral mode. */
    int SWITCH_TO_EPHEMERAL = 0;

    /** Represents when opening a URL fails as there is no URL available. */
    int NO_URL_FOR_OPEN = 1;

    /** Represents a failure to restore. */
    int FAILED_TO_RESTORE = 2;

    /** Represents getting a session with no root feature. */
    int NO_ROOT_FEATURE = 3;

    /** Represents a top level child being unbound when initializing. */
    int TOP_LEVEL_UNBOUND_CHILD = 4;

    /** Represents a top level feature being neither a card nor a cluster. */
    int TOP_LEVEL_INVALID_FEATURE_TYPE = 5;

    /** Represents a child of a cluster not having a feature. */
    int CLUSTER_CHILD_MISSING_FEATURE = 6;

    /** Represents a child of a cluster not being a card. */
    int CLUSTER_CHILD_NOT_CARD = 7;

    /** Represents a child of a card not having a feature. */
    int CARD_CHILD_MISSING_FEATURE = 8;

    /** Represents a failure to load shared states for Piet content. */
    int NULL_SHARED_STATES = 9;

    /** Represents a failure to create a leaf child. */
    int FAILED_TO_CREATE_LEAF = 10;

    /** Represents a token that could not be handled. */
    int UNHANDLED_TOKEN = 11;

    /**
     * Represents that the task queue timed out waiting for a network request and began executing
     * queued tasks.
     */
    int TASK_QUEUE_STARVATION = 12;

    /** Represents a call to ContentStorage that returned with a missing item. */
    int CONTENT_STORAGE_MISSING_ITEM = 13;

    /** Represents bytes from ContentStorage that cannot be parsed into the proto. */
    int ITEM_NOT_PARSED = 14;

    /**
     * Represents a storage miss where FeedStore reports success but is missing a number of items
     * that exceeds the configurable threshold.
     */
    int STORAGE_MISS_BEYOND_THRESHOLD = 15;

    /** Represents a failed content mutation in FeedSessionManager. */
    int CONTENT_MUTATION_FAILED = 16;

    /** Represents a root feature that is not bound as expected. */
    int ROOT_NOT_BOUND_TO_FEATURE = 17;

    /** The next value that should be used when adding additional values to the IntDef. */
    int NEXT_VALUE = 18;
}
