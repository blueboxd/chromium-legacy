// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedrequestmanager;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.MockitoAnnotations.initMocks;

import com.google.protobuf.ByteString;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.internal.common.SemanticPropertiesWithId;
import org.chromium.chrome.browser.feed.library.testing.protocoladapter.FakeProtocolAdapter;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamUploadableAction;
import org.chromium.components.feed.core.proto.wire.ActionPayloadForTestProto.ActionPayloadForTest;
import org.chromium.components.feed.core.proto.wire.ActionPayloadProto.ActionPayload;
import org.chromium.components.feed.core.proto.wire.ActionRequestProto.ActionRequest;
import org.chromium.components.feed.core.proto.wire.ConsistencyTokenProto.ConsistencyToken;
import org.chromium.components.feed.core.proto.wire.ContentIdProto.ContentId;
import org.chromium.components.feed.core.proto.wire.FeedActionProto.FeedAction;
import org.chromium.components.feed.core.proto.wire.FeedActionRequestProto.FeedActionRequest;
import org.chromium.components.feed.core.proto.wire.SemanticPropertiesProto.SemanticProperties;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.Arrays;
import java.util.HashSet;

/** Test of the {@link UploadableActionsRequestBuilder} class. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
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
    private final ActionPayload mPayload =
            ActionPayload.newBuilder()
                    .setExtension(ActionPayloadForTest.actionPayloadForTestExtension,
                            ActionPayloadForTest.newBuilder().setId(CONTENT_ID).build())
                    .build();
    private UploadableActionsRequestBuilder mBuilder;
    private HashSet<StreamUploadableAction> mActionSet = new HashSet<>();
    private ConsistencyToken mToken = ConsistencyToken.newBuilder()
                                              .setToken(ByteString.copyFrom(new byte[] {0x1, 0xf}))
                                              .build();
    private ActionRequest.Builder mRequestBuilder;
    private FeedActionRequest.Builder mFeedActionRequestBuilder;
    private FakeProtocolAdapter mFakeProtocolAdapter;

    @Before
    public void setUp() {
        initMocks(this);
        mFakeProtocolAdapter = new FakeProtocolAdapter();
        mFakeProtocolAdapter.addContentId(CONTENT_ID, ContentId.getDefaultInstance());
        mBuilder = new UploadableActionsRequestBuilder(mFakeProtocolAdapter);
        mActionSet.add(StreamUploadableAction.newBuilder()
                               .setFeatureContentId(CONTENT_ID)
                               .setPayload(mPayload)
                               .setTimestampSeconds(TIME)
                               .build());
        mRequestBuilder = ActionRequest.newBuilder().setRequestVersion(
                ActionRequest.RequestVersion.FEED_UPLOAD_ACTION);
        mFeedActionRequestBuilder = FeedActionRequest.newBuilder();
    }

    @Test
    public void testUploadableActionsRequest_noToken() throws Exception {
        FeedAction feedAction = FeedAction.newBuilder()
                                        .setContentId(ContentId.getDefaultInstance())
                                        .setActionPayload(mPayload)
                                        .setClientData(FeedAction.ClientData.newBuilder()
                                                               .setTimestampSeconds(TIME)
                                                               .build())
                                        .build();
        mFeedActionRequestBuilder.addFeedAction(feedAction);
        mRequestBuilder.setExtension(
                FeedActionRequest.feedActionRequest, mFeedActionRequestBuilder.build());

        ActionRequest expectedResult = mRequestBuilder.build();
        ActionRequest result = mBuilder.setActions(mActionSet).build();
        assertThat(result).isEqualTo(expectedResult);
    }

    @Test
    public void testUploadableActionsRequest_noActions() throws Exception {
        mFeedActionRequestBuilder.setConsistencyToken(mToken);
        mRequestBuilder.setExtension(
                FeedActionRequest.feedActionRequest, mFeedActionRequestBuilder.build());

        ActionRequest expectedResult = mRequestBuilder.build();
        ActionRequest result = mBuilder.setConsistencyToken(mToken).build();
        assertThat(result).isEqualTo(expectedResult);
    }

    @Test
    public void testUploadableActionsRequest() throws Exception {
        FeedAction feedAction = FeedAction.newBuilder()
                                        .setContentId(ContentId.getDefaultInstance())
                                        .setSemanticProperties(SEMANTIC_PROPERTIES)
                                        .setActionPayload(mPayload)
                                        .setClientData(FeedAction.ClientData.newBuilder()
                                                               .setTimestampSeconds(TIME)
                                                               .build())
                                        .build();
        mFeedActionRequestBuilder.addFeedAction(feedAction);
        mFeedActionRequestBuilder.setConsistencyToken(mToken);
        mRequestBuilder.setExtension(
                FeedActionRequest.feedActionRequest, mFeedActionRequestBuilder.build());

        ActionRequest expectedResult = mRequestBuilder.build();
        ActionRequest result =
                mBuilder.setActions(mActionSet)
                        .setConsistencyToken(mToken)
                        .setSemanticProperties(Arrays.asList(SEMANTIC_PROPERTIES_WITH_ID))
                        .build();
        assertThat(result).isEqualTo(expectedResult);
    }
}
