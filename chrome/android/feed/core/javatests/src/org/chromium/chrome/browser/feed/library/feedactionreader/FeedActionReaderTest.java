// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.feed.library.feedactionreader;

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

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.host.config.Configuration;
import org.chromium.chrome.browser.feed.library.api.host.config.Configuration.ConfigKey;
import org.chromium.chrome.browser.feed.library.api.internal.actionmanager.ActionReader;
import org.chromium.chrome.browser.feed.library.api.internal.common.DismissActionWithSemanticProperties;
import org.chromium.chrome.browser.feed.library.api.internal.common.SemanticPropertiesWithId;
import org.chromium.chrome.browser.feed.library.api.internal.common.testing.ContentIdGenerators;
import org.chromium.chrome.browser.feed.library.api.internal.protocoladapter.ProtocolAdapter;
import org.chromium.chrome.browser.feed.library.api.internal.store.LocalActionMutation.ActionType;
import org.chromium.chrome.browser.feed.library.api.internal.store.Store;
import org.chromium.chrome.browser.feed.library.common.Result;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeTaskQueue;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeThreadUtils;
import org.chromium.chrome.browser.feed.library.common.testing.ResponseBuilder;
import org.chromium.chrome.browser.feed.library.common.time.testing.FakeClock;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamLocalAction;
import org.chromium.components.feed.core.proto.wire.ContentIdProto.ContentId;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.TimeUnit;

/** Tests of the {@link FeedActionReader} class. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FeedActionReaderTest {
    private static final ContentIdGenerators ID_GENERATOR = new ContentIdGenerators();

    private static final ContentId CONTENT_ID = ResponseBuilder.createFeatureContentId(1);
    private static final String CONTENT_ID_STRING = ID_GENERATOR.createContentId(CONTENT_ID);
    private static final ContentId CONTENT_ID_2 = ResponseBuilder.createFeatureContentId(2);
    private static final String CONTENT_ID_STRING_2 = ID_GENERATOR.createContentId(CONTENT_ID_2);
    private static final long DEFAULT_TIME = TimeUnit.DAYS.toSeconds(42);

    private final FakeClock mFakeClock = new FakeClock();
    private final FakeThreadUtils mFakeThreadUtils = FakeThreadUtils.withThreadChecks();

    @Mock
    private Store mStore;
    @Mock
    private ProtocolAdapter mProtocolAdapter;
    @Mock
    private Configuration mConfiguration;

    private ActionReader mActionReader;

    @Before
    public void setUp() throws Exception {
        initMocks(this);

        when(mConfiguration.getValueOrDefault(
                     same(ConfigKey.DEFAULT_ACTION_TTL_SECONDS), anyLong()))
                .thenReturn(TimeUnit.DAYS.toSeconds(3));
        when(mConfiguration.getValueOrDefault(
                     same(ConfigKey.MINIMUM_VALID_ACTION_RATIO), anyDouble()))
                .thenReturn(0.9);

        when(mStore.triggerLocalActionGc(
                     anyListOf(StreamLocalAction.class), anyListOf(String.class)))
                .thenReturn(() -> {});

        mActionReader = new FeedActionReader(
                mStore, mFakeClock, mProtocolAdapter, getTaskQueue(), mConfiguration);

        when(mProtocolAdapter.getWireContentId(CONTENT_ID_STRING))
                .thenReturn(Result.success(CONTENT_ID));
        when(mProtocolAdapter.getWireContentId(CONTENT_ID_STRING_2))
                .thenReturn(Result.success(CONTENT_ID_2));
    }

    @Test
    public void getAllDismissedActions() {
        mFakeClock.set(DEFAULT_TIME);

        StreamLocalAction dismissAction = buildDismissAction(CONTENT_ID_STRING);
        mockStoreCalls(Collections.singletonList(dismissAction), Collections.emptyList());

        Result<List<DismissActionWithSemanticProperties>> dismissActionsResult =
                mActionReader.getDismissActionsWithSemanticProperties();
        assertThat(dismissActionsResult.isSuccessful()).isTrue();
        List<DismissActionWithSemanticProperties> dismissActions = dismissActionsResult.getValue();
        assertThat(dismissActions)
                .containsExactly(new DismissActionWithSemanticProperties(CONTENT_ID, null));
    }

    @Test
    public void getAllDismissedActions_empty() {
        mFakeClock.set(DEFAULT_TIME);

        mockStoreCalls(Collections.emptyList(), Collections.emptyList());

        Result<List<DismissActionWithSemanticProperties>> dismissActionsResult =
                mActionReader.getDismissActionsWithSemanticProperties();
        assertThat(dismissActionsResult.isSuccessful()).isTrue();
        List<DismissActionWithSemanticProperties> dismissActions = dismissActionsResult.getValue();
        assertThat(dismissActions).hasSize(0);
        verify(mStore, never())
                .triggerLocalActionGc(anyListOf(StreamLocalAction.class), anyListOf(String.class));
    }

    @Test
    public void getAllDismissedActions_storeError_getAllDismissActions() {
        mFakeClock.set(DEFAULT_TIME);
        when(mStore.getAllDismissLocalActions()).thenReturn(Result.failure());

        Result<List<DismissActionWithSemanticProperties>> dismissActionsResult =
                mActionReader.getDismissActionsWithSemanticProperties();
        assertThat(dismissActionsResult.isSuccessful()).isFalse();
        verify(mStore, never())
                .triggerLocalActionGc(anyListOf(StreamLocalAction.class), anyListOf(String.class));
    }

    @Test
    public void getAllDismissedActions_storeError_getSemanticProperties() {
        mFakeClock.set(DEFAULT_TIME);
        StreamLocalAction dismissAction = buildDismissAction(CONTENT_ID_STRING);
        when(mStore.getAllDismissLocalActions())
                .thenReturn(Result.success(Collections.singletonList(dismissAction)));
        when(mStore.getSemanticProperties(anyList())).thenReturn(Result.failure());

        Result<List<DismissActionWithSemanticProperties>> dismissActionsResult =
                mActionReader.getDismissActionsWithSemanticProperties();
        assertThat(dismissActionsResult.isSuccessful()).isFalse();
        verify(mStore, never())
                .triggerLocalActionGc(anyListOf(StreamLocalAction.class), anyListOf(String.class));
    }

    @Test
    public void getAllDismissedActions_expired() {
        mFakeClock.set(TimeUnit.SECONDS.toMillis(DEFAULT_TIME) + TimeUnit.DAYS.toMillis(3));

        StreamLocalAction dismissAction = buildDismissAction(CONTENT_ID_STRING);
        List<StreamLocalAction> dismissActions = Collections.singletonList(dismissAction);
        mockStoreCalls(dismissActions, Collections.emptyList());

        Result<List<DismissActionWithSemanticProperties>> dismissActionsResult =
                mActionReader.getDismissActionsWithSemanticProperties();
        assertThat(dismissActionsResult.isSuccessful()).isTrue();
        assertThat(dismissActionsResult.getValue()).hasSize(0);
        verify(mStore).triggerLocalActionGc(eq(dismissActions), anyListOf(String.class));
    }

    @Test
    public void getAllDismissedActions_semanticProperties() {
        mFakeClock.set(DEFAULT_TIME);

        StreamLocalAction dismissAction = buildDismissAction(CONTENT_ID_STRING);
        byte[] semanticData = {12, 41};
        mockStoreCalls(Collections.singletonList(dismissAction),
                Collections.singletonList(
                        new SemanticPropertiesWithId(CONTENT_ID_STRING, semanticData)));

        Result<List<DismissActionWithSemanticProperties>> dismissActionsResult =
                mActionReader.getDismissActionsWithSemanticProperties();
        assertThat(dismissActionsResult.isSuccessful()).isTrue();
        List<DismissActionWithSemanticProperties> dismissActions = dismissActionsResult.getValue();
        assertThat(dismissActions)
                .containsExactly(new DismissActionWithSemanticProperties(CONTENT_ID, semanticData));
        verify(mStore, never())
                .triggerLocalActionGc(anyListOf(StreamLocalAction.class), anyListOf(String.class));
    }

    @Test
    public void getAllDismissedActions_multipleActions() {
        mFakeClock.set(DEFAULT_TIME);

        StreamLocalAction dismissAction = buildDismissAction(CONTENT_ID_STRING);
        StreamLocalAction dismissAction2 = buildDismissAction(CONTENT_ID_STRING_2);
        mockStoreCalls(Arrays.asList(dismissAction, dismissAction2), Collections.emptyList());

        Result<List<DismissActionWithSemanticProperties>> dismissActionsResult =
                mActionReader.getDismissActionsWithSemanticProperties();
        assertThat(dismissActionsResult.isSuccessful()).isTrue();
        List<DismissActionWithSemanticProperties> dismissActions = dismissActionsResult.getValue();

        assertThat(dismissActions)
                .containsExactly(new DismissActionWithSemanticProperties(CONTENT_ID, null),
                        new DismissActionWithSemanticProperties(CONTENT_ID_2, null));
        verify(mStore, never())
                .triggerLocalActionGc(anyListOf(StreamLocalAction.class), anyListOf(String.class));
    }

    @Test
    public void getAllDismissedActions_multipleActions_semanticProperties() {
        mFakeClock.set(DEFAULT_TIME);

        StreamLocalAction dismissAction = buildDismissAction(CONTENT_ID_STRING);
        StreamLocalAction dismissAction2 = buildDismissAction(CONTENT_ID_STRING_2);
        byte[] semanticData = {12, 41};
        byte[] semanticData2 = {42, 72};
        mockStoreCalls(Arrays.asList(dismissAction, dismissAction2),
                Arrays.asList(new SemanticPropertiesWithId(CONTENT_ID_STRING, semanticData),
                        new SemanticPropertiesWithId(CONTENT_ID_STRING_2, semanticData2)));

        Result<List<DismissActionWithSemanticProperties>> dismissActionsResult =
                mActionReader.getDismissActionsWithSemanticProperties();
        assertThat(dismissActionsResult.isSuccessful()).isTrue();
        List<DismissActionWithSemanticProperties> dismissActions = dismissActionsResult.getValue();

        assertThat(dismissActions)
                .containsExactly(new DismissActionWithSemanticProperties(CONTENT_ID, semanticData),
                        new DismissActionWithSemanticProperties(CONTENT_ID_2, semanticData2));
        verify(mStore, never())
                .triggerLocalActionGc(anyListOf(StreamLocalAction.class), anyListOf(String.class));
    }

    @Test
    public void getAllDismissedActions_multipleActions_someExpired() {
        mFakeClock.set(TimeUnit.SECONDS.toMillis(DEFAULT_TIME) + TimeUnit.DAYS.toMillis(3));

        StreamLocalAction dismissAction = buildDismissAction(CONTENT_ID_STRING);
        StreamLocalAction dismissAction2 =
                StreamLocalAction.newBuilder()
                        .setAction(ActionType.DISMISS)
                        .setFeatureContentId(CONTENT_ID_STRING_2)
                        .setTimestampSeconds(DEFAULT_TIME + TimeUnit.DAYS.toSeconds(2))
                        .build();
        mockStoreCalls(Arrays.asList(dismissAction, dismissAction2), Collections.emptyList());

        Result<List<DismissActionWithSemanticProperties>> dismissActionsResult =
                mActionReader.getDismissActionsWithSemanticProperties();
        assertThat(dismissActionsResult.isSuccessful()).isTrue();

        assertThat(dismissActionsResult.getValue())
                .containsExactly(new DismissActionWithSemanticProperties(CONTENT_ID_2, null));
        verify(mStore).triggerLocalActionGc(Arrays.asList(dismissAction, dismissAction2),
                Collections.singletonList(CONTENT_ID_STRING_2));
    }

    @Test
    public void getAllDismissedActions_duplicateActions() {
        mFakeClock.set(DEFAULT_TIME);

        StreamLocalAction dismissAction = buildDismissAction(CONTENT_ID_STRING);
        StreamLocalAction dismissAction2 = buildDismissAction(CONTENT_ID_STRING);
        mockStoreCalls(Arrays.asList(dismissAction, dismissAction2), Collections.emptyList());

        Result<List<DismissActionWithSemanticProperties>> dismissActionsResult =
                mActionReader.getDismissActionsWithSemanticProperties();
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
        when(mStore.getAllDismissLocalActions()).thenReturn(Result.success(dismissActions));
        when(mStore.getSemanticProperties(anyList()))
                .thenReturn(Result.success(semanticProperties));
    }

    private FakeTaskQueue getTaskQueue() {
        FakeTaskQueue fakeTaskQueue = new FakeTaskQueue(mFakeClock, mFakeThreadUtils);
        fakeTaskQueue.initialize(() -> {});
        return fakeTaskQueue;
    }
}
