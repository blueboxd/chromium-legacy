// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedmodelprovider.internal;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.FeatureChange;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelChild;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelFeature;

import java.util.ArrayList;
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
public final class FeatureChangeImpl implements FeatureChange {
    private final ModelFeature mModelFeature;
    private final ChildChangesImpl mChildChanges;
    private boolean mFeatureChanged;

    public FeatureChangeImpl(ModelFeature modelFeature) {
        this.mModelFeature = modelFeature;
        this.mChildChanges = new ChildChangesImpl();
    }

    /** Returns the String ContentId of the ModelFeature which was changed. */
    @Override
    public String getContentId() {
        return mModelFeature.getStreamFeature().getContentId();
    }

    /** This is called to indicate the Content changed for this ModelFeature. */
    public void setFeatureChanged(boolean value) {
        mFeatureChanged = value;
    }

    /** Returns {@code true} if the ModelFeature changed. */
    @Override
    public boolean isFeatureChanged() {
        return mFeatureChanged;
    }

    /** Returns the ModelFeature that was changed. */
    @Override
    public ModelFeature getModelFeature() {
        return mModelFeature;
    }

    /** Returns the structural changes to the ModelFeature. */
    @Override
    public ChildChanges getChildChanges() {
        return mChildChanges;
    }

    public ChildChangesImpl getChildChangesImpl() {
        return mChildChanges;
    }

    /** Structure used to define the children changes. */
    public static final class ChildChangesImpl implements ChildChanges {
        private final List<ModelChild> mAppendChildren;
        private final List<ModelChild> mRemovedChildren;

        @VisibleForTesting
        ChildChangesImpl() {
            this.mAppendChildren = new ArrayList<>();
            this.mRemovedChildren = new ArrayList<>();
        }

        /**
         * Returns a List of the children added to this ModelFeature. These children are in the same
         * order they would be displayed in the stream.
         */
        @Override
        public List<ModelChild> getAppendedChildren() {
            return mAppendChildren;
        }

        @Override
        public List<ModelChild> getRemovedChildren() {
            return mRemovedChildren;
        }

        /** Add a child to be appended to the ModelFeature children List. */
        public void addAppendChild(ModelChild child) {
            mAppendChildren.add(child);
        }

        /** Remove a child from the ModelFeature children List. */
        public void removeChild(ModelChild child) {
            mRemovedChildren.add(child);
        }
    }
}
