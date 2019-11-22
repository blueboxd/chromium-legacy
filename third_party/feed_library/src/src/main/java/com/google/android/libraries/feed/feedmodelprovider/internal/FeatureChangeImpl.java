// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.feedmodelprovider.internal;

import android.support.annotation.VisibleForTesting;

import com.google.android.libraries.feed.api.internal.modelprovider.FeatureChange;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelChild;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelFeature;

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
    private final ModelFeature modelFeature;
    private final ChildChangesImpl childChanges;
    private boolean featureChanged;

    public FeatureChangeImpl(ModelFeature modelFeature) {
        this.modelFeature = modelFeature;
        this.childChanges = new ChildChangesImpl();
    }

    /** Returns the String ContentId of the ModelFeature which was changed. */
    @Override
    public String getContentId() {
        return modelFeature.getStreamFeature().getContentId();
    }

    /** This is called to indicate the Content changed for this ModelFeature. */
    public void setFeatureChanged(boolean value) {
        featureChanged = value;
    }

    /** Returns {@code true} if the ModelFeature changed. */
    public boolean isFeatureChanged() {
        return featureChanged;
    }

    /** Returns the ModelFeature that was changed. */
    public ModelFeature getModelFeature() {
        return modelFeature;
    }

    /** Returns the structural changes to the ModelFeature. */
    public ChildChanges getChildChanges() {
        return childChanges;
    }

    public ChildChangesImpl getChildChangesImpl() {
        return childChanges;
    }

    /** Structure used to define the children changes. */
    public static final class ChildChangesImpl implements ChildChanges {
        private final List<ModelChild> appendChildren;
        private final List<ModelChild> removedChildren;

        @VisibleForTesting
        ChildChangesImpl() {
            this.appendChildren = new ArrayList<>();
            this.removedChildren = new ArrayList<>();
        }

        /**
         * Returns a List of the children added to this ModelFeature. These children are in the same
         * order they would be displayed in the stream.
         */
        @Override
        public List<ModelChild> getAppendedChildren() {
            return appendChildren;
        }

        @Override
        public List<ModelChild> getRemovedChildren() {
            return removedChildren;
        }

        /** Add a child to be appended to the ModelFeature children List. */
        public void addAppendChild(ModelChild child) {
            appendChildren.add(child);
        }

        /** Remove a child from the ModelFeature children List. */
        public void removeChild(ModelChild child) {
            removedChildren.add(child);
        }
    }
}
