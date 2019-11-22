// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.feedrequestmanager;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.MockitoAnnotations.initMocks;

import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.util.Base64;

import com.google.android.libraries.feed.api.host.config.Configuration;
import com.google.android.libraries.feed.api.host.config.Configuration.ConfigKey;
import com.google.android.libraries.feed.api.host.network.HttpRequest;
import com.google.android.libraries.feed.api.host.network.HttpRequest.HttpMethod;
import com.google.android.libraries.feed.api.host.network.HttpResponse;
import com.google.android.libraries.feed.common.Result;
import com.google.android.libraries.feed.common.concurrent.testing.FakeTaskQueue;
import com.google.android.libraries.feed.common.concurrent.testing.FakeThreadUtils;
import com.google.android.libraries.feed.common.protoextensions.FeedExtensionRegistry;
import com.google.android.libraries.feed.common.testing.RequiredConsumer;
import com.google.android.libraries.feed.common.time.testing.FakeClock;
import com.google.android.libraries.feed.testing.network.FakeNetworkClient;
import com.google.android.libraries.feed.testing.protocoladapter.FakeProtocolAdapter;
import com.google.android.libraries.feed.testing.store.FakeStore;
import com.google.protobuf.ByteString;
import com.google.protobuf.CodedOutputStream;
import com.google.protobuf.ExtensionRegistryLite;
import com.google.search.now.feed.client.StreamDataProto.StreamUploadableAction;
import com.google.search.now.wire.feed.ActionRequestProto.ActionRequest;
import com.google.search.now.wire.feed.ConsistencyTokenProto.ConsistencyToken;
import com.google.search.now.wire.feed.FeedActionRequestProto.FeedActionRequest;
import com.google.search.now.wire.feed.FeedActionResponseProto.FeedActionResponse;
import com.google.search.now.wire.feed.FeedRequestProto.FeedRequest;
import com.google.search.now.wire.feed.ResponseProto.Response;
import com.google.search.now.wire.feed.SemanticPropertiesProto.SemanticProperties;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.util.ReflectionHelpers;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.Set;

/** Test of the {@link FeedActionUploadRequestManager} class. */
@RunWith(RobolectricTestRunner.class)
public class FeedActionUploadRequestManagerTest {
    public static final long ID = 42;
    private static final ConsistencyToken TOKEN_1 =
            ConsistencyToken.newBuilder()
                    .setToken(ByteString.copyFrom(new byte[] {0x1, 0xa}))
                    .build();
    private static final ConsistencyToken TOKEN_2 =
            ConsistencyToken.newBuilder()
                    .setToken(ByteString.copyFrom(new byte[] {0x1, 0xf}))
                    .build();
    private static final ConsistencyToken TOKEN_3 =
            ConsistencyToken.newBuilder()
                    .setToken(ByteString.copyFrom(new byte[] {0x2, 0xa}))
                    .build();
    private static final String CONTENT_ID = "contentId";
    private static final String CONTENT_ID_2 = "contentId2";
    private static final byte[] SEMANTIC_PROPERTIES_BYTES = new byte[] {0x1, 0xa};
    private static final SemanticProperties SEMANTIC_PROPERTIES =
            SemanticProperties.newBuilder()
                    .setSemanticPropertiesData(ByteString.copyFrom(SEMANTIC_PROPERTIES_BYTES))
                    .build();
    private static final Response RESPONSE_1 =
            Response.newBuilder()
                    .setExtension(FeedActionResponse.feedActionResponse,
                            FeedActionResponse.newBuilder().setConsistencyToken(TOKEN_2).build())
                    .build();
    private static final Response RESPONSE_2 =
            Response.newBuilder()
                    .setExtension(FeedActionResponse.feedActionResponse,
                            FeedActionResponse.newBuilder().setConsistencyToken(TOKEN_3).build())
                    .build();

    private final Configuration configuration = new Configuration.Builder().build();
    private final FakeClock fakeClock = new FakeClock();

    private ExtensionRegistryLite registry;
    private FakeNetworkClient fakeNetworkClient;
    private FakeProtocolAdapter fakeProtocolAdapter;
    private FakeStore fakeStore;
    private FakeTaskQueue fakeTaskQueue;
    private FakeThreadUtils fakeThreadUtils;
    private FeedActionUploadRequestManager requestManager;
    private RequiredConsumer<Result<ConsistencyToken>> consumer;

    @Before
    public void setUp() {
        initMocks(this);
        registry = ExtensionRegistryLite.newInstance();
        registry.add(FeedRequest.feedRequest);
        registry.add(FeedActionRequest.feedActionRequest);
        fakeThreadUtils = FakeThreadUtils.withThreadChecks();
        fakeNetworkClient = new FakeNetworkClient(fakeThreadUtils);
        fakeTaskQueue = new FakeTaskQueue(fakeClock, fakeThreadUtils);
        fakeProtocolAdapter = new FakeProtocolAdapter();
        fakeStore = new FakeStore(configuration, fakeThreadUtils, fakeTaskQueue, fakeClock);
        consumer = new RequiredConsumer<>(input -> { fakeThreadUtils.checkNotMainThread(); });

        ReflectionHelpers.setStaticField(Build.VERSION.class, "SDK_INT", VERSION_CODES.KITKAT);
        ReflectionHelpers.setStaticField(Build.VERSION.class, "RELEASE", "4.4.3");
        ReflectionHelpers.setStaticField(Build.class, "CPU_ABI", "armeabi");
        ReflectionHelpers.setStaticField(Build.class, "TAGS", "dev-keys");
        requestManager = createRequestManager(configuration);
        fakeThreadUtils.enforceMainThread(false);
        fakeTaskQueue.initialize(() -> {});
    }

    @Test
    public void testTriggerUploadActions_ttlExceededRemove() throws Exception {
        Configuration configuration =
                new Configuration.Builder()
                        .put(ConfigKey.FEED_ACTION_SERVER_METHOD, HttpMethod.GET)
                        .put(ConfigKey.FEED_ACTION_TTL_SECONDS, 1L)
                        .put(ConfigKey.FEED_ACTION_MAX_UPLOAD_ATTEMPTS, 5L)
                        .build();
        requestManager = createRequestManager(configuration);
        StreamUploadableAction action = StreamUploadableAction.newBuilder()
                                                .setUploadAttempts(1)
                                                .setTimestampSeconds(1L)
                                                .setFeatureContentId(CONTENT_ID)
                                                .build();
        fakeStore.setStreamUploadableActions(action);
        fakeClock.set(5000L);
        fakeNetworkClient.addResponse(
                createHttpResponse(/* responseCode= */ 200, Response.getDefaultInstance()));
        requestManager.triggerUploadActions(
                setOf(action), ConsistencyToken.getDefaultInstance(), consumer);

        assertThat(consumer.isCalled()).isTrue();
        assertThat(fakeStore.getContentById(CONTENT_ID)).isEmpty();
    }

    @Test
    public void testTriggerUploadActions_maxUploadsRemove() throws Exception {
        Configuration configuration =
                new Configuration.Builder()
                        .put(ConfigKey.FEED_ACTION_SERVER_METHOD, HttpMethod.GET)
                        .put(ConfigKey.FEED_ACTION_TTL_SECONDS, 1000L)
                        .put(ConfigKey.FEED_ACTION_MAX_UPLOAD_ATTEMPTS, 2L)
                        .build();
        requestManager = createRequestManager(configuration);
        StreamUploadableAction action = StreamUploadableAction.newBuilder()
                                                .setUploadAttempts(2)
                                                .setFeatureContentId(CONTENT_ID)
                                                .build();
        fakeNetworkClient.addResponse(
                createHttpResponse(/* responseCode= */ 200, Response.getDefaultInstance()));
        requestManager.triggerUploadActions(
                setOf(action), ConsistencyToken.getDefaultInstance(), consumer);

        assertThat(consumer.isCalled()).isTrue();
        assertThat(fakeStore.getContentById(CONTENT_ID)).isEmpty();
    }

    @Test
    public void testTriggerUploadActions_batchSuccess() throws Exception {
        Configuration configuration =
                new Configuration.Builder()
                        .put(ConfigKey.FEED_ACTION_SERVER_METHOD, HttpMethod.GET)
                        .put(ConfigKey.FEED_ACTION_TTL_SECONDS, 1000L)
                        .put(ConfigKey.FEED_ACTION_SERVER_MAX_SIZE_PER_REQUEST, 20L)
                        .put(ConfigKey.FEED_ACTION_MAX_UPLOAD_ATTEMPTS, 2L)
                        .put(ConfigKey.FEED_ACTION_SERVER_MAX_ACTIONS_PER_REQUEST, 2L)
                        .build();
        requestManager = createRequestManager(configuration);

        Set<StreamUploadableAction> actionSet = setOf(
                StreamUploadableAction.newBuilder().setFeatureContentId(CONTENT_ID).build(),
                StreamUploadableAction.newBuilder().setFeatureContentId(CONTENT_ID_2).build());
        consumer = new RequiredConsumer<>(input -> {
            fakeThreadUtils.checkNotMainThread();
            assertThat(input.isSuccessful()).isTrue();
            assertThat(input.getValue().toByteArray()).isEqualTo(TOKEN_3.toByteArray());
        });
        fakeNetworkClient.addResponse(createHttpResponse(/* responseCode= */ 200, RESPONSE_1))
                .addResponse(createHttpResponse(/* responseCode= */ 200, RESPONSE_2));
        requestManager.triggerUploadActions(actionSet, TOKEN_1, consumer);

        assertThat(consumer.isCalled()).isTrue();
    }

    @Test
    public void testTriggerUploadActions_batchFirstFailure() throws Exception {
        Configuration configuration =
                new Configuration.Builder()
                        .put(ConfigKey.FEED_ACTION_SERVER_METHOD, HttpMethod.GET)
                        .put(ConfigKey.FEED_ACTION_TTL_SECONDS, 1000L)
                        .put(ConfigKey.FEED_ACTION_MAX_UPLOAD_ATTEMPTS, 2L)
                        .put(ConfigKey.FEED_ACTION_SERVER_MAX_SIZE_PER_REQUEST, 20L)
                        .put(ConfigKey.FEED_ACTION_SERVER_MAX_ACTIONS_PER_REQUEST, 2L)
                        .build();
        requestManager = createRequestManager(configuration);
        Set<StreamUploadableAction> actionSet = setOf(
                StreamUploadableAction.newBuilder().setFeatureContentId(CONTENT_ID).build(),
                StreamUploadableAction.newBuilder().setFeatureContentId(CONTENT_ID_2).build());
        consumer = new RequiredConsumer<>(input -> {
            fakeThreadUtils.checkNotMainThread();
            assertThat(input.isSuccessful()).isFalse();
        });
        fakeNetworkClient.addResponse(createHttpResponse(/* responseCode= */ 500, RESPONSE_1));
        requestManager.triggerUploadActions(actionSet, TOKEN_1, consumer);

        assertThat(consumer.isCalled()).isTrue();
    }

    @Test
    public void testTriggerUploadActions_batchFirstSuccessSecondFailure() throws Exception {
        Configuration configuration =
                new Configuration.Builder()
                        .put(ConfigKey.FEED_ACTION_SERVER_METHOD, HttpMethod.GET)
                        .put(ConfigKey.FEED_ACTION_TTL_SECONDS, 1000L)
                        .put(ConfigKey.FEED_ACTION_MAX_UPLOAD_ATTEMPTS, 2L)
                        .put(ConfigKey.FEED_ACTION_SERVER_MAX_SIZE_PER_REQUEST, 20L)
                        .put(ConfigKey.FEED_ACTION_SERVER_MAX_ACTIONS_PER_REQUEST, 2L)
                        .build();
        requestManager = createRequestManager(configuration);
        Set<StreamUploadableAction> actionSet = setOf(
                StreamUploadableAction.newBuilder().setFeatureContentId(CONTENT_ID).build(),
                StreamUploadableAction.newBuilder().setFeatureContentId(CONTENT_ID_2).build());
        consumer = new RequiredConsumer<>(input -> {
            fakeThreadUtils.checkNotMainThread();
            assertThat(input.isSuccessful()).isTrue();
            assertThat(input.getValue().toByteArray()).isEqualTo(TOKEN_2.toByteArray());
        });
        fakeNetworkClient.addResponse(createHttpResponse(/* responseCode= */ 200, RESPONSE_1))
                .addResponse(createHttpResponse(/* responseCode= */ 500, RESPONSE_2));
        requestManager.triggerUploadActions(actionSet, TOKEN_1, consumer);

        assertThat(consumer.isCalled()).isTrue();
    }

    @Test
    public void testTriggerUploadActions_batchFirstReachesMax() throws Exception {
        Configuration configuration =
                new Configuration.Builder()
                        .put(ConfigKey.FEED_ACTION_SERVER_METHOD, HttpMethod.GET)
                        .put(ConfigKey.FEED_ACTION_TTL_SECONDS, 1000L)
                        .put(ConfigKey.FEED_ACTION_MAX_UPLOAD_ATTEMPTS, 2L)
                        .put(ConfigKey.FEED_ACTION_SERVER_MAX_SIZE_PER_REQUEST, 20L)
                        .put(ConfigKey.FEED_ACTION_SERVER_MAX_ACTIONS_PER_REQUEST, 1L)
                        .build();
        requestManager = createRequestManager(configuration);
        Set<StreamUploadableAction> actionSet = setOf(
                StreamUploadableAction.newBuilder().setFeatureContentId(CONTENT_ID).build(),
                StreamUploadableAction.newBuilder().setFeatureContentId(CONTENT_ID_2).build());
        consumer = new RequiredConsumer<>(input -> {
            fakeThreadUtils.checkNotMainThread();
            assertThat(input.isSuccessful()).isTrue();
            assertThat(input.getValue().toByteArray()).isEqualTo(TOKEN_2.toByteArray());
        });
        fakeNetworkClient.addResponse(createHttpResponse(/* responseCode= */ 200, RESPONSE_1));
        requestManager.triggerUploadActions(actionSet, TOKEN_1, consumer);

        assertThat(consumer.isCalled()).isTrue();
    }

    @Test
    public void testTriggerUploadActions_getMethod() throws Exception {
        Configuration configuration =
                new Configuration.Builder()
                        .put(ConfigKey.FEED_ACTION_SERVER_METHOD, HttpMethod.GET)
                        .put(ConfigKey.FEED_ACTION_TTL_SECONDS, 1000L)
                        .put(ConfigKey.FEED_ACTION_MAX_UPLOAD_ATTEMPTS, 2L)
                        .build();
        requestManager = createRequestManager(configuration);
        StreamUploadableAction action =
                StreamUploadableAction.newBuilder().setFeatureContentId(CONTENT_ID).build();
        Set<StreamUploadableAction> actionSet = setOf(action);
        fakeNetworkClient.addResponse(
                createHttpResponse(/* responseCode= */ 200, Response.getDefaultInstance()));
        requestManager.triggerUploadActions(
                actionSet, ConsistencyToken.getDefaultInstance(), consumer);

        HttpRequest httpRequest = fakeNetworkClient.getLatestRequest();
        assertHttpRequestFormattedCorrectly(httpRequest);

        ActionRequest request = getActionRequestFromHttpRequest(httpRequest);
        UploadableActionsRequestBuilder builder =
                new UploadableActionsRequestBuilder(fakeProtocolAdapter);
        ActionRequest expectedRequest =
                builder.setConsistencyToken(ConsistencyToken.getDefaultInstance())
                        .setActions(actionSet)
                        .build();
        assertThat(request.toByteArray()).isEqualTo(expectedRequest.toByteArray());
        assertThat(consumer.isCalled()).isTrue();
        assertThat(fakeStore.getContentById(CONTENT_ID))
                .contains(StreamUploadableAction.newBuilder()
                                  .setFeatureContentId(CONTENT_ID)
                                  .setUploadAttempts(1)
                                  .build());
    }

    @Test
    public void testTriggerUploadActions_defaultMethod() throws Exception {
        Set<StreamUploadableAction> actionSet =
                setOf(StreamUploadableAction.newBuilder().setFeatureContentId(CONTENT_ID).build());
        fakeNetworkClient.addResponse(
                createHttpResponse(/* responseCode= */ 200, Response.getDefaultInstance()));
        requestManager.triggerUploadActions(
                actionSet, ConsistencyToken.getDefaultInstance(), consumer);

        HttpRequest httpRequest = fakeNetworkClient.getLatestRequest();

        ActionRequest request = getActionRequestFromHttpRequestBody(httpRequest);
        UploadableActionsRequestBuilder builder =
                new UploadableActionsRequestBuilder(fakeProtocolAdapter);
        ActionRequest expectedRequest =
                builder.setConsistencyToken(ConsistencyToken.getDefaultInstance())
                        .setActions(actionSet)
                        .build();
        assertThat(request.toByteArray()).isEqualTo(expectedRequest.toByteArray());

        assertThat(consumer.isCalled()).isTrue();
    }

    @Test
    public void testTriggerUploadActions_withSemanticProperties() throws Exception {
        fakeStore.addSemanticProperties(CONTENT_ID, SEMANTIC_PROPERTIES);
        Set<StreamUploadableAction> actionSet =
                setOf(StreamUploadableAction.newBuilder().setFeatureContentId(CONTENT_ID).build());
        fakeNetworkClient.addResponse(
                createHttpResponse(/* responseCode= */ 200, Response.getDefaultInstance()));
        requestManager.triggerUploadActions(
                actionSet, ConsistencyToken.getDefaultInstance(), consumer);

        HttpRequest httpRequest = fakeNetworkClient.getLatestRequest();

        ActionRequest request = getActionRequestFromHttpRequestBody(httpRequest);
        assertThat(request.getExtension(FeedActionRequest.feedActionRequest)
                           .getFeedActionList()
                           .get(0)
                           .getSemanticProperties()
                           .getSemanticPropertiesData())
                .isEqualTo(SEMANTIC_PROPERTIES.getSemanticPropertiesData());
    }

    private static void assertHttpRequestFormattedCorrectly(HttpRequest httpRequest) {
        assertThat(httpRequest.getBody()).hasLength(0);
        assertThat(httpRequest.getMethod()).isEqualTo(HttpMethod.GET);
        assertThat(httpRequest.getUri().getQueryParameter("fmt")).isEqualTo("bin");
        assertThat(httpRequest.getUri().getQueryParameter(RequestHelper.MOTHERSHIP_PARAM_PAYLOAD))
                .isNotNull();
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

    private ActionRequest getActionRequestFromHttpRequest(HttpRequest httpRequest)
            throws Exception {
        return ActionRequest.parseFrom(
                Base64.decode(httpRequest.getUri().getQueryParameter(
                                      RequestHelper.MOTHERSHIP_PARAM_PAYLOAD),
                        Base64.URL_SAFE),
                registry);
    }

    private ActionRequest getActionRequestFromHttpRequestBody(HttpRequest httpRequest)
            throws Exception {
        return ActionRequest.parseFrom(httpRequest.getBody(), registry);
    }

    private FeedActionUploadRequestManager createRequestManager(Configuration configuration) {
        return new FeedActionUploadRequestManager(configuration, fakeNetworkClient,
                fakeProtocolAdapter, new FeedExtensionRegistry(ArrayList::new), fakeTaskQueue,
                fakeThreadUtils, fakeStore, fakeClock);
    }

    private static <T> Set<T> setOf(T... items) {
        Set<T> result = new HashSet<>();
        Collections.addAll(result, items);
        return result;
    }
}
