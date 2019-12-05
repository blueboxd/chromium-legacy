// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.infraintegration;

import static com.google.common.truth.Truth.assertThat;

import com.google.protobuf.ByteString;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.client.requestmanager.RequestManager;
import org.chromium.chrome.browser.feed.library.api.internal.common.SemanticPropertiesWithId;
import org.chromium.chrome.browser.feed.library.api.internal.common.testing.ContentIdGenerators;
import org.chromium.chrome.browser.feed.library.api.internal.store.Store;
import org.chromium.chrome.browser.feed.library.common.Result;
import org.chromium.chrome.browser.feed.library.common.testing.InfraIntegrationScope;
import org.chromium.chrome.browser.feed.library.common.testing.ResponseBuilder;
import org.chromium.chrome.browser.feed.library.testing.requestmanager.FakeFeedRequestManager;
import org.chromium.components.feed.core.proto.wire.ContentIdProto.ContentId;
import org.chromium.components.feed.core.proto.wire.ResponseProto.Response;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.Collections;
import java.util.List;

/** Tests around Semantic Properties */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SemanticPropertiesTest {
    private final InfraIntegrationScope mScope = new InfraIntegrationScope.Builder().build();
    private final FakeFeedRequestManager mFakeFeedRequestManager =
            mScope.getFakeFeedRequestManager();
    private final RequestManager mRequestManager = mScope.getRequestManager();
    private final Store mStore = mScope.getStore();

    @Test
    public void persistingSemanticProperties() {
        ContentId contentId = ResponseBuilder.createFeatureContentId(13);
        ByteString semanticData = ByteString.copyFromUtf8("helloWorld");

        Response response = new ResponseBuilder()
                                    .addClearOperation()
                                    .addCardWithSemanticData(contentId, semanticData)
                                    .build();
        mFakeFeedRequestManager.queueResponse(response);
        mRequestManager.triggerScheduledRefresh();

        mScope.getFakeThreadUtils().enforceMainThread(false);
        ContentIdGenerators idGenerators = new ContentIdGenerators();
        String contentIdString = idGenerators.createContentId(contentId);
        Result<List<SemanticPropertiesWithId>> semanticPropertiesResult =
                mStore.getSemanticProperties(Collections.singletonList(contentIdString));
        assertThat(semanticPropertiesResult.isSuccessful()).isTrue();
        List<SemanticPropertiesWithId> semanticProperties = semanticPropertiesResult.getValue();
        assertThat(semanticProperties).hasSize(1);
        assertThat(semanticProperties.get(0).contentId).isEqualTo(contentIdString);
        assertThat(semanticProperties.get(0).semanticData).isEqualTo(semanticData.toByteArray());
    }
}
