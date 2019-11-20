// Copyright 2018 The Feed Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
package com.google.android.libraries.feed.sharedstream.piet;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import com.google.android.libraries.feed.api.host.logging.BasicLoggingApi;
import com.google.search.now.ui.piet.ErrorsProto.ErrorCode;
import java.util.Collections;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

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
