// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.feedrequestmanager;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.MockitoAnnotations.initMocks;

import com.google.android.libraries.feed.api.internal.common.SemanticPropertiesWithId;
import com.google.android.libraries.feed.testing.protocoladapter.FakeProtocolAdapter;
import com.google.protobuf.ByteString;
import com.google.search.now.feed.client.StreamDataProto.StreamUploadableAction;
import com.google.search.now.wire.feed.ActionPayloadForTestProto.ActionPayloadForTest;
import com.google.search.now.wire.feed.ActionPayloadProto.ActionPayload;
import com.google.search.now.wire.feed.ActionRequestProto.ActionRequest;
import com.google.search.now.wire.feed.ConsistencyTokenProto.ConsistencyToken;
import com.google.search.now.wire.feed.ContentIdProto.ContentId;
import com.google.search.now.wire.feed.FeedActionProto.FeedAction;
import com.google.search.now.wire.feed.FeedActionRequestProto.FeedActionRequest;
import com.google.search.now.wire.feed.SemanticPropertiesProto.SemanticProperties;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

import java.util.Arrays;
import java.util.HashSet;

/** Test of the {@link UploadableActionsRequestBuilder} class. */
@RunWith(RobolectricTestRunner.class)
public class UploadableActionsRequestBuilderTest {
    private static final String CONTENT_ID = "contentId";
    private static final int TIME = 100;
    private static final byte[] SEMANTIC_PROPERTIES_BYTES = new byte[] {0x1, 0xf};
    private static final SemanticProperties SEMANTIC_PROPERTIES =
            SemanticProperties.newBuilder()
                    .setSemanticPropertiesData(ByteString.copyFrom(SEMANTIC_PROPERTIES_BYTES))
                    .build();
    private static final SemanticPropertiesWithId SEMANTIC_PROPERTIES_WITH_ID =
            new SemanticPropertiesWithId(CONTENT_ID, SEMANTIC_PROPERTIES_BYTES);
    private final ActionPayload payload =
            ActionPayload.newBuilder()
                    .setExtension(ActionPayloadForTest.actionPayloadForTestExtension,
                            ActionPayloadForTest.newBuilder().setId(CONTENT_ID).build())
                    .build();
    private UploadableActionsRequestBuilder builder;
    private HashSet<StreamUploadableAction> actionSet = new HashSet<>();
    private ConsistencyToken token = ConsistencyToken.newBuilder()
                                             .setToken(ByteString.copyFrom(new byte[] {0x1, 0xf}))
                                             .build();
    private ActionRequest.Builder requestBuilder;
    private FeedActionRequest.Builder feedActionRequestBuilder;
    private FakeProtocolAdapter fakeProtocolAdapter;

    @Before
    public void setUp() {
        initMocks(this);
        fakeProtocolAdapter = new FakeProtocolAdapter();
        fakeProtocolAdapter.addContentId(CONTENT_ID, ContentId.getDefaultInstance());
        builder = new UploadableActionsRequestBuilder(fakeProtocolAdapter);
        actionSet.add(StreamUploadableAction.newBuilder()
                              .setFeatureContentId(CONTENT_ID)
                              .setPayload(payload)
                              .setTimestampSeconds(TIME)
                              .build());
        requestBuilder = ActionRequest.newBuilder().setRequestVersion(
                ActionRequest.RequestVersion.FEED_UPLOAD_ACTION);
        feedActionRequestBuilder = FeedActionRequest.newBuilder();
    }

    @Test
    public void testUploadableActionsRequest_noToken() throws Exception {
        FeedAction feedAction = FeedAction.newBuilder()
                                        .setContentId(ContentId.getDefaultInstance())
                                        .setActionPayload(payload)
                                        .setClientData(FeedAction.ClientData.newBuilder()
                                                               .setTimestampSeconds(TIME)
                                                               .build())
                                        .build();
        feedActionRequestBuilder.addFeedAction(feedAction);
        requestBuilder.setExtension(
                FeedActionRequest.feedActionRequest, feedActionRequestBuilder.build());

        ActionRequest expectedResult = requestBuilder.build();
        ActionRequest result = builder.setActions(actionSet).build();
        assertThat(result).isEqualTo(expectedResult);
    }

    @Test
    public void testUploadableActionsRequest_noActions() throws Exception {
        feedActionRequestBuilder.setConsistencyToken(token);
        requestBuilder.setExtension(
                FeedActionRequest.feedActionRequest, feedActionRequestBuilder.build());

        ActionRequest expectedResult = requestBuilder.build();
        ActionRequest result = builder.setConsistencyToken(token).build();
        assertThat(result).isEqualTo(expectedResult);
    }

    @Test
    public void testUploadableActionsRequest() throws Exception {
        FeedAction feedAction = FeedAction.newBuilder()
                                        .setContentId(ContentId.getDefaultInstance())
                                        .setSemanticProperties(SEMANTIC_PROPERTIES)
                                        .setActionPayload(payload)
                                        .setClientData(FeedAction.ClientData.newBuilder()
                                                               .setTimestampSeconds(TIME)
                                                               .build())
                                        .build();
        feedActionRequestBuilder.addFeedAction(feedAction);
        feedActionRequestBuilder.setConsistencyToken(token);
        requestBuilder.setExtension(
                FeedActionRequest.feedActionRequest, feedActionRequestBuilder.build());

        ActionRequest expectedResult = requestBuilder.build();
        ActionRequest result =
                builder.setActions(actionSet)
                        .setConsistencyToken(token)
                        .setSemanticProperties(Arrays.asList(SEMANTIC_PROPERTIES_WITH_ID))
                        .build();
        assertThat(result).isEqualTo(expectedResult);
    }
}
