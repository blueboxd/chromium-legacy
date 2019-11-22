// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package com.google.android.libraries.feed.feedstore.internal;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.verify;
import static org.mockito.MockitoAnnotations.initMocks;

import com.google.android.libraries.feed.api.host.storage.JournalMutation;
import com.google.android.libraries.feed.api.host.storage.JournalOperation;
import com.google.android.libraries.feed.api.host.storage.JournalOperation.Append;
import com.google.android.libraries.feed.api.host.storage.JournalOperation.Type;
import com.google.android.libraries.feed.api.host.storage.JournalStorageDirect;
import com.google.android.libraries.feed.api.internal.store.LocalActionMutation.ActionType;
import com.google.android.libraries.feed.common.time.TimingUtils;
import com.google.search.now.feed.client.StreamDataProto.StreamLocalAction;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.robolectric.RobolectricTestRunner;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.TimeUnit;

/** Tests of the {@link LocalActionGc} class. */
@RunWith(RobolectricTestRunner.class)
public class LocalActionGcTest {
    private static final String DISMISS_JOURNAL_NAME = "DISMISS";
    private static final String CONTENT_ID_1 = "contentId1";
    private static final String CONTENT_ID_2 = "contentId2";

    @Mock
    private JournalStorageDirect journalStorage;
    private TimingUtils timingUtils = new TimingUtils();

    @Before
    public void setUp() throws Exception {
        initMocks(this);
    }

    @Test
    public void gc_empty() throws Exception {
        // Probably won't be called with empty actions
        List<StreamLocalAction> allActions = Collections.emptyList();
        List<String> validContentIds = Collections.emptyList();

        ArgumentCaptor<JournalMutation> journalMutationCaptor =
                ArgumentCaptor.forClass(JournalMutation.class);

        LocalActionGc localActionGc = new LocalActionGc(
                allActions, validContentIds, journalStorage, timingUtils, DISMISS_JOURNAL_NAME);
        localActionGc.gc();

        verify(journalStorage).commit(journalMutationCaptor.capture());

        JournalMutation journalMutation = journalMutationCaptor.getValue();
        assertThat(journalMutation.getJournalName()).isEqualTo(DISMISS_JOURNAL_NAME);
        List<JournalOperation> journalOperations = journalMutation.getOperations();
        assertThat(journalOperations).hasSize(1);
        assertThat(journalOperations.get(0).getType()).isEqualTo(Type.DELETE);
    }

    @Test
    public void gc_allValid() throws Exception {
        StreamLocalAction action1 = StreamLocalAction.newBuilder()
                                            .setAction(ActionType.DISMISS)
                                            .setFeatureContentId(CONTENT_ID_1)
                                            .setTimestampSeconds(TimeUnit.DAYS.toSeconds(43))
                                            .build();
        StreamLocalAction action2 = StreamLocalAction.newBuilder()
                                            .setAction(ActionType.DISMISS)
                                            .setFeatureContentId(CONTENT_ID_2)
                                            .setTimestampSeconds(TimeUnit.DAYS.toSeconds(44))
                                            .build();
        List<StreamLocalAction> allActions = Arrays.asList(action1, action2);
        List<String> validContentIds = Arrays.asList(CONTENT_ID_1, CONTENT_ID_2);

        ArgumentCaptor<JournalMutation> journalMutationCaptor =
                ArgumentCaptor.forClass(JournalMutation.class);

        LocalActionGc localActionGc = new LocalActionGc(
                allActions, validContentIds, journalStorage, timingUtils, DISMISS_JOURNAL_NAME);
        localActionGc.gc();

        verify(journalStorage).commit(journalMutationCaptor.capture());

        JournalMutation journalMutation = journalMutationCaptor.getValue();
        assertThat(journalMutation.getJournalName()).isEqualTo(DISMISS_JOURNAL_NAME);
        List<JournalOperation> journalOperations = journalMutation.getOperations();
        assertThat(journalOperations).hasSize(3);
        assertThat(journalOperations.get(0).getType()).isEqualTo(Type.DELETE);
        assertThat(journalOperations.get(1).getType()).isEqualTo(Type.APPEND);
        assertThat(((Append) journalOperations.get(1)).getValue()).isEqualTo(action1.toByteArray());
        assertThat(journalOperations.get(2).getType()).isEqualTo(Type.APPEND);
        assertThat(((Append) journalOperations.get(2)).getValue()).isEqualTo(action2.toByteArray());
    }

    @Test
    public void gc_allInvalid() throws Exception {
        StreamLocalAction action1 = StreamLocalAction.newBuilder()
                                            .setAction(ActionType.DISMISS)
                                            .setFeatureContentId(CONTENT_ID_1)
                                            .setTimestampSeconds(TimeUnit.DAYS.toSeconds(43))
                                            .build();
        StreamLocalAction action2 = StreamLocalAction.newBuilder()
                                            .setAction(ActionType.DISMISS)
                                            .setFeatureContentId(CONTENT_ID_2)
                                            .setTimestampSeconds(TimeUnit.DAYS.toSeconds(44))
                                            .build();
        List<StreamLocalAction> allActions = Arrays.asList(action1, action2);
        List<String> validContentIds = Collections.emptyList();

        ArgumentCaptor<JournalMutation> journalMutationCaptor =
                ArgumentCaptor.forClass(JournalMutation.class);

        LocalActionGc localActionGc = new LocalActionGc(
                allActions, validContentIds, journalStorage, timingUtils, DISMISS_JOURNAL_NAME);
        localActionGc.gc();

        verify(journalStorage).commit(journalMutationCaptor.capture());

        JournalMutation journalMutation = journalMutationCaptor.getValue();
        assertThat(journalMutation.getJournalName()).isEqualTo(DISMISS_JOURNAL_NAME);
        List<JournalOperation> journalOperations = journalMutation.getOperations();
        assertThat(journalOperations).hasSize(1);
        assertThat(journalOperations.get(0).getType()).isEqualTo(Type.DELETE);
    }

    @Test
    public void gc_someValid() throws Exception {
        StreamLocalAction action1 = StreamLocalAction.newBuilder()
                                            .setAction(ActionType.DISMISS)
                                            .setFeatureContentId(CONTENT_ID_1)
                                            .setTimestampSeconds(TimeUnit.DAYS.toSeconds(43))
                                            .build();
        StreamLocalAction action2 = StreamLocalAction.newBuilder()
                                            .setAction(ActionType.DISMISS)
                                            .setFeatureContentId(CONTENT_ID_2)
                                            .setTimestampSeconds(TimeUnit.DAYS.toSeconds(44))
                                            .build();
        List<StreamLocalAction> allActions = Arrays.asList(action1, action2);
        List<String> validContentIds = Collections.singletonList(CONTENT_ID_2);

        ArgumentCaptor<JournalMutation> journalMutationCaptor =
                ArgumentCaptor.forClass(JournalMutation.class);

        LocalActionGc localActionGc = new LocalActionGc(
                allActions, validContentIds, journalStorage, timingUtils, DISMISS_JOURNAL_NAME);
        localActionGc.gc();

        verify(journalStorage).commit(journalMutationCaptor.capture());

        JournalMutation journalMutation = journalMutationCaptor.getValue();
        assertThat(journalMutation.getJournalName()).isEqualTo(DISMISS_JOURNAL_NAME);
        List<JournalOperation> journalOperations = journalMutation.getOperations();
        assertThat(journalOperations).hasSize(2);
        assertThat(journalOperations.get(0).getType()).isEqualTo(Type.DELETE);
        assertThat(journalOperations.get(1).getType()).isEqualTo(Type.APPEND);
        assertThat(((Append) journalOperations.get(1)).getValue()).isEqualTo(action2.toByteArray());
    }
}
