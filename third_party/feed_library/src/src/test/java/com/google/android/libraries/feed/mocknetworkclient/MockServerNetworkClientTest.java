// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.mocknetworkclient;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import android.app.Activity;
import android.content.Context;
import android.net.Uri;

import com.google.android.libraries.feed.api.host.config.ApplicationInfo;
import com.google.android.libraries.feed.api.host.config.Configuration;
import com.google.android.libraries.feed.api.host.network.HttpRequest;
import com.google.android.libraries.feed.api.host.network.HttpRequest.HttpMethod;
import com.google.android.libraries.feed.api.host.network.HttpResponse;
import com.google.android.libraries.feed.api.host.scheduler.SchedulerApi;
import com.google.android.libraries.feed.api.host.stream.TooltipSupportedApi;
import com.google.android.libraries.feed.api.internal.actionmanager.ActionReader;
import com.google.android.libraries.feed.api.internal.common.Model;
import com.google.android.libraries.feed.api.internal.protocoladapter.ProtocolAdapter;
import com.google.android.libraries.feed.common.Result;
import com.google.android.libraries.feed.common.concurrent.MainThreadRunner;
import com.google.android.libraries.feed.common.concurrent.testing.FakeMainThreadRunner;
import com.google.android.libraries.feed.common.concurrent.testing.FakeTaskQueue;
import com.google.android.libraries.feed.common.concurrent.testing.FakeThreadUtils;
import com.google.android.libraries.feed.common.functional.Consumer;
import com.google.android.libraries.feed.common.protoextensions.FeedExtensionRegistry;
import com.google.android.libraries.feed.common.time.TimingUtils;
import com.google.android.libraries.feed.common.time.testing.FakeClock;
import com.google.android.libraries.feed.feedrequestmanager.FeedRequestManagerImpl;
import com.google.android.libraries.feed.testing.conformance.network.NetworkClientConformanceTest;
import com.google.android.libraries.feed.testing.host.logging.FakeBasicLoggingApi;
import com.google.common.truth.extensions.proto.LiteProtoTruth;
import com.google.protobuf.ByteString;
import com.google.protobuf.CodedInputStream;
import com.google.search.now.feed.client.StreamDataProto.StreamToken;
import com.google.search.now.wire.feed.ConsistencyTokenProto.ConsistencyToken;
import com.google.search.now.wire.feed.ResponseProto.Response;
import com.google.search.now.wire.feed.ResponseProto.Response.ResponseVersion;
import com.google.search.now.wire.feed.mockserver.MockServerProto.ConditionalResponse;
import com.google.search.now.wire.feed.mockserver.MockServerProto.MockServer;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;

/** Tests of the {@link MockServerNetworkClient} class. */
@RunWith(RobolectricTestRunner.class)
public class MockServerNetworkClientTest extends NetworkClientConformanceTest {
    private final Configuration configuration = new Configuration.Builder().build();
    private final FakeClock fakeClock = new FakeClock();
    private final FakeThreadUtils fakeThreadUtils = FakeThreadUtils.withThreadChecks();
    private final FeedExtensionRegistry extensionRegistry =
            new FeedExtensionRegistry(ArrayList::new);
    private final TimingUtils timingUtils = new TimingUtils();

    @Mock
    private ActionReader actionReader;
    @Mock
    private ProtocolAdapter protocolAdapter;
    @Mock
    private SchedulerApi scheduler;
    @Mock
    private TooltipSupportedApi tooltipSupportedApi;
    @Captor
    private ArgumentCaptor<Response> responseCaptor;
    private ApplicationInfo applicationInfo;
    private Context context;
    private FakeBasicLoggingApi basicLoggingApi;
    private MainThreadRunner mainThreadRunner;

    @Override
    protected Uri getValidUri(@HttpMethod String method) {
        // The URI does not matter - mockNetworkClient will default to an empty response
        return new Uri.Builder().path("foo").appendPath(method).build();
    }

    @Before
    public void setUp() throws Exception {
        initMocks(this);
        context = Robolectric.buildActivity(Activity.class).get();
        applicationInfo = new ApplicationInfo.Builder(context).setVersionString("0").build();
        mainThreadRunner = FakeMainThreadRunner.runTasksImmediately();

        MockServer mockServer = MockServer.getDefaultInstance();
        networkClient =
                new MockServerNetworkClient(context, mockServer, /* responseDelayMillis= */ 0L);

        when(actionReader.getDismissActionsWithSemanticProperties())
                .thenReturn(Result.success(Collections.emptyList()));

        basicLoggingApi = new FakeBasicLoggingApi();
    }

    @Test
    public void testSend() {
        MockServer.Builder mockServerBuilder = MockServer.newBuilder();
        Response initialResponse =
                Response.newBuilder().setResponseVersion(ResponseVersion.FEED_RESPONSE).build();
        mockServerBuilder.setInitialResponse(initialResponse);
        MockServerNetworkClient networkClient = new MockServerNetworkClient(
                context, mockServerBuilder.build(), /* responseDelayMillis= */ 0L);
        Consumer<HttpResponse> responseConsumer = input -> {
            try {
                CodedInputStream inputStream =
                        CodedInputStream.newInstance(input.getResponseBody());
                int length = inputStream.readRawVarint32();
                assertThat(inputStream.readRawBytes(length))
                        .isEqualTo(initialResponse.toByteArray());
                assertThat(input.getResponseCode()).isEqualTo(200);
            } catch (IOException e) {
                throw new RuntimeException(e);
            }
        };
        networkClient.send(
                new HttpRequest(Uri.EMPTY, HttpMethod.POST, Collections.emptyList(), new byte[] {}),
                responseConsumer);
    }

    @Test
    public void testSend_oneTimeResponse() {
        MockServer.Builder mockServerBuilder = MockServer.newBuilder();
        Response initialResponse =
                Response.newBuilder().setResponseVersion(ResponseVersion.FEED_RESPONSE).build();
        mockServerBuilder.setInitialResponse(initialResponse);
        MockServerNetworkClient networkClient = new MockServerNetworkClient(
                context, mockServerBuilder.build(), /* responseDelayMillis= */ 0L);
        Consumer<HttpResponse> responseConsumer = input -> {
            try {
                CodedInputStream inputStream =
                        CodedInputStream.newInstance(input.getResponseBody());
                int length = inputStream.readRawVarint32();
                assertThat(inputStream.readRawBytes(length))
                        .isEqualTo(initialResponse.toByteArray());
                assertThat(input.getResponseCode()).isEqualTo(200);
            } catch (IOException e) {
                throw new RuntimeException(e);
            }
        };
        networkClient.send(
                new HttpRequest(Uri.EMPTY, HttpMethod.POST, Collections.emptyList(), new byte[] {}),
                responseConsumer);
    }

    @Test
    public void testPaging() {
        MockServer.Builder mockServerBuilder = MockServer.newBuilder();
        Response response =
                Response.newBuilder().setResponseVersion(ResponseVersion.FEED_RESPONSE).build();
        ByteString token = ByteString.copyFromUtf8("fooToken");
        StreamToken streamToken = StreamToken.newBuilder().setNextPageToken(token).build();
        ConditionalResponse.Builder conditionalResponseBuilder = ConditionalResponse.newBuilder();
        conditionalResponseBuilder.setContinuationToken(token).setResponse(response);
        mockServerBuilder.addConditionalResponses(conditionalResponseBuilder.build());
        MockServerNetworkClient networkClient = new MockServerNetworkClient(
                context, mockServerBuilder.build(), /* responseDelayMillis= */ 0L);
        FeedRequestManagerImpl feedRequestManager = new FeedRequestManagerImpl(configuration,
                networkClient, protocolAdapter, extensionRegistry, scheduler, getTaskQueue(),
                timingUtils, fakeThreadUtils, actionReader, context, applicationInfo,
                mainThreadRunner, basicLoggingApi, tooltipSupportedApi);
        when(protocolAdapter.createModel(any(Response.class)))
                .thenReturn(Result.success(Model.empty()));

        fakeThreadUtils.enforceMainThread(false);
        feedRequestManager.loadMore(streamToken, ConsistencyToken.getDefaultInstance(), result -> {
            assertThat(result.isSuccessful()).isTrue();
            assertThat(result.getValue().streamDataOperations).hasSize(0);
        });

        verify(protocolAdapter).createModel(responseCaptor.capture());
        LiteProtoTruth.assertThat(responseCaptor.getValue()).isEqualTo(response);
    }

    @Test
    public void testPaging_noMatch() {
        MockServer.Builder mockServerBuilder = MockServer.newBuilder();
        Response response =
                Response.newBuilder().setResponseVersion(ResponseVersion.FEED_RESPONSE).build();
        // Create a MockServerConfig without a matching token.
        ConditionalResponse.Builder conditionalResponseBuilder = ConditionalResponse.newBuilder();
        conditionalResponseBuilder.setResponse(response);
        mockServerBuilder.addConditionalResponses(conditionalResponseBuilder.build());
        MockServerNetworkClient networkClient = new MockServerNetworkClient(
                context, mockServerBuilder.build(), /* responseDelayMillis= */ 0L);
        FeedRequestManagerImpl feedRequestManager = new FeedRequestManagerImpl(configuration,
                networkClient, protocolAdapter, extensionRegistry, scheduler, getTaskQueue(),
                timingUtils, fakeThreadUtils, actionReader, context, applicationInfo,
                mainThreadRunner, basicLoggingApi, tooltipSupportedApi);
        when(protocolAdapter.createModel(any(Response.class)))
                .thenReturn(Result.success(Model.empty()));

        fakeThreadUtils.enforceMainThread(false);
        ByteString token = ByteString.copyFromUtf8("fooToken");
        StreamToken streamToken = StreamToken.newBuilder().setNextPageToken(token).build();
        feedRequestManager.loadMore(streamToken, ConsistencyToken.getDefaultInstance(), result -> {
            assertThat(result.isSuccessful()).isTrue();
            assertThat(result.getValue().streamDataOperations).hasSize(0);
        });

        verify(protocolAdapter).createModel(responseCaptor.capture());
        LiteProtoTruth.assertThat(responseCaptor.getValue()).isEqualToDefaultInstance();
    }

    private FakeTaskQueue getTaskQueue() {
        FakeTaskQueue taskQueue = new FakeTaskQueue(fakeClock, fakeThreadUtils);
        taskQueue.initialize(() -> {});
        return taskQueue;
    }
}
