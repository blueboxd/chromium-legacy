// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.internal.modelprovider;

import java.util.List;

/**
 * Class defining the specific changes made to feature nodes within the model. This is passed to the
 * observer to handle model changes. There are a few types of changes described here:
 *
 * <ol>
 *   <li>if {@link #isFeatureChanged()} returns {@code true}, then the ModelFeature was changed
 *   <li>{@link #getChildChanges()} returns a list of features either, appended, prepended, or
 *       removed from a parent
 * </ol>
 */
public interface FeatureChange {
    /** Returns the contentId of the ModelFeature which was changed. */
    String getContentId();

    /** Returns {@code true} if the ModelFeature changed. */
    boolean isFeatureChanged();

    /** Returns the ModelFeature that was changed. */
    ModelFeature getModelFeature();

    /** Returns the structural changes to the ModelFeature. */
    ChildChanges getChildChanges();

    /** Class describing changes to the children. */
    interface ChildChanges {
        /**
         * Returns a List of the children added to this ModelFeature. These children are in the same
         * order they would be displayed in the stream.
         */
        List<ModelChild> getAppendedChildren();

        /** Returns a List of the children removed from this ModelFeature. */
        List<ModelChild> getRemovedChildren();
    }
}
