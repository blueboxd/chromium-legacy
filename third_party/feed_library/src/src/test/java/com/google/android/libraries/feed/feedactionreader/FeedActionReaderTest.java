// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package com.google.android.libraries.feed.feedactionreader;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.anyDouble;
import static org.mockito.ArgumentMatchers.anyList;
import static org.mockito.ArgumentMatchers.anyListOf;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.same;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import com.google.android.libraries.feed.api.host.config.Configuration;
import com.google.android.libraries.feed.api.host.config.Configuration.ConfigKey;
import com.google.android.libraries.feed.api.internal.actionmanager.ActionReader;
import com.google.android.libraries.feed.api.internal.common.DismissActionWithSemanticProperties;
import com.google.android.libraries.feed.api.internal.common.SemanticPropertiesWithId;
import com.google.android.libraries.feed.api.internal.common.testing.ContentIdGenerators;
import com.google.android.libraries.feed.api.internal.protocoladapter.ProtocolAdapter;
import com.google.android.libraries.feed.api.internal.store.LocalActionMutation.ActionType;
import com.google.android.libraries.feed.api.internal.store.Store;
import com.google.android.libraries.feed.common.Result;
import com.google.android.libraries.feed.common.concurrent.testing.FakeTaskQueue;
import com.google.android.libraries.feed.common.concurrent.testing.FakeThreadUtils;
import com.google.android.libraries.feed.common.testing.ResponseBuilder;
import com.google.android.libraries.feed.common.time.testing.FakeClock;
import com.google.search.now.feed.client.StreamDataProto.StreamLocalAction;
import com.google.search.now.wire.feed.ContentIdProto.ContentId;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.RobolectricTestRunner;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.TimeUnit;

/** Tests of the {@link FeedActionReader} class. */
@RunWith(RobolectricTestRunner.class)
public class FeedActionReaderTest {
    private static final ContentIdGenerators ID_GENERATOR = new ContentIdGenerators();

    private static final ContentId CONTENT_ID = ResponseBuilder.createFeatureContentId(1);
    private static final String CONTENT_ID_STRING = ID_GENERATOR.createContentId(CONTENT_ID);
    private static final ContentId CONTENT_ID_2 = ResponseBuilder.createFeatureContentId(2);
    private static final String CONTENT_ID_STRING_2 = ID_GENERATOR.createContentId(CONTENT_ID_2);
    private static final long DEFAULT_TIME = TimeUnit.DAYS.toSeconds(42);

    private final FakeClock fakeClock = new FakeClock();
    private final FakeThreadUtils fakeThreadUtils = FakeThreadUtils.withThreadChecks();

    @Mock
    private Store store;
    @Mock
    private ProtocolAdapter protocolAdapter;
    @Mock
    private Configuration configuration;

    private ActionReader actionReader;

    @Before
    public void setUp() throws Exception {
        initMocks(this);

        when(configuration.getValueOrDefault(same(ConfigKey.DEFAULT_ACTION_TTL_SECONDS), anyLong()))
                .thenReturn(TimeUnit.DAYS.toSeconds(3));
        when(configuration.getValueOrDefault(
                     same(ConfigKey.MINIMUM_VALID_ACTION_RATIO), anyDouble()))
                .thenReturn(0.9);

        when(store.triggerLocalActionGc(
                     anyListOf(StreamLocalAction.class), anyListOf(String.class)))
                .thenReturn(() -> {});

        actionReader = new FeedActionReader(
                store, fakeClock, protocolAdapter, getTaskQueue(), configuration);

        when(protocolAdapter.getWireContentId(CONTENT_ID_STRING))
                .thenReturn(Result.success(CONTENT_ID));
        when(protocolAdapter.getWireContentId(CONTENT_ID_STRING_2))
                .thenReturn(Result.success(CONTENT_ID_2));
    }

    @Test
    public void getAllDismissedActions() {
        fakeClock.set(DEFAULT_TIME);

        StreamLocalAction dismissAction = buildDismissAction(CONTENT_ID_STRING);
        mockStoreCalls(Collections.singletonList(dismissAction), Collections.emptyList());

        Result<List<DismissActionWithSemanticProperties>> dismissActionsResult =
                actionReader.getDismissActionsWithSemanticProperties();
        assertThat(dismissActionsResult.isSuccessful()).isTrue();
        List<DismissActionWithSemanticProperties> dismissActions = dismissActionsResult.getValue();
        assertThat(dismissActions)
                .containsExactly(new DismissActionWithSemanticProperties(CONTENT_ID, null));
    }

    @Test
    public void getAllDismissedActions_empty() {
        fakeClock.set(DEFAULT_TIME);

        mockStoreCalls(Collections.emptyList(), Collections.emptyList());

        Result<List<DismissActionWithSemanticProperties>> dismissActionsResult =
                actionReader.getDismissActionsWithSemanticProperties();
        assertThat(dismissActionsResult.isSuccessful()).isTrue();
        List<DismissActionWithSemanticProperties> dismissActions = dismissActionsResult.getValue();
        assertThat(dismissActions).hasSize(0);
        verify(store, never())
                .triggerLocalActionGc(anyListOf(StreamLocalAction.class), anyListOf(String.class));
    }

    @Test
    public void getAllDismissedActions_storeError_getAllDismissActions() {
        fakeClock.set(DEFAULT_TIME);
        when(store.getAllDismissLocalActions()).thenReturn(Result.failure());

        Result<List<DismissActionWithSemanticProperties>> dismissActionsResult =
                actionReader.getDismissActionsWithSemanticProperties();
        assertThat(dismissActionsResult.isSuccessful()).isFalse();
        verify(store, never())
                .triggerLocalActionGc(anyListOf(StreamLocalAction.class), anyListOf(String.class));
    }

    @Test
    public void getAllDismissedActions_storeError_getSemanticProperties() {
        fakeClock.set(DEFAULT_TIME);
        StreamLocalAction dismissAction = buildDismissAction(CONTENT_ID_STRING);
        when(store.getAllDismissLocalActions())
                .thenReturn(Result.success(Collections.singletonList(dismissAction)));
        when(store.getSemanticProperties(anyList())).thenReturn(Result.failure());

        Result<List<DismissActionWithSemanticProperties>> dismissActionsResult =
                actionReader.getDismissActionsWithSemanticProperties();
        assertThat(dismissActionsResult.isSuccessful()).isFalse();
        verify(store, never())
                .triggerLocalActionGc(anyListOf(StreamLocalAction.class), anyListOf(String.class));
    }

    @Test
    public void getAllDismissedActions_expired() {
        fakeClock.set(TimeUnit.SECONDS.toMillis(DEFAULT_TIME) + TimeUnit.DAYS.toMillis(3));

        StreamLocalAction dismissAction = buildDismissAction(CONTENT_ID_STRING);
        List<StreamLocalAction> dismissActions = Collections.singletonList(dismissAction);
        mockStoreCalls(dismissActions, Collections.emptyList());

        Result<List<DismissActionWithSemanticProperties>> dismissActionsResult =
                actionReader.getDismissActionsWithSemanticProperties();
        assertThat(dismissActionsResult.isSuccessful()).isTrue();
        assertThat(dismissActionsResult.getValue()).hasSize(0);
        verify(store).triggerLocalActionGc(eq(dismissActions), anyListOf(String.class));
    }

    @Test
    public void getAllDismissedActions_semanticProperties() {
        fakeClock.set(DEFAULT_TIME);

        StreamLocalAction dismissAction = buildDismissAction(CONTENT_ID_STRING);
        byte[] semanticData = {12, 41};
        mockStoreCalls(Collections.singletonList(dismissAction),
                Collections.singletonList(
                        new SemanticPropertiesWithId(CONTENT_ID_STRING, semanticData)));

        Result<List<DismissActionWithSemanticProperties>> dismissActionsResult =
                actionReader.getDismissActionsWithSemanticProperties();
        assertThat(dismissActionsResult.isSuccessful()).isTrue();
        List<DismissActionWithSemanticProperties> dismissActions = dismissActionsResult.getValue();
        assertThat(dismissActions)
                .containsExactly(new DismissActionWithSemanticProperties(CONTENT_ID, semanticData));
        verify(store, never())
                .triggerLocalActionGc(anyListOf(StreamLocalAction.class), anyListOf(String.class));
    }

    @Test
    public void getAllDismissedActions_multipleActions() {
        fakeClock.set(DEFAULT_TIME);

        StreamLocalAction dismissAction = buildDismissAction(CONTENT_ID_STRING);
        StreamLocalAction dismissAction2 = buildDismissAction(CONTENT_ID_STRING_2);
        mockStoreCalls(Arrays.asList(dismissAction, dismissAction2), Collections.emptyList());

        Result<List<DismissActionWithSemanticProperties>> dismissActionsResult =
                actionReader.getDismissActionsWithSemanticProperties();
        assertThat(dismissActionsResult.isSuccessful()).isTrue();
        List<DismissActionWithSemanticProperties> dismissActions = dismissActionsResult.getValue();

        assertThat(dismissActions)
                .containsExactly(new DismissActionWithSemanticProperties(CONTENT_ID, null),
                        new DismissActionWithSemanticProperties(CONTENT_ID_2, null));
        verify(store, never())
                .triggerLocalActionGc(anyListOf(StreamLocalAction.class), anyListOf(String.class));
    }

    @Test
    public void getAllDismissedActions_multipleActions_semanticProperties() {
        fakeClock.set(DEFAULT_TIME);

        StreamLocalAction dismissAction = buildDismissAction(CONTENT_ID_STRING);
        StreamLocalAction dismissAction2 = buildDismissAction(CONTENT_ID_STRING_2);
        byte[] semanticData = {12, 41};
        byte[] semanticData2 = {42, 72};
        mockStoreCalls(Arrays.asList(dismissAction, dismissAction2),
                Arrays.asList(new SemanticPropertiesWithId(CONTENT_ID_STRING, semanticData),
                        new SemanticPropertiesWithId(CONTENT_ID_STRING_2, semanticData2)));

        Result<List<DismissActionWithSemanticProperties>> dismissActionsResult =
                actionReader.getDismissActionsWithSemanticProperties();
        assertThat(dismissActionsResult.isSuccessful()).isTrue();
        List<DismissActionWithSemanticProperties> dismissActions = dismissActionsResult.getValue();

        assertThat(dismissActions)
                .containsExactly(new DismissActionWithSemanticProperties(CONTENT_ID, semanticData),
                        new DismissActionWithSemanticProperties(CONTENT_ID_2, semanticData2));
        verify(store, never())
                .triggerLocalActionGc(anyListOf(StreamLocalAction.class), anyListOf(String.class));
    }

    @Test
    public void getAllDismissedActions_multipleActions_someExpired() {
        fakeClock.set(TimeUnit.SECONDS.toMillis(DEFAULT_TIME) + TimeUnit.DAYS.toMillis(3));

        StreamLocalAction dismissAction = buildDismissAction(CONTENT_ID_STRING);
        StreamLocalAction dismissAction2 =
                StreamLocalAction.newBuilder()
                        .setAction(ActionType.DISMISS)
                        .setFeatureContentId(CONTENT_ID_STRING_2)
                        .setTimestampSeconds(DEFAULT_TIME + TimeUnit.DAYS.toSeconds(2))
                        .build();
        mockStoreCalls(Arrays.asList(dismissAction, dismissAction2), Collections.emptyList());

        Result<List<DismissActionWithSemanticProperties>> dismissActionsResult =
                actionReader.getDismissActionsWithSemanticProperties();
        assertThat(dismissActionsResult.isSuccessful()).isTrue();

        assertThat(dismissActionsResult.getValue())
                .containsExactly(new DismissActionWithSemanticProperties(CONTENT_ID_2, null));
        verify(store).triggerLocalActionGc(Arrays.asList(dismissAction, dismissAction2),
                Collections.singletonList(CONTENT_ID_STRING_2));
    }

    @Test
    public void getAllDismissedActions_duplicateActions() {
        fakeClock.set(DEFAULT_TIME);

        StreamLocalAction dismissAction = buildDismissAction(CONTENT_ID_STRING);
        StreamLocalAction dismissAction2 = buildDismissAction(CONTENT_ID_STRING);
        mockStoreCalls(Arrays.asList(dismissAction, dismissAction2), Collections.emptyList());

        Result<List<DismissActionWithSemanticProperties>> dismissActionsResult =
                actionReader.getDismissActionsWithSemanticProperties();
        assertThat(dismissActionsResult.isSuccessful()).isTrue();
        List<DismissActionWithSemanticProperties> dismissActions = dismissActionsResult.getValue();

        assertThat(dismissActions).hasSize(1);
        assertThat(dismissActions)
                .containsExactly(new DismissActionWithSemanticProperties(CONTENT_ID, null));
    }

    private StreamLocalAction buildDismissAction(String contentId) {
        return StreamLocalAction.newBuilder()
                .setAction(ActionType.DISMISS)
                .setFeatureContentId(contentId)
                .setTimestampSeconds(DEFAULT_TIME)
                .build();
    }

    private void mockStoreCalls(List<StreamLocalAction> dismissActions,
            List<SemanticPropertiesWithId> semanticProperties) {
        when(store.getAllDismissLocalActions()).thenReturn(Result.success(dismissActions));
        when(store.getSemanticProperties(anyList())).thenReturn(Result.success(semanticProperties));
    }

    private FakeTaskQueue getTaskQueue() {
        FakeTaskQueue fakeTaskQueue = new FakeTaskQueue(fakeClock, fakeThreadUtils);
        fakeTaskQueue.initialize(() -> {});
        return fakeTaskQueue;
    }
}
