// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.testing.modelprovider;

import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.FeatureChange;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelChild;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelFeature;

import java.util.ArrayList;
import java.util.List;

/**
 * Builds a {@link FeatureChange} representing the addition or removal of given {@link ModelChild}
 * instances.
 */
public class FeatureChangeBuilder {
    private final List<ModelChild> mRemovedChildren = new ArrayList<>();
    private final List<ModelChild> mAppendedChildren = new ArrayList<>();

    public FeatureChangeBuilder addChildForRemoval(ModelChild modelChild) {
        mRemovedChildren.add(modelChild);
        return this;
    }

    public FeatureChangeBuilder addChildForAppending(ModelChild modelChild) {
        mAppendedChildren.add(modelChild);
        return this;
    }

    public FeatureChange build() {
        return new FeatureChange() {
            @Override
            public String getContentId() {
                return "";
            }

            @Override
            public boolean isFeatureChanged() {
                return false;
            }

            @Override
            public ModelFeature getModelFeature() {
                return FakeModelFeature.newBuilder().build();
            }

            @Override
            public ChildChanges getChildChanges() {
                return new ChildChanges() {
                    @Override
                    public List<ModelChild> getAppendedChildren() {
                        return mAppendedChildren;
                    }

                    @Override
                    public List<ModelChild> getRemovedChildren() {
                        return mRemovedChildren;
                    }
                };
            }
        };
    }
}
