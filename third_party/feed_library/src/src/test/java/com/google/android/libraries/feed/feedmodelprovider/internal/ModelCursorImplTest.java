// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.feedmodelprovider.internal;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.mock;
import static org.mockito.MockitoAnnotations.initMocks;

import com.google.android.libraries.feed.api.internal.common.testing.ContentIdGenerators;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelFeature;
import com.google.android.libraries.feed.feedmodelprovider.internal.ModelCursorImpl.CursorIterator;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

import java.util.ArrayList;
import java.util.List;

/** Tests of the {@link ModelCursorImpl}. */
@RunWith(RobolectricTestRunner.class)
public class ModelCursorImplTest {
    private List<UpdatableModelChild> modelChildren;
    private String parentContentId;
    private final ContentIdGenerators contentIdGenerators = new ContentIdGenerators();

    @Before
    public void setup() {
        initMocks(this);
        modelChildren = new ArrayList<>();
        parentContentId = "parent.content.id";
    }

    @Test
    public void testEmptyCursor() {
        ModelCursorImpl cursor = new ModelCursorImpl(parentContentId, modelChildren);
        assertThat(cursor.isAtEnd()).isTrue();
    }

    @Test
    public void testReleaseCursor() {
        modelChildren.add(new UpdatableModelChild("contentId", "parentId"));
        ModelCursorImpl cursor = new ModelCursorImpl(parentContentId, modelChildren);
        assertThat(cursor.isAtEnd()).isFalse();

        cursor.release();
        assertThat(cursor.isAtEnd()).isTrue();

        assertThat(cursor.getNextItem()).isNull();
    }

    @Test
    public void testIteration() {
        int childrenToAdd = 3;
        for (int i = 0; i < childrenToAdd; i++) {
            modelChildren.add(new UpdatableModelChild("contentId", "parentId"));
        }
        ModelCursorImpl cursor = new ModelCursorImpl(parentContentId, modelChildren);
        int childCount = 0;
        for (int i = 0; i < childrenToAdd; i++) {
            childCount++;
            assertThat(cursor.getNextItem()).isNotNull();
        }
        assertThat(cursor.getNextItem()).isNull();
        assertThat(cursor.isAtEnd()).isTrue();
        assertThat(childCount).isEqualTo(childrenToAdd);
    }

    @Test
    public void testUpdateIterator_append() {
        int childrenToAdd = 3;
        for (int i = 0; i < childrenToAdd; i++) {
            modelChildren.add(new UpdatableModelChild("contentId", "parentId"));
        }
        ModelCursorImpl cursor = new ModelCursorImpl(parentContentId, modelChildren);
        ModelFeature modelFeature = mock(ModelFeature.class);
        FeatureChangeImpl featureChange = new FeatureChangeImpl(modelFeature);
        UpdatableModelChild newChild = new UpdatableModelChild("contentId", "parentId");
        featureChange.getChildChangesImpl().addAppendChild(newChild);
        cursor.updateIterator(featureChange);
        List<UpdatableModelChild> children = cursor.getChildListForTesting();
        assertThat(children).hasSize(4);
        assertThat(children.get(3)).isEqualTo(newChild);
    }

    @Test
    public void testUpdateIterator_remove() {
        int childrenToAdd = 3;
        for (int i = 0; i < childrenToAdd; i++) {
            String contentId = contentIdGenerators.createFeatureContentId(i);
            UpdatableModelChild child = new UpdatableModelChild(contentId, "parentId");
            modelChildren.add(child);
        }
        ModelCursorImpl cursor = new ModelCursorImpl(parentContentId, modelChildren);
        ModelFeature modelFeature = mock(ModelFeature.class);
        FeatureChangeImpl featureChange = new FeatureChangeImpl(modelFeature);
        featureChange.getChildChangesImpl().removeChild(modelChildren.get(1));
        cursor.updateIterator(featureChange);
        List<UpdatableModelChild> children = cursor.getChildListForTesting();
        assertThat(children).hasSize(2);
    }

    @Test
    public void testCursorIterator() {
        int childrenToAdd = 3;
        for (int i = 0; i < childrenToAdd; i++) {
            modelChildren.add(new UpdatableModelChild("contentId", "parentId"));
        }
        CursorIterator cursorIterator =
                new ModelCursorImpl(parentContentId, modelChildren).new CursorIterator();
        assertThat(cursorIterator.getPosition()).isEqualTo(0);
        assertThat(cursorIterator.hasNext()).isTrue();
        assertThat(cursorIterator.next()).isEqualTo(modelChildren.get(0));
        assertThat(cursorIterator.getPosition()).isEqualTo(1);
    }

    @Test
    public void testCopiesList() {
        UpdatableModelChild child1 = new UpdatableModelChild("content id", null);
        modelChildren.add(child1);
        ModelCursorImpl modelCursor = new ModelCursorImpl(parentContentId, modelChildren);
        UpdatableModelChild child2 = new UpdatableModelChild("content id 2", null);
        modelChildren.add(child2);
        assertThat(modelCursor.getChildListForTesting()).containsExactly(child1);
    }
}
