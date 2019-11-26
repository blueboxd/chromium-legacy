// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.feedrequestmanager;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.extensions.proto.LiteProtoTruth.assertThat;

import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import android.app.Activity;
import android.content.Context;
import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.util.Base64;

import com.google.android.libraries.feed.api.host.config.ApplicationInfo;
import com.google.android.libraries.feed.api.host.config.Configuration;
import com.google.android.libraries.feed.api.host.config.Configuration.ConfigKey;
import com.google.android.libraries.feed.api.host.logging.RequestReason;
import com.google.android.libraries.feed.api.host.network.HttpRequest;
import com.google.android.libraries.feed.api.host.network.HttpRequest.HttpMethod;
import com.google.android.libraries.feed.api.host.network.HttpResponse;
import com.google.android.libraries.feed.api.host.scheduler.SchedulerApi;
import com.google.android.libraries.feed.api.host.stream.TooltipInfo.FeatureName;
import com.google.android.libraries.feed.api.internal.common.DismissActionWithSemanticProperties;
import com.google.android.libraries.feed.api.internal.common.Model;
import com.google.android.libraries.feed.common.Result;
import com.google.android.libraries.feed.common.concurrent.testing.FakeMainThreadRunner;
import com.google.android.libraries.feed.common.concurrent.testing.FakeTaskQueue;
import com.google.android.libraries.feed.common.concurrent.testing.FakeThreadUtils;
import com.google.android.libraries.feed.common.locale.LocaleUtils;
import com.google.android.libraries.feed.common.protoextensions.FeedExtensionRegistry;
import com.google.android.libraries.feed.common.testing.RequiredConsumer;
import com.google.android.libraries.feed.common.time.TimingUtils;
import com.google.android.libraries.feed.common.time.testing.FakeClock;
import com.google.android.libraries.feed.testing.actionmanager.FakeActionReader;
import com.google.android.libraries.feed.testing.host.logging.FakeBasicLoggingApi;
import com.google.android.libraries.feed.testing.host.stream.FakeTooltipSupportedApi;
import com.google.android.libraries.feed.testing.network.FakeNetworkClient;
import com.google.android.libraries.feed.testing.protocoladapter.FakeProtocolAdapter;
import com.google.protobuf.ByteString;
import com.google.protobuf.CodedOutputStream;
import com.google.protobuf.ExtensionRegistryLite;
import com.google.search.now.feed.client.StreamDataProto.StreamToken;
import com.google.search.now.wire.feed.ActionTypeProto.ActionType;
import com.google.search.now.wire.feed.CapabilityProto.Capability;
import com.google.search.now.wire.feed.ClientInfoProto.ClientInfo;
import com.google.search.now.wire.feed.ClientInfoProto.ClientInfo.AppType;
import com.google.search.now.wire.feed.ClientInfoProto.ClientInfo.PlatformType;
import com.google.search.now.wire.feed.ConsistencyTokenProto.ConsistencyToken;
import com.google.search.now.wire.feed.ContentIdProto.ContentId;
import com.google.search.now.wire.feed.DisplayInfoProto.DisplayInfo;
import com.google.search.now.wire.feed.FeedActionQueryDataProto.Action;
import com.google.search.now.wire.feed.FeedActionQueryDataProto.FeedActionQueryData;
import com.google.search.now.wire.feed.FeedActionQueryDataProto.FeedActionQueryDataItem;
import com.google.search.now.wire.feed.FeedQueryProto.FeedQuery;
import com.google.search.now.wire.feed.FeedRequestProto.FeedRequest;
import com.google.search.now.wire.feed.RequestProto.Request;
import com.google.search.now.wire.feed.RequestProto.Request.RequestVersion;
import com.google.search.now.wire.feed.ResponseProto.Response;
import com.google.search.now.wire.feed.SemanticPropertiesProto.SemanticProperties;
import com.google.search.now.wire.feed.VersionProto.Version;
import com.google.search.now.wire.feed.VersionProto.Version.Architecture;
import com.google.search.now.wire.feed.VersionProto.Version.BuildType;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;
import org.robolectric.util.ReflectionHelpers;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.charset.Charset;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.EnumSet;
import java.util.List;
import java.util.Locale;
import java.util.Set;

/** Test of the {@link FeedRequestManagerImpl} class. */
@RunWith(RobolectricTestRunner.class)
public class FeedRequestManagerImplTest {
    private static final int NOT_FOUND = 404;
    private static final String TABLE = "table";
    private static final String TABLE_2 = "table2";
    private static final String CONTENT_DOMAIN = "contentDomain";
    private static final String CONTENT_DOMAIN_2 = "contentDomain2";
    public static final long ID = 42;
    private static final long ID_2 = 2;
    private static final String APP_VERSION_STRING = "5.7";

    private final FakeClock fakeClock = new FakeClock();
    private final TimingUtils timingUtils = new TimingUtils();
    private final Configuration configuration = new Configuration.Builder().build();

    @Mock
    private SchedulerApi scheduler;
    @Mock
    private ApplicationInfo applicationInfo;

    private Context context;
    private ExtensionRegistryLite registry;
    private FeedRequestManagerImpl requestManager;
    private FakeActionReader fakeActionReader;
    private FakeMainThreadRunner fakeMainThreadRunner;
    private FakeProtocolAdapter fakeProtocolAdapter;
    private FakeThreadUtils fakeThreadUtils;
    private FakeTaskQueue fakeTaskQueue;
    private FakeBasicLoggingApi fakeBasicLoggingApi;
    private FakeNetworkClient fakeNetworkClient;
    private FakeTooltipSupportedApi fakeTooltipSupportedApi;
    private RequiredConsumer<Result<Model>> consumer;
    private Result<Model> consumedResult = Result.failure();
    private HttpResponse failingResponse;

    @Before
    public void setUp() throws Exception {
        initMocks(this);
        context = Robolectric.buildActivity(Activity.class).get();
        context.getResources().getConfiguration().locale = Locale.US;
        FeedExtensionRegistry feedExtensionRegistry = new FeedExtensionRegistry(ArrayList::new);
        registry = ExtensionRegistryLite.newInstance();
        registry.add(FeedRequest.feedRequest);
        fakeActionReader = new FakeActionReader();
        fakeProtocolAdapter = new FakeProtocolAdapter();
        fakeBasicLoggingApi = new FakeBasicLoggingApi();
        fakeThreadUtils = FakeThreadUtils.withThreadChecks();
        fakeMainThreadRunner =
                FakeMainThreadRunner.runTasksImmediatelyWithThreadChecks(fakeThreadUtils);
        fakeNetworkClient = new FakeNetworkClient(fakeThreadUtils);
        fakeNetworkClient.setDefaultResponse(
                createHttpResponse(/* responseCode= */ 200, Response.getDefaultInstance()));
        fakeTaskQueue = new FakeTaskQueue(fakeClock, fakeThreadUtils);
        fakeTaskQueue.initialize(() -> {});
        fakeTooltipSupportedApi = new FakeTooltipSupportedApi(fakeThreadUtils);
        failingResponse =
                createHttpResponse(/* responseCode= */ NOT_FOUND, Response.getDefaultInstance());
        consumer = new RequiredConsumer<>(input -> { consumedResult = input; });
        ReflectionHelpers.setStaticField(Build.VERSION.class, "SDK_INT", VERSION_CODES.KITKAT);
        ReflectionHelpers.setStaticField(Build.VERSION.class, "RELEASE", "4.4.3");
        ReflectionHelpers.setStaticField(Build.class, "CPU_ABI", "armeabi");
        ReflectionHelpers.setStaticField(Build.class, "TAGS", "dev-keys");
        when(applicationInfo.getAppType()).thenReturn(ApplicationInfo.AppType.CHROME);
        when(applicationInfo.getArchitecture()).thenReturn(ApplicationInfo.Architecture.ARM);
        when(applicationInfo.getBuildType()).thenReturn(ApplicationInfo.BuildType.DEV);
        when(applicationInfo.getVersionString()).thenReturn(APP_VERSION_STRING);
        requestManager = new FeedRequestManagerImpl(configuration, fakeNetworkClient,
                fakeProtocolAdapter, feedExtensionRegistry, scheduler, fakeTaskQueue, timingUtils,
                fakeThreadUtils, fakeActionReader, context, applicationInfo, fakeMainThreadRunner,
                fakeBasicLoggingApi, fakeTooltipSupportedApi);
    }

    @Test
    public void testTriggerRefresh() throws Exception {
        requestManager.triggerRefresh(RequestReason.HOST_REQUESTED, input -> {});

        assertThat(fakeTooltipSupportedApi.getLatestFeatureName()).isNull();

        HttpRequest httpRequest = fakeNetworkClient.getLatestRequest();
        assertHttpRequestFormattedCorrectly(httpRequest, context);

        Request request = getRequestFromHttpRequest(httpRequest);
        Request expectedRequest =
                Request.newBuilder()
                        .setRequestVersion(RequestVersion.FEED_QUERY)
                        .setExtension(FeedRequest.feedRequest,
                                getTestFeedRequestBuilder()
                                        .setFeedQuery(FeedQuery.newBuilder().setReason(
                                                FeedQuery.RequestReason.SCHEDULED_REFRESH))
                                        .addClientCapability(Capability.BASE_UI)
                                        .build())
                        .build();
        assertThat(request).isEqualTo(expectedRequest);
    }

    @Test
    public void testTriggerRefresh_FeedUiCapabilityAddedWhenFlagIsOn() throws Exception {
        testCapabilityAdded(ConfigKey.FEED_UI_ENABLED, Capability.FEED_UI);
    }

    @Test
    public void testTriggerRefresh_undoableActionCapabilityAddedWhenFlagIsOn() throws Exception {
        testCapabilityAdded(ConfigKey.UNDOABLE_ACTIONS_ENABLED, Capability.UNDOABLE_ACTIONS);
    }

    @Test
    public void testTriggerRefresh_manageInterestsCapabilityAddedWhenFlagIsOn() throws Exception {
        testCapabilityAdded(ConfigKey.MANAGE_INTERESTS_ENABLED, Capability.MANAGE_INTERESTS);
    }

    @Test
    public void testTriggerRefresh_tooltipCapabilityAddedWhenFlagIsOn() throws Exception {
        testCapabilityAdded(ConfigKey.CARD_MENU_TOOLTIP_ELIGIBLE, Capability.CARD_MENU_TOOLTIP);
    }

    @Test
    public void testTriggerRefresh_tooltipCapabilityNotAdded() throws Exception {
        // If the config key for card menu tool tip is set but the
        // TooltipSupportedApi.wouldTriggerHelpUi() returns false, then the capability should not be
        // added and only the BASE_UI should be present.
        fakeTooltipSupportedApi.addUnsupportedFeature(FeatureName.CARD_MENU_TOOLTIP);
        testCapabilityAdded(ConfigKey.CARD_MENU_TOOLTIP_ELIGIBLE, /* capability= */ null);
    }

    @Test
    public void testTriggerRefresh_useSecondaryPageRequestAdded() throws Exception {
        testCapabilityAdded(
                ConfigKey.USE_SECONDARY_PAGE_REQUEST, Capability.USE_SECONDARY_PAGE_REQUEST);
    }

    @Test
    public void testTriggerRefresh_articleSnippetsAdded() throws Exception {
        testCapabilityAdded(ConfigKey.SNIPPETS_ENABLED, Capability.ARTICLE_SNIPPETS);
    }

    @Test
    public void testTriggerRefresh_enableCarouselsAdded() throws Exception {
        testCapabilityAdded(ConfigKey.ENABLE_CAROUSELS, Capability.CAROUSELS);
    }

    @Test
    public void testActionData_simpleDismiss() throws Exception {
        fakeActionReader.addDismissActionsWithSemanticProperties(
                buildDismissAction(ID, CONTENT_DOMAIN, TABLE, null));
        requestManager.triggerRefresh(RequestReason.HOST_REQUESTED, input -> {});

        HttpRequest httpRequest = fakeNetworkClient.getLatestRequest();
        assertHttpRequestFormattedCorrectly(httpRequest, context);

        Request request = getRequestFromHttpRequest(httpRequest);

        List<Long> expectedIds = Collections.singletonList(ID);
        List<String> expectedContentDomains = Collections.singletonList(CONTENT_DOMAIN);
        List<String> expectedTables = Collections.singletonList(TABLE);
        List<SemanticProperties> expectedSemanticProperties = Collections.emptyList();
        List<FeedActionQueryDataItem> expectedDataItems =
                Collections.singletonList(FeedActionQueryDataItem.newBuilder()
                                                  .setIdIndex(0)
                                                  .setContentDomainIndex(0)
                                                  .setTableIndex(0)
                                                  .build());

        Request expectedRequest =
                Request.newBuilder()
                        .setRequestVersion(RequestVersion.FEED_QUERY)
                        .setExtension(FeedRequest.feedRequest,
                                getTestFeedRequestBuilder()
                                        .setFeedQuery(FeedQuery.newBuilder().setReason(
                                                FeedQuery.RequestReason.SCHEDULED_REFRESH))
                                        .addFeedActionQueryData(
                                                FeedActionQueryData.newBuilder()
                                                        .setAction(
                                                                Action.newBuilder().setActionType(
                                                                        ActionType.DISMISS))
                                                        .addAllUniqueId(expectedIds)
                                                        .addAllUniqueContentDomain(
                                                                expectedContentDomains)
                                                        .addAllUniqueTable(expectedTables)
                                                        .addAllUniqueSemanticProperties(
                                                                expectedSemanticProperties)
                                                        .addAllFeedActionQueryDataItem(
                                                                expectedDataItems))
                                        .addClientCapability(Capability.BASE_UI)
                                        .build())
                        .build();
        assertThat(request).isEqualTo(expectedRequest);
    }

    @Test
    public void testActionData_uniqueDismisses() throws Exception {
        fakeActionReader.addDismissActionsWithSemanticProperties(
                buildDismissAction(ID, CONTENT_DOMAIN, TABLE, null),
                buildDismissAction(ID_2, CONTENT_DOMAIN_2, TABLE_2, null));
        requestManager.triggerRefresh(RequestReason.HOST_REQUESTED, input -> {});

        HttpRequest httpRequest = fakeNetworkClient.getLatestRequest();
        assertHttpRequestFormattedCorrectly(httpRequest, context);

        Request request = getRequestFromHttpRequest(httpRequest);

        List<Long> expectedIds = Arrays.asList(ID, ID_2);
        List<String> expectedContentDomains = Arrays.asList(CONTENT_DOMAIN, CONTENT_DOMAIN_2);
        List<String> expectedTables = Arrays.asList(TABLE, TABLE_2);
        List<SemanticProperties> expectedSemanticProperties = Collections.emptyList();
        List<FeedActionQueryDataItem> expectedDataItems =
                Arrays.asList(FeedActionQueryDataItem.newBuilder()
                                      .setIdIndex(0)
                                      .setContentDomainIndex(0)
                                      .setTableIndex(0)
                                      .build(),
                        FeedActionQueryDataItem.newBuilder()
                                .setIdIndex(1)
                                .setContentDomainIndex(1)
                                .setTableIndex(1)
                                .build());

        Request expectedRequest =
                Request.newBuilder()
                        .setRequestVersion(RequestVersion.FEED_QUERY)
                        .setExtension(FeedRequest.feedRequest,
                                getTestFeedRequestBuilder()
                                        .setFeedQuery(FeedQuery.newBuilder().setReason(
                                                FeedQuery.RequestReason.SCHEDULED_REFRESH))
                                        .addFeedActionQueryData(
                                                FeedActionQueryData.newBuilder()
                                                        .setAction(
                                                                Action.newBuilder().setActionType(
                                                                        ActionType.DISMISS))
                                                        .addAllUniqueId(expectedIds)
                                                        .addAllUniqueContentDomain(
                                                                expectedContentDomains)
                                                        .addAllUniqueTable(expectedTables)
                                                        .addAllUniqueSemanticProperties(
                                                                expectedSemanticProperties)
                                                        .addAllFeedActionQueryDataItem(
                                                                expectedDataItems))
                                        .addClientCapability(Capability.BASE_UI)
                                        .build())
                        .build();
        assertThat(request).isEqualTo(expectedRequest);
    }

    @Test
    public void testActionData_overlappingDismisses() throws Exception {
        fakeActionReader.addDismissActionsWithSemanticProperties(
                buildDismissAction(ID, CONTENT_DOMAIN, TABLE, null),
                buildDismissAction(ID_2, CONTENT_DOMAIN, TABLE, null));
        requestManager.triggerRefresh(RequestReason.HOST_REQUESTED, input -> {});

        HttpRequest httpRequest = fakeNetworkClient.getLatestRequest();
        assertHttpRequestFormattedCorrectly(httpRequest, context);

        Request request = getRequestFromHttpRequest(httpRequest);

        List<Long> expectedIds = Arrays.asList(ID, ID_2);
        List<String> expectedContentDomains = Collections.singletonList(CONTENT_DOMAIN);
        List<String> expectedTables = Collections.singletonList(TABLE);
        List<SemanticProperties> expectedSemanticProperties = Collections.emptyList();
        List<FeedActionQueryDataItem> expectedDataItems =
                Arrays.asList(FeedActionQueryDataItem.newBuilder()
                                      .setIdIndex(0)
                                      .setContentDomainIndex(0)
                                      .setTableIndex(0)
                                      .build(),
                        FeedActionQueryDataItem.newBuilder()
                                .setIdIndex(1)
                                .setContentDomainIndex(0)
                                .setTableIndex(0)
                                .build());

        Request expectedRequest =
                Request.newBuilder()
                        .setRequestVersion(RequestVersion.FEED_QUERY)
                        .setExtension(FeedRequest.feedRequest,
                                getTestFeedRequestBuilder()
                                        .setFeedQuery(FeedQuery.newBuilder().setReason(
                                                FeedQuery.RequestReason.SCHEDULED_REFRESH))
                                        .addFeedActionQueryData(
                                                FeedActionQueryData.newBuilder()
                                                        .setAction(
                                                                Action.newBuilder().setActionType(
                                                                        ActionType.DISMISS))
                                                        .addAllUniqueId(expectedIds)
                                                        .addAllUniqueContentDomain(
                                                                expectedContentDomains)
                                                        .addAllUniqueTable(expectedTables)
                                                        .addAllUniqueSemanticProperties(
                                                                expectedSemanticProperties)
                                                        .addAllFeedActionQueryDataItem(
                                                                expectedDataItems))
                                        .addClientCapability(Capability.BASE_UI)
                                        .build())
                        .build();
        assertThat(request).isEqualTo(expectedRequest);
    }

    @Test
    public void testActionData_simpleDismissWithSemanticProperties() throws Exception {
        byte[] semanticPropertiesBytes = {42, 17, 88};
        fakeActionReader.addDismissActionsWithSemanticProperties(
                buildDismissAction(ID, CONTENT_DOMAIN, TABLE, semanticPropertiesBytes));
        requestManager.triggerRefresh(RequestReason.HOST_REQUESTED, input -> {});

        HttpRequest httpRequest = fakeNetworkClient.getLatestRequest();
        assertHttpRequestFormattedCorrectly(httpRequest, context);

        Request request = getRequestFromHttpRequest(httpRequest);

        List<Long> expectedIds = Collections.singletonList(ID);
        List<String> expectedContentDomains = Collections.singletonList(CONTENT_DOMAIN);
        List<String> expectedTables = Collections.singletonList(TABLE);
        List<SemanticProperties> expectedSemanticProperties = Collections.singletonList(
                SemanticProperties.newBuilder()
                        .setSemanticPropertiesData(ByteString.copyFrom(semanticPropertiesBytes))
                        .build());
        List<FeedActionQueryDataItem> expectedDataItems =
                Collections.singletonList(FeedActionQueryDataItem.newBuilder()
                                                  .setIdIndex(0)
                                                  .setContentDomainIndex(0)
                                                  .setTableIndex(0)
                                                  .setSemanticPropertiesIndex(0)
                                                  .build());

        Request expectedRequest =
                Request.newBuilder()
                        .setRequestVersion(RequestVersion.FEED_QUERY)
                        .setExtension(FeedRequest.feedRequest,
                                getTestFeedRequestBuilder()
                                        .setFeedQuery(FeedQuery.newBuilder().setReason(
                                                FeedQuery.RequestReason.SCHEDULED_REFRESH))
                                        .addFeedActionQueryData(
                                                FeedActionQueryData.newBuilder()
                                                        .setAction(
                                                                Action.newBuilder().setActionType(
                                                                        ActionType.DISMISS))
                                                        .addAllUniqueId(expectedIds)
                                                        .addAllUniqueContentDomain(
                                                                expectedContentDomains)
                                                        .addAllUniqueTable(expectedTables)
                                                        .addAllUniqueSemanticProperties(
                                                                expectedSemanticProperties)
                                                        .addAllFeedActionQueryDataItem(
                                                                expectedDataItems))
                                        .addClientCapability(Capability.BASE_UI)
                                        .build())
                        .build();
        assertThat(request).isEqualTo(expectedRequest);
    }

    @Test
    public void testActionData_uniqueDismissesWithSemanticProperties() throws Exception {
        byte[] semanticPropertiesBytes = {42, 17, 88};
        byte[] semanticPropertiesBytes2 = {7, 43, 91};
        fakeActionReader.addDismissActionsWithSemanticProperties(
                buildDismissAction(ID, CONTENT_DOMAIN, TABLE, semanticPropertiesBytes),
                buildDismissAction(ID_2, CONTENT_DOMAIN_2, TABLE_2, semanticPropertiesBytes2));
        requestManager.triggerRefresh(RequestReason.HOST_REQUESTED, input -> {});

        HttpRequest httpRequest = fakeNetworkClient.getLatestRequest();
        assertHttpRequestFormattedCorrectly(httpRequest, context);

        Request request = getRequestFromHttpRequest(httpRequest);

        List<Long> expectedIds = Arrays.asList(ID, ID_2);
        List<String> expectedContentDomains = Arrays.asList(CONTENT_DOMAIN, CONTENT_DOMAIN_2);
        List<String> expectedTables = Arrays.asList(TABLE, TABLE_2);
        List<SemanticProperties> expectedSemanticProperties = Arrays.asList(
                SemanticProperties.newBuilder()
                        .setSemanticPropertiesData(ByteString.copyFrom(semanticPropertiesBytes))
                        .build(),
                SemanticProperties.newBuilder()
                        .setSemanticPropertiesData(ByteString.copyFrom(semanticPropertiesBytes2))
                        .build());
        List<FeedActionQueryDataItem> expectedDataItems =
                Arrays.asList(FeedActionQueryDataItem.newBuilder()
                                      .setIdIndex(0)
                                      .setContentDomainIndex(0)
                                      .setTableIndex(0)
                                      .setSemanticPropertiesIndex(0)
                                      .build(),
                        FeedActionQueryDataItem.newBuilder()
                                .setIdIndex(1)
                                .setContentDomainIndex(1)
                                .setTableIndex(1)
                                .setSemanticPropertiesIndex(1)
                                .build());

        Request expectedRequest =
                Request.newBuilder()
                        .setRequestVersion(RequestVersion.FEED_QUERY)
                        .setExtension(FeedRequest.feedRequest,
                                getTestFeedRequestBuilder()
                                        .setFeedQuery(FeedQuery.newBuilder().setReason(
                                                FeedQuery.RequestReason.SCHEDULED_REFRESH))
                                        .addFeedActionQueryData(
                                                FeedActionQueryData.newBuilder()
                                                        .setAction(
                                                                Action.newBuilder().setActionType(
                                                                        ActionType.DISMISS))
                                                        .addAllUniqueId(expectedIds)
                                                        .addAllUniqueContentDomain(
                                                                expectedContentDomains)
                                                        .addAllUniqueTable(expectedTables)
                                                        .addAllUniqueSemanticProperties(
                                                                expectedSemanticProperties)
                                                        .addAllFeedActionQueryDataItem(
                                                                expectedDataItems))
                                        .addClientCapability(Capability.BASE_UI)
                                        .build())
                        .build();
        assertThat(request).isEqualTo(expectedRequest);
    }

    @Test
    public void testActionData_overlappingDismissesWithSemanticProperties() throws Exception {
        byte[] semanticPropertiesBytes = {42, 17, 88};
        fakeActionReader.addDismissActionsWithSemanticProperties(
                buildDismissAction(ID, CONTENT_DOMAIN, TABLE, semanticPropertiesBytes),
                buildDismissAction(ID_2, CONTENT_DOMAIN, TABLE_2, semanticPropertiesBytes));
        requestManager.triggerRefresh(RequestReason.HOST_REQUESTED, input -> {});

        HttpRequest httpRequest = fakeNetworkClient.getLatestRequest();
        assertHttpRequestFormattedCorrectly(httpRequest, context);

        Request request = getRequestFromHttpRequest(httpRequest);

        List<Long> expectedIds = Arrays.asList(ID, ID_2);
        List<String> expectedContentDomains = Collections.singletonList(CONTENT_DOMAIN);
        List<String> expectedTables = Arrays.asList(TABLE, TABLE_2);
        List<SemanticProperties> expectedSemanticProperties = Collections.singletonList(
                SemanticProperties.newBuilder()
                        .setSemanticPropertiesData(ByteString.copyFrom(semanticPropertiesBytes))
                        .build());
        List<FeedActionQueryDataItem> expectedDataItems =
                Arrays.asList(FeedActionQueryDataItem.newBuilder()
                                      .setIdIndex(0)
                                      .setContentDomainIndex(0)
                                      .setTableIndex(0)
                                      .setSemanticPropertiesIndex(0)
                                      .build(),
                        FeedActionQueryDataItem.newBuilder()
                                .setIdIndex(1)
                                .setContentDomainIndex(0)
                                .setTableIndex(1)
                                .setSemanticPropertiesIndex(0)
                                .build());

        Request expectedRequest =
                Request.newBuilder()
                        .setRequestVersion(RequestVersion.FEED_QUERY)
                        .setExtension(FeedRequest.feedRequest,
                                getTestFeedRequestBuilder()
                                        .setFeedQuery(FeedQuery.newBuilder().setReason(
                                                FeedQuery.RequestReason.SCHEDULED_REFRESH))
                                        .addFeedActionQueryData(
                                                FeedActionQueryData.newBuilder()
                                                        .setAction(
                                                                Action.newBuilder().setActionType(
                                                                        ActionType.DISMISS))
                                                        .addAllUniqueId(expectedIds)
                                                        .addAllUniqueContentDomain(
                                                                expectedContentDomains)
                                                        .addAllUniqueTable(expectedTables)
                                                        .addAllUniqueSemanticProperties(
                                                                expectedSemanticProperties)
                                                        .addAllFeedActionQueryDataItem(
                                                                expectedDataItems))
                                        .addClientCapability(Capability.BASE_UI)
                                        .build())
                        .build();
        assertThat(request).isEqualTo(expectedRequest);
    }

    @Test
    public void testActionData_someDismissesWithSemanticProperties() throws Exception {
        byte[] semanticPropertiesBytes = {42, 17, 88};
        fakeActionReader.addDismissActionsWithSemanticProperties(
                buildDismissAction(ID, CONTENT_DOMAIN, TABLE, null),
                buildDismissAction(ID_2, CONTENT_DOMAIN_2, TABLE_2, semanticPropertiesBytes));
        requestManager.triggerRefresh(RequestReason.HOST_REQUESTED, input -> {});

        HttpRequest httpRequest = fakeNetworkClient.getLatestRequest();
        assertHttpRequestFormattedCorrectly(httpRequest, context);

        Request request = getRequestFromHttpRequest(httpRequest);

        List<Long> expectedIds = Arrays.asList(ID, ID_2);
        List<String> expectedContentDomains = Arrays.asList(CONTENT_DOMAIN, CONTENT_DOMAIN_2);
        List<String> expectedTables = Arrays.asList(TABLE, TABLE_2);
        List<SemanticProperties> expectedSemanticProperties = Collections.singletonList(
                SemanticProperties.newBuilder()
                        .setSemanticPropertiesData(ByteString.copyFrom(semanticPropertiesBytes))
                        .build());
        List<FeedActionQueryDataItem> expectedDataItems =
                Arrays.asList(FeedActionQueryDataItem.newBuilder()
                                      .setIdIndex(0)
                                      .setContentDomainIndex(0)
                                      .setTableIndex(0)
                                      .build(),
                        FeedActionQueryDataItem.newBuilder()
                                .setIdIndex(1)
                                .setContentDomainIndex(1)
                                .setTableIndex(1)
                                .setSemanticPropertiesIndex(0)
                                .build());

        Request expectedRequest =
                Request.newBuilder()
                        .setRequestVersion(RequestVersion.FEED_QUERY)
                        .setExtension(FeedRequest.feedRequest,
                                getTestFeedRequestBuilder()
                                        .setFeedQuery(FeedQuery.newBuilder().setReason(
                                                FeedQuery.RequestReason.SCHEDULED_REFRESH))
                                        .addFeedActionQueryData(
                                                FeedActionQueryData.newBuilder()
                                                        .setAction(
                                                                Action.newBuilder().setActionType(
                                                                        ActionType.DISMISS))
                                                        .addAllUniqueId(expectedIds)
                                                        .addAllUniqueContentDomain(
                                                                expectedContentDomains)
                                                        .addAllUniqueTable(expectedTables)
                                                        .addAllUniqueSemanticProperties(
                                                                expectedSemanticProperties)
                                                        .addAllFeedActionQueryDataItem(
                                                                expectedDataItems))
                                        .addClientCapability(Capability.BASE_UI)
                                        .build())
                        .build();
        assertThat(request).isEqualTo(expectedRequest);
    }

    @Test
    public void testHandleResponse() throws Exception {
        requestManager.triggerRefresh(RequestReason.HOST_REQUESTED, consumer);

        assertThat(fakeProtocolAdapter.getLastResponse()).isEqualToDefaultInstance();
        assertThat(consumer.isCalled()).isTrue();
        assertThat(consumedResult.isSuccessful()).isTrue();
    }

    @Test
    public void testHandleResponse_notFound() throws Exception {
        fakeNetworkClient.addResponse(failingResponse);
        requestManager.triggerRefresh(RequestReason.HOST_REQUESTED, consumer);

        verify(scheduler).onRequestError(NOT_FOUND);
        assertThat(consumer.isCalled()).isTrue();
        assertThat(consumedResult.isSuccessful()).isFalse();
    }

    @Test
    public void testHandleResponse_pageNotFound() throws Exception {
        fakeNetworkClient.addResponse(failingResponse);
        StreamToken token =
                StreamToken.newBuilder()
                        .setNextPageToken(ByteString.copyFrom("abc", Charset.defaultCharset()))
                        .build();
        fakeThreadUtils.enforceMainThread(false);
        requestManager.loadMore(token, ConsistencyToken.getDefaultInstance(), consumer);

        verify(scheduler, never()).onRequestError(NOT_FOUND);
        assertThat(consumer.isCalled()).isTrue();
        assertThat(consumedResult.isSuccessful()).isFalse();
    }

    @Test
    public void testHandleResponse_missingLengthPrefixNotSupported() {
        fakeNetworkClient.addResponse(new HttpResponse(
                /* responseCode= */ 200, Response.getDefaultInstance().toByteArray()));
        requestManager.triggerRefresh(RequestReason.HOST_REQUESTED, consumer);
        assertThat(consumer.isCalled()).isTrue();
        assertThat(consumedResult.isSuccessful()).isFalse();
        assertThat(fakeProtocolAdapter.getLastResponse()).isNull();
    }

    @Test
    public void testGetWireRequestResponse_unknown() throws Exception {
        testReason(RequestReason.UNKNOWN, FeedQuery.RequestReason.UNKNOWN_REQUEST_REASON);
    }

    @Test
    public void testGetWireRequestResponse_zeroState() throws Exception {
        testReason(RequestReason.ZERO_STATE, FeedQuery.RequestReason.ZERO_STATE_REFRESH);
    }

    @Test
    public void testGetWireRequestResponse_hostRequested() throws Exception {
        testReason(RequestReason.HOST_REQUESTED, FeedQuery.RequestReason.SCHEDULED_REFRESH);
    }

    @Test
    public void testGetWireRequestResponse_openWithContent() throws Exception {
        testReason(RequestReason.OPEN_WITH_CONTENT, FeedQuery.RequestReason.WITH_CONTENT);
    }

    @Test
    public void testGetWireRequestResponse_manualContinuation() throws Exception {
        testReason(RequestReason.MANUAL_CONTINUATION, FeedQuery.RequestReason.NEXT_PAGE_SCROLL);
    }

    @Test
    public void testGetWireRequestResponse_automaticContinuation() throws Exception {
        testReason(RequestReason.AUTOMATIC_CONTINUATION, FeedQuery.RequestReason.NEXT_PAGE_SCROLL);
    }

    @Test
    public void testGetWireRequestResponse_openWithoutContent() throws Exception {
        testReason(RequestReason.OPEN_WITHOUT_CONTENT, FeedQuery.RequestReason.INITIAL_LOAD);
    }

    @Test
    public void testGetWireRequestResponse_clearAll() throws Exception {
        testReason(RequestReason.CLEAR_ALL, FeedQuery.RequestReason.CLEAR_ALL);
    }

    @Test
    @Config(qualifiers = "en-rGB", sdk = VERSION_CODES.LOLLIPOP)
    public void testClientInfo_postLollipop() throws Exception {
        ReflectionHelpers.setStaticField(Build.VERSION.class, "SDK_INT", VERSION_CODES.LOLLIPOP);
        ReflectionHelpers.setStaticField(Build.VERSION.class, "RELEASE", "7.1.2b4.1");
        ReflectionHelpers.setStaticField(Build.class, "SUPPORTED_ABIS", new String[] {"arm64-v8a"});
        ReflectionHelpers.setStaticField(Build.class, "CPU_ABI", "armeabi");
        ReflectionHelpers.setStaticField(Build.class, "TAGS", "release-keys");
        when(applicationInfo.getAppType()).thenReturn(ApplicationInfo.AppType.SEARCH_APP);
        when(applicationInfo.getArchitecture()).thenReturn(ApplicationInfo.Architecture.ARM64);
        when(applicationInfo.getBuildType()).thenReturn(ApplicationInfo.BuildType.RELEASE);
        when(applicationInfo.getVersionString()).thenReturn("1.2.3.4");

        requestManager.triggerRefresh(RequestReason.HOST_REQUESTED, input -> {});

        HttpRequest httpRequest = fakeNetworkClient.getLatestRequest();

        Request request = getRequestFromHttpRequest(httpRequest);
        Request expectedRequest =
                Request.newBuilder()
                        .setRequestVersion(RequestVersion.FEED_QUERY)
                        .setExtension(FeedRequest.feedRequest,
                                getTestFeedRequestBuilder()
                                        .setFeedQuery(FeedQuery.newBuilder().setReason(
                                                FeedQuery.RequestReason.SCHEDULED_REFRESH))
                                        .setClientInfo(
                                                ClientInfo.newBuilder()
                                                        .setPlatformType(PlatformType.ANDROID_ID)
                                                        .setPlatformVersion(
                                                                Version.newBuilder()
                                                                        .setMajor(7)
                                                                        .setMinor(1)
                                                                        .setBuild(2)
                                                                        .setRevision(1)
                                                                        .setArchitecture(
                                                                                Architecture.ARM64)
                                                                        .setBuildType(
                                                                                BuildType.RELEASE)
                                                                        .setApiVersion(
                                                                                VERSION_CODES
                                                                                        .LOLLIPOP)
                                                                        .build())
                                                        .setLocale(
                                                                LocaleUtils.getLanguageTag(context))
                                                        .setAppType(AppType.GSA)
                                                        .setAppVersion(
                                                                Version.newBuilder()
                                                                        .setMajor(1)
                                                                        .setMinor(2)
                                                                        .setBuild(3)
                                                                        .setRevision(4)
                                                                        .setArchitecture(
                                                                                Architecture.ARM64)
                                                                        .setBuildType(
                                                                                BuildType.RELEASE)
                                                                        .build())
                                                        .addDisplayInfo(
                                                                DisplayInfo.newBuilder()
                                                                        .setScreenDensity(1.0f)
                                                                        .setScreenWidthInPixels(320)
                                                                        .setScreenHeightInPixels(
                                                                                470))
                                                        .build())
                                        .addClientCapability(Capability.BASE_UI)
                                        .build())
                        .build();
        assertThat(request).isEqualTo(expectedRequest);
    }

    private void testReason(@RequestReason int reason, FeedQuery.RequestReason expectedReason)
            throws Exception {
        fakeNetworkClient.addResponse(failingResponse);
        requestManager.triggerRefresh(reason, input -> {});

        HttpRequest httpRequest = fakeNetworkClient.getLatestRequest();
        Request request = getRequestFromHttpRequest(httpRequest);
        assertThat(request.getExtension(FeedRequest.feedRequest).getFeedQuery().getReason())
                .isEqualTo(expectedReason);
        assertThat(fakeBasicLoggingApi.serverRequestReason).isEqualTo(reason);
    }

    private static void assertHttpRequestFormattedCorrectly(
            HttpRequest httpRequest, Context context) {
        assertThat(httpRequest.getBody()).hasLength(0);
        assertThat(httpRequest.getMethod()).isEqualTo(HttpMethod.GET);
        assertThat(httpRequest.getUri().getQueryParameter("fmt")).isEqualTo("bin");
        assertThat(httpRequest.getUri().getQueryParameter(RequestHelper.MOTHERSHIP_PARAM_PAYLOAD))
                .isNotNull();
        assertThat(httpRequest.getUri().getQueryParameter(RequestHelper.LOCALE_PARAM))
                .isEqualTo(LocaleUtils.getLanguageTag(context));
    }

    private static HttpResponse createHttpResponse(int responseCode, Response response)
            throws IOException {
        byte[] rawResponse = response.toByteArray();
        ByteBuffer buffer = ByteBuffer.allocate(rawResponse.length + (Integer.SIZE / 8));
        CodedOutputStream codedOutputStream = CodedOutputStream.newInstance(buffer);
        codedOutputStream.writeUInt32NoTag(rawResponse.length);
        codedOutputStream.writeRawBytes(rawResponse);
        codedOutputStream.flush();
        return new HttpResponse(responseCode, buffer.array());
    }

    private static DismissActionWithSemanticProperties buildDismissAction(
            long id, String contentDomain, String table, byte /*@Nullable*/[] semanticProperties) {
        ContentId contentId = ContentId.newBuilder()
                                      .setTable(table)
                                      .setContentDomain(contentDomain)
                                      .setId(id)
                                      .build();
        return new DismissActionWithSemanticProperties(contentId, semanticProperties);
    }

    private Request getRequestFromHttpRequest(HttpRequest httpRequest) throws Exception {
        return Request.parseFrom(Base64.decode(httpRequest.getUri().getQueryParameter(
                                                       RequestHelper.MOTHERSHIP_PARAM_PAYLOAD),
                                         Base64.URL_SAFE),
                registry);
    }

    private static FeedRequest.Builder getTestFeedRequestBuilder() {
        return FeedRequest.newBuilder()
                .setConsistencyToken(ConsistencyToken.getDefaultInstance())
                .setClientInfo(
                        ClientInfo.newBuilder()
                                .setPlatformType(PlatformType.ANDROID_ID)
                                .setPlatformVersion(Version.newBuilder()
                                                            .setMajor(4)
                                                            .setMinor(4)
                                                            .setBuild(3)
                                                            .setArchitecture(Architecture.ARM)
                                                            .setBuildType(BuildType.DEV)
                                                            .setApiVersion(VERSION_CODES.KITKAT)
                                                            .build())
                                .setLocale(Locale.US.toLanguageTag())
                                .setAppType(AppType.CHROME)
                                .setAppVersion(Version.newBuilder()
                                                       .setMajor(5)
                                                       .setMinor(7)
                                                       .setArchitecture(Architecture.ARM)
                                                       .setBuildType(BuildType.DEV)
                                                       .build())
                                .addDisplayInfo(DisplayInfo.newBuilder()
                                                        .setScreenDensity(1.0f)
                                                        .setScreenWidthInPixels(320)
                                                        .setScreenHeightInPixels(470))
                                .build());
    }

    private void testCapabilityAdded(String configKey, Capability capability) throws Exception {
        Configuration configuration = new Configuration.Builder().put(configKey, true).build();
        requestManager =
                new FeedRequestManagerImpl(configuration, fakeNetworkClient, fakeProtocolAdapter,
                        new FeedExtensionRegistry(ArrayList::new), scheduler, fakeTaskQueue,
                        timingUtils, fakeThreadUtils, fakeActionReader, context, applicationInfo,
                        fakeMainThreadRunner, fakeBasicLoggingApi, fakeTooltipSupportedApi);
        requestManager.triggerRefresh(RequestReason.HOST_REQUESTED, input -> {});

        HttpRequest httpRequest = fakeNetworkClient.getLatestRequest();
        assertHttpRequestFormattedCorrectly(httpRequest, context);

        Set<Capability> expectedCap = EnumSet.of(Capability.BASE_UI);
        if (capability != null) {
            expectedCap.add(capability);
        }

        Request request = getRequestFromHttpRequest(httpRequest);
        assertThat(request.getExtension(FeedRequest.feedRequest).getClientCapabilityList())
                .containsExactlyElementsIn(expectedCap);
    }
}
