// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package com.google.android.libraries.feed.sharedstream.piet;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import com.google.android.libraries.feed.api.host.logging.BasicLoggingApi;
import com.google.search.now.ui.piet.ErrorsProto.ErrorCode;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

import java.util.Collections;

/** Tests for {@link PietEventLogger}. */
@RunWith(RobolectricTestRunner.class)
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
