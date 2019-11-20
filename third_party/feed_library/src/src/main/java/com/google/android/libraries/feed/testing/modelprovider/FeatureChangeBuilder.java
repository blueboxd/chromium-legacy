// Copyright 2018 The Feed Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package com.google.android.libraries.feed.testing.modelprovider;

import com.google.android.libraries.feed.api.internal.modelprovider.FeatureChange;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelChild;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelFeature;
import java.util.ArrayList;
import java.util.List;

/**
 * Builds a {@link FeatureChange} representing the addition or removal of given {@link ModelChild}
 * instances.
 */
public class FeatureChangeBuilder {

  private final List<ModelChild> removedChildren = new ArrayList<>();
  private final List<ModelChild> appendedChildren = new ArrayList<>();

  public FeatureChangeBuilder addChildForRemoval(ModelChild modelChild) {
    removedChildren.add(modelChild);
    return this;
  }

  public FeatureChangeBuilder addChildForAppending(ModelChild modelChild) {
    appendedChildren.add(modelChild);
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
            return appendedChildren;
          }

          @Override
          public List<ModelChild> getRemovedChildren() {
            return removedChildren;
          }
        };
      }
    };
  }
}
