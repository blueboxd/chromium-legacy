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

import com.google.android.libraries.feed.api.internal.modelprovider.ModelChild;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelCursor;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelFeature;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelToken;
import com.google.common.collect.ImmutableList;
import com.google.search.now.feed.client.StreamDataProto.StreamFeature;
import com.google.search.now.feed.client.StreamDataProto.StreamToken;
import com.google.search.now.ui.stream.StreamStructureProto.Card;
import com.google.search.now.ui.stream.StreamStructureProto.Cluster;
import com.google.search.now.ui.stream.StreamStructureProto.Content;
import java.util.ArrayList;
import java.util.List;

/** A fake {@link ModelCursor} for testing. */
public class FakeModelCursor implements ModelCursor {

  private List<ModelChild> modelChildren;
  private int currentIndex = 0;

  public FakeModelCursor(List<ModelChild> modelChildren) {
    currentIndex = 0;
    this.modelChildren = modelChildren;
  }

  public static FakeModelCursor emptyCursor() {
    return FakeModelCursor.newBuilder().build();
  }

  public static Builder newBuilder() {
    return new Builder();
  }

  public void setModelChildren(List<ModelChild> modelChildren) {
    this.modelChildren = modelChildren;
  }

  @Override
  /*@Nullable*/
  public ModelChild getNextItem() {
    if (isAtEnd()) {
      return null;
    }
    return modelChildren.get(currentIndex++);
  }

  @Override
  public boolean isAtEnd() {
    return currentIndex >= modelChildren.size();
  }

  public ModelChild getChildAt(int i) {
    return modelChildren.get(i);
  }

  public List<ModelChild> getModelChildren() {
    return ImmutableList.copyOf(modelChildren);
  }

  public static class Builder {
    final List<ModelChild> cursorChildren = new ArrayList<>();

    private Builder() {}

    public Builder addCard() {
      final FakeModelCursor build = emptyCursor();
      return addChildWithModelFeature(getCardModelFeatureWithCursor(build));
    }

    public Builder addCard(String contentId) {
      return addChildWithModelFeatureAndContentId(
          getCardModelFeatureWithCursor(emptyCursor()), contentId);
    }

    public Builder addChild(ModelFeature feature) {
      return addChild(FakeModelChild.newBuilder().setModelFeature(feature).build());
    }

    public Builder addChild(ModelChild child) {
      cursorChildren.add(child);
      return this;
    }

    public Builder addChildren(List<ModelChild> children) {
      for (ModelChild child : children) {
        addChild(child);
      }
      return this;
    }

    public Builder addCluster() {
      final FakeModelCursor cursor = new FakeModelCursor(new ArrayList<>());
      return addChildWithModelFeature(getClusterModelFeatureWithCursor(cursor));
    }

    public Builder addCluster(String contentId) {
      return addChildWithModelFeature(getClusterModelFeatureWithContentId(contentId));
    }

    public Builder addClusters(int count) {
      for (int i = 0; i < count; i++) {
        addCluster();
      }

      return this;
    }

    public Builder addUnboundChild() {
      addChild(FakeModelChild.newBuilder().build());
      return this;
    }

    public Builder addChildWithModelFeature(FakeModelFeature modelFeature) {
      ModelChild cardChild = FakeModelChild.newBuilder().setModelFeature(modelFeature).build();
      cursorChildren.add(cardChild);
      return this;
    }

    public Builder addChildWithModelFeatureAndContentId(
        FakeModelFeature modelFeature, String contentId) {
      ModelChild cardChild =
          FakeModelChild.newBuilder().setModelFeature(modelFeature).setContentId(contentId).build();
      cursorChildren.add(cardChild);
      return this;
    }

    public Builder addContent(Content content) {
      final FakeModelCursor cursor = new FakeModelCursor(new ArrayList<>());
      return addChildWithModelFeature(getContentModelFeatureWithCursor(content, cursor));
    }

    public Builder addContent(Content content, String contentId) {
      final FakeModelCursor cursor = new FakeModelCursor(new ArrayList<>());
      return addChildWithModelFeatureAndContentId(
          getContentModelFeatureWithCursor(content, cursor), contentId);
    }

    public Builder addToken(boolean isSynthetic) {
      return addToken(FakeModelToken.newBuilder().setIsSynthetic(isSynthetic).build());
    }

    public Builder addToken() {
      return addToken(/* isSynthetic= */ false);
    }

    public Builder addToken(ModelToken token) {
      ModelChild tokenChild = FakeModelChild.newBuilder().setModelToken(token).build();

      cursorChildren.add(tokenChild);

      return this;
    }

    public Builder addToken(String contentId) {
      ModelChild tokenChild =
          FakeModelChild.newBuilder()
              .setModelToken(
                  FakeModelToken.newBuilder()
                      .setStreamToken(StreamToken.getDefaultInstance())
                      .build())
              .setContentId(contentId)
              .build();
      cursorChildren.add(tokenChild);
      return this;
    }

    public Builder addSyntheticToken() {
      return addToken(/* isSynthetic= */ true);
    }

    public FakeModelCursor build() {
      return new FakeModelCursor(cursorChildren);
    }
  }

  public static FakeModelFeature getCardModelFeatureWithCursor(ModelCursor modelCursor) {
    return FakeModelFeature.newBuilder()
        .setStreamFeature(StreamFeature.newBuilder().setCard(Card.getDefaultInstance()).build())
        .setModelCursor(modelCursor)
        .build();
  }

  public static FakeModelFeature getCardModelFeatureWithContentId(String contentId) {
    return FakeModelFeature.newBuilder()
        .setStreamFeature(
            StreamFeature.newBuilder()
                .setCard(Card.getDefaultInstance())
                .setContentId(contentId)
                .build())
        .setModelCursor(FakeModelCursor.newBuilder().build())
        .build();
  }

  public static FakeModelFeature getClusterModelFeatureWithCursor(ModelCursor cursor) {
    return FakeModelFeature.newBuilder()
        .setStreamFeature(
            StreamFeature.newBuilder().setCluster(Cluster.getDefaultInstance()).build())
        .setModelCursor(cursor)
        .build();
  }

  public static FakeModelFeature getClusterModelFeatureWithContentId(String contentId) {
    return FakeModelFeature.newBuilder()
        .setStreamFeature(
            StreamFeature.newBuilder()
                .setCluster(Cluster.getDefaultInstance())
                .setContentId(contentId)
                .build())
        .setModelCursor(FakeModelCursor.newBuilder().build())
        .build();
  }

  public static FakeModelFeature getContentModelFeatureWithCursor(
      Content content, ModelCursor cursor) {
    return FakeModelFeature.newBuilder()
        .setStreamFeature(StreamFeature.newBuilder().setContent(content).build())
        .setModelCursor(cursor)
        .build();
  }

  public static FakeModelFeature getModelFeatureWithEmptyCursor() {
    return FakeModelFeature.newBuilder().setModelCursor(newBuilder().build()).build();
  }
}
