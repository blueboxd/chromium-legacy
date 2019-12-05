// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.feed.library.sharedstream.piet;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.host.logging.BasicLoggingApi;
import org.chromium.components.feed.core.proto.ui.piet.ErrorsProto.ErrorCode;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.Collections;

/** Tests for {@link PietEventLogger}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PietEventLoggerTest {
    @Test
    public void testLogging() {
        BasicLoggingApi basicLoggingApi = mock(BasicLoggingApi.class);
        PietEventLogger logger = new PietEventLogger(basicLoggingApi);

        logger.logEvents(Collections.singletonList(ErrorCode.ERR_MISSING_BINDING_VALUE));

        verify(basicLoggingApi)
                .onPietFrameRenderingEvent(
                        Collections.singletonList(ErrorCode.ERR_MISSING_BINDING_VALUE_VALUE));
    }
}
