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

package com.google.android.libraries.feed.sharedstream.contentchanged;

import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.MockitoAnnotations.initMocks;

import com.google.android.libraries.feed.api.client.stream.Stream.ContentChangedListener;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.RobolectricTestRunner;

/**
 * Tests for {@link StreamContentChangedListener}.
 */
@RunWith(RobolectricTestRunner.class)
public class StreamContentChangedListenerTest {
  @Mock ContentChangedListener contentChangedListener;

  StreamContentChangedListener streamContentChangedListener;

  @Before
  public void setup() {
    initMocks(this);
    streamContentChangedListener = new StreamContentChangedListener();
  }

  @Test
  public void testOnContentChanged() {
    streamContentChangedListener.addContentChangedListener(contentChangedListener);

    streamContentChangedListener.onContentChanged();

    verify(contentChangedListener).onContentChanged();
  }

  @Test
  public void testRemoveContentChangedListener() {
    streamContentChangedListener.addContentChangedListener(contentChangedListener);
    streamContentChangedListener.removeContentChangedListener(contentChangedListener);

    streamContentChangedListener.onContentChanged();

    verify(contentChangedListener, never()).onContentChanged();
  }
}
