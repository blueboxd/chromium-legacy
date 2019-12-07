// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.basicstream.internal.drivers;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyListOf;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import android.support.v7.widget.RecyclerView;

import com.google.common.collect.ImmutableList;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.ArgumentMatchers;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.robolectric.annotation.Config;

import org.chromium.base.Supplier;
import org.chromium.chrome.browser.feed.library.api.client.knowncontent.ContentMetadata;
import org.chromium.chrome.browser.feed.library.api.client.stream.Stream.ContentChangedListener;
import org.chromium.chrome.browser.feed.library.api.host.action.ActionApi;
import org.chromium.chrome.browser.feed.library.api.host.action.StreamActionApi;
import org.chromium.chrome.browser.feed.library.api.host.config.Configuration;
import org.chromium.chrome.browser.feed.library.api.host.config.Configuration.ConfigKey;
import org.chromium.chrome.browser.feed.library.api.host.logging.BasicLoggingApi;
import org.chromium.chrome.browser.feed.library.api.host.logging.ContentLoggingData;
import org.chromium.chrome.browser.feed.library.api.host.logging.InternalFeedError;
import org.chromium.chrome.browser.feed.library.api.host.stream.TooltipApi;
import org.chromium.chrome.browser.feed.library.api.internal.actionmanager.ActionManager;
import org.chromium.chrome.browser.feed.library.api.internal.actionparser.ActionParser;
import org.chromium.chrome.browser.feed.library.api.internal.actionparser.ActionParserFactory;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelFeature;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProvider;
import org.chromium.chrome.browser.feed.library.basicstream.internal.actions.StreamActionApiImpl;
import org.chromium.chrome.browser.feed.library.basicstream.internal.actions.ViewElementActionHandler;
import org.chromium.chrome.browser.feed.library.basicstream.internal.pendingdismiss.ClusterPendingDismissHelper;
import org.chromium.chrome.browser.feed.library.basicstream.internal.viewholders.PietViewHolder;
import org.chromium.chrome.browser.feed.library.basicstream.internal.viewloggingupdater.ViewLoggingUpdater;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeMainThreadRunner;
import org.chromium.chrome.browser.feed.library.common.time.testing.FakeClock;
import org.chromium.chrome.browser.feed.library.sharedstream.contextmenumanager.ContextMenuManager;
import org.chromium.chrome.browser.feed.library.sharedstream.logging.LoggingListener;
import org.chromium.chrome.browser.feed.library.sharedstream.logging.StreamContentLoggingData;
import org.chromium.chrome.browser.feed.library.testing.sharedstream.offlinemonitor.FakeStreamOfflineMonitor;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamFeature;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamSharedState;
import org.chromium.components.feed.core.proto.ui.action.FeedActionPayloadProto.FeedActionPayload;
import org.chromium.components.feed.core.proto.ui.action.FeedActionProto.FeedAction;
import org.chromium.components.feed.core.proto.ui.action.FeedActionProto.FeedActionMetadata.ElementType;
import org.chromium.components.feed.core.proto.ui.piet.PietProto.Frame;
import org.chromium.components.feed.core.proto.ui.piet.PietProto.PietSharedState;
import org.chromium.components.feed.core.proto.ui.piet.PietProto.Template;
import org.chromium.components.feed.core.proto.ui.stream.StreamStructureProto.BasicLoggingMetadata;
import org.chromium.components.feed.core.proto.ui.stream.StreamStructureProto.Content;
import org.chromium.components.feed.core.proto.ui.stream.StreamStructureProto.Content.Type;
import org.chromium.components.feed.core.proto.ui.stream.StreamStructureProto.OfflineMetadata;
import org.chromium.components.feed.core.proto.ui.stream.StreamStructureProto.PietContent;
import org.chromium.components.feed.core.proto.ui.stream.StreamStructureProto.RepresentationData;
import org.chromium.components.feed.core.proto.wire.ContentIdProto.ContentId;
import org.chromium.components.feed.core.proto.wire.PietSharedStateItemProto.PietSharedStateItem;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.ArrayList;
import java.util.List;

/** Tests for {@link ContentDriver}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ContentDriverTest {
    private static final long VIEW_MIN_TIME_MS = 200;
    private static final int POSITION = 1;
    private static final String CONTENT_URL = "google.com";
    private static final String CONTENT_TITLE = "title";

    private static final BasicLoggingMetadata BASIC_LOGGING_METADATA =
            BasicLoggingMetadata.newBuilder().setScore(40).build();
    private static final RepresentationData REPRESENTATION_DATA =
            RepresentationData.newBuilder().setUri(CONTENT_URL).setPublishedTimeSeconds(10).build();
    private static final StreamContentLoggingData CONTENT_LOGGING_DATA =
            new StreamContentLoggingData(POSITION, BASIC_LOGGING_METADATA, REPRESENTATION_DATA,
                    /*availableOffline= */ false);
    private static final OfflineMetadata OFFLINE_METADATA =
            OfflineMetadata.newBuilder().setTitle(CONTENT_TITLE).build();

    private static final ContentId CONTENT_ID_1 = ContentId.newBuilder()
                                                          .setContentDomain("piet-shared-state")
                                                          .setId(2)
                                                          .setTable("piet-shared-state")
                                                          .build();
    private static final ContentId CONTENT_ID_2 = ContentId.newBuilder()
                                                          .setContentDomain("piet-shared-state")
                                                          .setId(3)
                                                          .setTable("piet-shared-state")
                                                          .build();
    private static final PietContent PIET_CONTENT =
            PietContent.newBuilder()
                    .setFrame(Frame.newBuilder().setStylesheetId("id"))
                    .addPietSharedStates(CONTENT_ID_1)
                    .addPietSharedStates(CONTENT_ID_2)
                    .build();
    private static final Content CONTENT_NO_OFFLINE_METADATA =
            Content.newBuilder()
                    .setBasicLoggingMetadata(BASIC_LOGGING_METADATA)
                    .setRepresentationData(REPRESENTATION_DATA)
                    .setType(Type.PIET)
                    .setExtension(PietContent.pietContentExtension, PIET_CONTENT)
                    .build();
    private static final Content CONTENT =
            CONTENT_NO_OFFLINE_METADATA.toBuilder().setOfflineMetadata(OFFLINE_METADATA).build();

    private static final PietSharedState PIET_SHARED_STATE_1 =
            PietSharedState.newBuilder()
                    .addTemplates(Template.newBuilder().setTemplateId("1"))
                    .build();
    private static final PietSharedState PIET_SHARED_STATE_2 =
            PietSharedState.newBuilder()
                    .addTemplates(Template.newBuilder().setTemplateId("2"))
                    .build();
    private static final ImmutableList<PietSharedState> PIET_SHARED_STATES =
            ImmutableList.of(PIET_SHARED_STATE_1, PIET_SHARED_STATE_2);

    private static final StreamSharedState STREAM_SHARED_STATE_1 =
            StreamSharedState.newBuilder()
                    .setPietSharedStateItem(PietSharedStateItem.newBuilder()
                                                    .setContentId(CONTENT_ID_1)
                                                    .setPietSharedState(PIET_SHARED_STATE_1))
                    .build();
    private static final StreamSharedState STREAM_SHARED_STATE_2 =
            StreamSharedState.newBuilder()
                    .setPietSharedStateItem(PietSharedStateItem.newBuilder()
                                                    .setContentId(CONTENT_ID_2)
                                                    .setPietSharedState(PIET_SHARED_STATE_2))
                    .build();

    @Mock
    private ActionApi mActionApi;
    @Mock
    private ActionManager mActionManager;
    @Mock
    private ActionParserFactory mActionParserFactory;
    @Mock
    private BasicLoggingApi mBasicLoggingApi;
    @Mock
    private ModelFeature mModelFeature;
    @Mock
    private ModelProvider mModelProvider;
    @Mock
    private ClusterPendingDismissHelper mClusterPendingDismissHelper;
    @Mock
    private PietViewHolder mPietViewHolder;
    @Mock
    private StreamActionApiImpl mStreamActionApi;
    @Mock
    private ContentChangedListener mContentChangedListener;
    @Mock
    private ActionParser mActionParser;
    @Mock
    private ContextMenuManager mContextMenuManager;
    @Mock
    private TooltipApi mTooltipApi;

    private FakeStreamOfflineMonitor mFakeStreamOfflineMonitor;
    private ContentDriver mContentDriver;
    private final FakeClock mFakeClock = new FakeClock();
    private final FakeMainThreadRunner mFakeMainThreadRunner =
            FakeMainThreadRunner.create(mFakeClock);
    private final Configuration mConfiguration =
            new Configuration.Builder().put(ConfigKey.VIEW_MIN_TIME_MS, VIEW_MIN_TIME_MS).build();
    private ViewLoggingUpdater mViewLoggingUpdater;

    @Before
    public void setup() {
        initMocks(this);

        when(mModelFeature.getStreamFeature())
                .thenReturn(StreamFeature.newBuilder().setContent(CONTENT).build());
        when(mModelProvider.getSharedState(CONTENT_ID_1)).thenReturn(STREAM_SHARED_STATE_1);
        when(mModelProvider.getSharedState(CONTENT_ID_2)).thenReturn(STREAM_SHARED_STATE_2);
        when(mActionParserFactory.build(ArgumentMatchers.<Supplier<ContentMetadata>>any()))
                .thenReturn(mActionParser);
        mViewLoggingUpdater = new ViewLoggingUpdater();
        mFakeStreamOfflineMonitor = FakeStreamOfflineMonitor.create();
        mContentDriver = new ContentDriver(mActionApi, mActionManager, mActionParserFactory,
                mBasicLoggingApi, mModelFeature, mModelProvider, POSITION,
                FeedActionPayload.getDefaultInstance(), mClusterPendingDismissHelper,
                mFakeStreamOfflineMonitor, mContentChangedListener, mContextMenuManager,
                mFakeMainThreadRunner, mConfiguration, mViewLoggingUpdater, mTooltipApi) {
            @Override
            StreamActionApiImpl createStreamActionApi(ActionApi actionApi,
                    ActionParser actionParser, ActionManager actionManager,
                    BasicLoggingApi basicLoggingApi,
                    Supplier<ContentLoggingData> contentLoggingData, String sessionId,
                    ContextMenuManager contextMenuManager,
                    ClusterPendingDismissHelper clusterPendingDismissHelper,
                    ViewElementActionHandler handler, String contentId, TooltipApi tooltipApi) {
                return mStreamActionApi;
            }
        };
    }

    @Test
    public void testBind() {
        mContentDriver.bind(mPietViewHolder);

        verify(mPietViewHolder)
                .bind(eq(PIET_CONTENT.getFrame()), eq(PIET_SHARED_STATES), eq(mStreamActionApi),
                        eq(FeedActionPayload.getDefaultInstance()), any(LoggingListener.class),
                        eq(mActionParser));
    }

    @Test
    public void testMaybeRebind() {
        mContentDriver.bind(mPietViewHolder);
        mContentDriver.maybeRebind();
        verify(mPietViewHolder, times(2))
                .bind(eq(PIET_CONTENT.getFrame()), eq(PIET_SHARED_STATES), eq(mStreamActionApi),
                        eq(FeedActionPayload.getDefaultInstance()), any(LoggingListener.class),
                        eq(mActionParser));
        verify(mPietViewHolder).unbind();
    }

    @Test
    public void testMaybeRebind_nullViewHolder() {
        // bind/unbind to associated the pietViewHolder with the contentDriver
        mContentDriver.bind(mPietViewHolder);
        mContentDriver.unbind();
        reset(mPietViewHolder);

        mContentDriver.maybeRebind();
        verify(mPietViewHolder, never())
                .bind(any(Frame.class), ArgumentMatchers.<List<PietSharedState>>any(),
                        any(StreamActionApi.class), any(FeedActionPayload.class),
                        any(LoggingListener.class), any(ActionParser.class));
        verify(mPietViewHolder, never()).unbind();
    }

    @Test
    public void testOnViewVisible_visibilityListenerLogsContentViewed() {
        mContentDriver.bind(mPietViewHolder);

        ArgumentCaptor<LoggingListener> visibilityListenerCaptor =
                ArgumentCaptor.forClass(LoggingListener.class);

        verify(mPietViewHolder)
                .bind(any(Frame.class), anyListOf(PietSharedState.class),
                        any(StreamActionApi.class), any(FeedActionPayload.class),
                        visibilityListenerCaptor.capture(), any(ActionParser.class));
        LoggingListener listener = visibilityListenerCaptor.getValue();
        listener.onViewVisible();

        verify(mBasicLoggingApi).onContentViewed(CONTENT_LOGGING_DATA);
    }

    @Test
    public void testOnViewVisible_resetVisibility_logsTwice() {
        mContentDriver.bind(mPietViewHolder);

        ArgumentCaptor<LoggingListener> visibilityListenerCaptor =
                ArgumentCaptor.forClass(LoggingListener.class);

        verify(mPietViewHolder)
                .bind(any(Frame.class), anyListOf(PietSharedState.class),
                        any(StreamActionApi.class), any(FeedActionPayload.class),
                        visibilityListenerCaptor.capture(), any(ActionParser.class));
        LoggingListener listener = visibilityListenerCaptor.getValue();
        listener.onViewVisible();
        mViewLoggingUpdater.resetViewTracking();
        listener.onViewVisible();

        verify(mBasicLoggingApi, times(2)).onContentViewed(CONTENT_LOGGING_DATA);
    }

    @Test
    public void
    testOnContentClicked_offlineStatusChanges_logsContentClicked_withNewOfflineStatus() {
        mContentDriver.bind(mPietViewHolder);

        ArgumentCaptor<LoggingListener> visibilityListenerCaptor =
                ArgumentCaptor.forClass(LoggingListener.class);

        verify(mPietViewHolder)
                .bind(any(Frame.class), anyListOf(PietSharedState.class),
                        any(StreamActionApi.class), any(FeedActionPayload.class),
                        visibilityListenerCaptor.capture(), any(ActionParser.class));
        LoggingListener listener = visibilityListenerCaptor.getValue();

        mFakeStreamOfflineMonitor.setOfflineStatus(CONTENT_URL, true);

        listener.onContentClicked();

        verify(mBasicLoggingApi)
                .onContentClicked(CONTENT_LOGGING_DATA.createWithOfflineStatus(true));
    }

    @Test
    public void testOnContentClicked_visibilityListenerLogsContentClicked() {
        mContentDriver.bind(mPietViewHolder);

        ArgumentCaptor<LoggingListener> visibilityListenerCaptor =
                ArgumentCaptor.forClass(LoggingListener.class);

        verify(mPietViewHolder)
                .bind(any(Frame.class), anyListOf(PietSharedState.class),
                        any(StreamActionApi.class), any(FeedActionPayload.class),
                        visibilityListenerCaptor.capture(), any(ActionParser.class));
        LoggingListener listener = visibilityListenerCaptor.getValue();
        listener.onContentClicked();

        verify(mBasicLoggingApi).onContentClicked(CONTENT_LOGGING_DATA);
    }

    @Test
    public void testOnContentSwipe_visibilityListenerLogsContentSwiped() {
        mContentDriver.bind(mPietViewHolder);

        ArgumentCaptor<LoggingListener> visibilityListenerCaptor =
                ArgumentCaptor.forClass(LoggingListener.class);

        verify(mPietViewHolder)
                .bind(any(Frame.class), anyListOf(PietSharedState.class),
                        any(StreamActionApi.class), any(FeedActionPayload.class),
                        visibilityListenerCaptor.capture(), any(ActionParser.class));
        LoggingListener listener = visibilityListenerCaptor.getValue();
        listener.onContentSwiped();

        verify(mBasicLoggingApi).onContentSwiped(CONTENT_LOGGING_DATA);
    }

    @Test
    public void testBind_nullSharedStates() {
        reset(mModelProvider);
        when(mModelProvider.getSharedState(CONTENT_ID_1)).thenReturn(null);

        mContentDriver = new ContentDriver(mActionApi, mActionManager, mActionParserFactory,
                mBasicLoggingApi, mModelFeature, mModelProvider, POSITION,
                FeedActionPayload.getDefaultInstance(), mClusterPendingDismissHelper,
                mFakeStreamOfflineMonitor, mContentChangedListener, mContextMenuManager,
                mFakeMainThreadRunner, mConfiguration, mViewLoggingUpdater, mTooltipApi);

        mContentDriver.bind(mPietViewHolder);

        verify(mPietViewHolder)
                .bind(eq(PIET_CONTENT.getFrame()),
                        /* pietSharedStates= */ eq(new ArrayList<>()), any(StreamActionApi.class),
                        eq(FeedActionPayload.getDefaultInstance()), any(LoggingListener.class),
                        any(ActionParser.class));
        verify(mModelProvider).getSharedState(CONTENT_ID_1);
        verify(mBasicLoggingApi).onInternalError(InternalFeedError.NULL_SHARED_STATES);
    }

    @Test
    public void testBind_swipeableWithNonDefaultFeedActionPayload() {
        FeedActionPayload payload = FeedActionPayload.newBuilder()
                                            .setExtension(FeedAction.feedActionExtension,
                                                    FeedAction.getDefaultInstance())
                                            .build();
        mContentDriver = new ContentDriver(mActionApi, mActionManager, mActionParserFactory,
                mBasicLoggingApi, mModelFeature, mModelProvider, POSITION, payload,
                mClusterPendingDismissHelper, mFakeStreamOfflineMonitor, mContentChangedListener,
                mContextMenuManager, mFakeMainThreadRunner, mConfiguration, mViewLoggingUpdater,
                mTooltipApi);

        mContentDriver.bind(mPietViewHolder);

        verify(mPietViewHolder)
                .bind(eq(PIET_CONTENT.getFrame()), eq(PIET_SHARED_STATES),
                        any(StreamActionApi.class), eq(payload), any(LoggingListener.class),
                        any(ActionParser.class));
    }

    @Test
    public void testOfflineStatusChange() {
        mContentDriver = new ContentDriver(mActionApi, mActionManager, mActionParserFactory,
                mBasicLoggingApi, mModelFeature, mModelProvider, POSITION,
                FeedActionPayload.getDefaultInstance(), mClusterPendingDismissHelper,
                mFakeStreamOfflineMonitor, mContentChangedListener, mContextMenuManager,
                mFakeMainThreadRunner, mConfiguration, mViewLoggingUpdater, mTooltipApi);

        mContentDriver.bind(mPietViewHolder);

        reset(mPietViewHolder);

        mFakeStreamOfflineMonitor.setOfflineStatus(CONTENT_URL, true);

        verify(mContentChangedListener).onContentChanged();
        InOrder inOrder = Mockito.inOrder(mPietViewHolder);
        inOrder.verify(mPietViewHolder).unbind();
        inOrder.verify(mPietViewHolder)
                .bind(eq(PIET_CONTENT.getFrame()), eq(PIET_SHARED_STATES),
                        any(StreamActionApi.class), eq(FeedActionPayload.getDefaultInstance()),
                        any(LoggingListener.class), any(ActionParser.class));
    }

    @Test
    public void testOfflineStatusChange_noOpWithoutUpdate() {
        mFakeStreamOfflineMonitor = FakeStreamOfflineMonitor.create();

        mContentDriver = new ContentDriver(mActionApi, mActionManager, mActionParserFactory,
                mBasicLoggingApi, mModelFeature, mModelProvider, POSITION,
                FeedActionPayload.getDefaultInstance(), mClusterPendingDismissHelper,
                mFakeStreamOfflineMonitor, mContentChangedListener, mContextMenuManager,
                mFakeMainThreadRunner, mConfiguration, mViewLoggingUpdater, mTooltipApi);

        mFakeStreamOfflineMonitor.setOfflineStatus(CONTENT_URL, true);
        mContentDriver.bind(mPietViewHolder);

        reset(mPietViewHolder);

        mFakeStreamOfflineMonitor.setOfflineStatus(CONTENT_URL, true);

        verifyNoMoreInteractions(mPietViewHolder, mContentChangedListener);
    }

    @Test
    public void testNotEnoughInformationForContentMetadata() {
        assertThat(getContentMetadataFor(CONTENT_NO_OFFLINE_METADATA)).isNull();
    }

    @Test
    public void testContentMetadataProvider() {
        ContentMetadata contentMetadata = getContentMetadataFor(CONTENT);

        assertThat(contentMetadata).isNotNull();
        assertThat(contentMetadata.getTitle()).isEqualTo(CONTENT_TITLE);
        assertThat(contentMetadata.getUrl()).isEqualTo(CONTENT_URL);
    }

    @Test
    public void testUnbind() {
        mContentDriver.bind(mPietViewHolder);

        mContentDriver.unbind();

        verify(mPietViewHolder).unbind();
        assertThat(mContentDriver.isBound()).isFalse();
    }

    @Test
    public void testOnElementView_logsElementViewed() {
        mContentDriver.onElementView(ElementType.INTEREST_HEADER.getNumber());
        mFakeClock.advance(VIEW_MIN_TIME_MS);

        verify(mBasicLoggingApi)
                .onVisualElementViewed(
                        any(ContentLoggingData.class), eq(ElementType.INTEREST_HEADER.getNumber()));
    }

    @Test
    public void testOnElementView_logsElementViewed_usingOfflineStatusWhenLogging() {
        mContentDriver.onElementView(ElementType.INTEREST_HEADER.getNumber());

        // When the view first happens, the offline status is false.
        mFakeClock.advance(VIEW_MIN_TIME_MS / 2);

        // When waiting to log the view, the offline status switches to true
        mFakeStreamOfflineMonitor.setOfflineStatus(CONTENT_URL, true);

        // Advance so that the event is logged
        mFakeClock.advance(VIEW_MIN_TIME_MS / 2);

        // Should be logged with offline status true.
        verify(mBasicLoggingApi)
                .onVisualElementViewed(eq(CONTENT_LOGGING_DATA.createWithOfflineStatus(true)),
                        eq(ElementType.INTEREST_HEADER.getNumber()));
    }

    @Test
    public void testOnElementView_logsElementViewed_beforeTimeout() {
        mContentDriver.onElementView(ElementType.INTEREST_HEADER.getNumber());
        mFakeClock.advance(VIEW_MIN_TIME_MS - 1);

        verify(mBasicLoggingApi, never())
                .onVisualElementViewed(
                        any(ContentLoggingData.class), eq(ElementType.INTEREST_HEADER.getNumber()));
    }

    @Test
    public void testOnElementView_logsElementViewedThenHidden() {
        mContentDriver.onElementView(ElementType.INTEREST_HEADER.getNumber());
        mContentDriver.onElementHide(ElementType.INTEREST_HEADER.getNumber());
        mFakeClock.advance(VIEW_MIN_TIME_MS);

        verify(mBasicLoggingApi, never())
                .onVisualElementViewed(
                        any(ContentLoggingData.class), eq(ElementType.INTEREST_HEADER.getNumber()));
    }

    @Test
    public void testOnElementView_logsElementViewedThenHiddenThenViewed() {
        mContentDriver.onElementView(ElementType.INTEREST_HEADER.getNumber());
        mContentDriver.onElementHide(ElementType.INTEREST_HEADER.getNumber());
        mFakeClock.advance(VIEW_MIN_TIME_MS - 1);
        mContentDriver.onElementView(ElementType.INTEREST_HEADER.getNumber());
        mFakeClock.advance(VIEW_MIN_TIME_MS);
        // Check that it only gets recorded once.
        verify(mBasicLoggingApi)
                .onVisualElementViewed(
                        any(ContentLoggingData.class), eq(ElementType.INTEREST_HEADER.getNumber()));
    }

    @Test
    public void testOnElementView_logsElementViewedMultiple_differentTypes() {
        mContentDriver.onElementView(ElementType.INTEREST_HEADER.getNumber());
        mContentDriver.onElementView(ElementType.CARD_LARGE_IMAGE.getNumber());
        mFakeClock.advance(VIEW_MIN_TIME_MS);

        verify(mBasicLoggingApi)
                .onVisualElementViewed(
                        any(ContentLoggingData.class), eq(ElementType.INTEREST_HEADER.getNumber()));
        verify(mBasicLoggingApi)
                .onVisualElementViewed(any(ContentLoggingData.class),
                        eq(ElementType.CARD_LARGE_IMAGE.getNumber()));
    }

    @Test
    public void testOnElementView_logsElementViewedMultiple_sameTypes() {
        mContentDriver.onElementView(ElementType.INTEREST_HEADER.getNumber());
        mContentDriver.onElementView(ElementType.INTEREST_HEADER.getNumber());
        mFakeClock.advance(VIEW_MIN_TIME_MS);

        // Make sure it only logs one view.
        verify(mBasicLoggingApi)
                .onVisualElementViewed(
                        any(ContentLoggingData.class), eq(ElementType.INTEREST_HEADER.getNumber()));
    }

    @Test
    public void testOnScrollStateChanged() {
        mContentDriver.onElementView(ElementType.INTEREST_HEADER.getNumber());
        mContentDriver.onScrollStateChanged(RecyclerView.SCROLL_STATE_DRAGGING);
        mFakeClock.advance(VIEW_MIN_TIME_MS);
        verify(mBasicLoggingApi, never())
                .onVisualElementViewed(
                        any(ContentLoggingData.class), eq(ElementType.INTEREST_HEADER.getNumber()));
    }

    @Test
    public void testOnScrollStateChanged_idle() {
        mContentDriver.onElementView(ElementType.INTEREST_HEADER.getNumber());
        mContentDriver.onScrollStateChanged(RecyclerView.SCROLL_STATE_IDLE);
        mFakeClock.advance(VIEW_MIN_TIME_MS);
        verify(mBasicLoggingApi)
                .onVisualElementViewed(
                        any(ContentLoggingData.class), eq(ElementType.INTEREST_HEADER.getNumber()));
    }

    @Test
    public void testOnDestroy_cancelsTasks() {
        mContentDriver.onElementView(ElementType.INTEREST_HEADER.getNumber());
        mContentDriver.onDestroy();
        mFakeClock.advance(VIEW_MIN_TIME_MS);
        verify(mBasicLoggingApi, never())
                .onVisualElementViewed(
                        any(ContentLoggingData.class), eq(ElementType.INTEREST_HEADER.getNumber()));
    }

    /** For Captor of generic. */
    @SuppressWarnings("unchecked")
    /*@Nullable*/
    private ContentMetadata getContentMetadataFor(Content content) {
        when(mModelFeature.getStreamFeature())
                .thenReturn(StreamFeature.newBuilder().setContent(content).build());

        reset(mActionParserFactory);
        when(mActionParserFactory.build(ArgumentMatchers.<Supplier<ContentMetadata>>any()))
                .thenReturn(mActionParser);

        mContentDriver = new ContentDriver(mActionApi, mActionManager, mActionParserFactory,
                mBasicLoggingApi, mModelFeature, mModelProvider, POSITION,
                FeedActionPayload.getDefaultInstance(), mClusterPendingDismissHelper,
                mFakeStreamOfflineMonitor, mContentChangedListener, mContextMenuManager,
                mFakeMainThreadRunner, mConfiguration, mViewLoggingUpdater, mTooltipApi);

        ArgumentCaptor<Supplier<ContentMetadata>> contentMetadataSupplierCaptor =
                ArgumentCaptor.forClass((Class<Supplier<ContentMetadata>>) (Class) Supplier.class);

        verify(mActionParserFactory).build(contentMetadataSupplierCaptor.capture());

        return contentMetadataSupplierCaptor.getValue().get();
    }
}
