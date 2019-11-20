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

package com.google.android.libraries.feed.hostimpl.storage;

import static com.google.common.truth.Truth.assertThat;
import static org.mockito.MockitoAnnotations.initMocks;

import android.content.Context;
import androidx.test.core.app.ApplicationProvider;
import com.google.android.libraries.feed.api.internal.common.ThreadUtils;
import com.google.android.libraries.feed.testing.conformance.storage.JournalStorageDirectConformanceTest;
import com.google.common.util.concurrent.MoreExecutors;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.RobolectricTestRunner;

/** Tests for {@link PersistentContentStorage}. */
@RunWith(RobolectricTestRunner.class)
public class PersistentJournalStorageDirectTest extends JournalStorageDirectConformanceTest {

  private Context context;
  @Mock private ThreadUtils threadUtils;

  @Before
  public void setUp() throws Exception {
    initMocks(this);
    context = ApplicationProvider.getApplicationContext();
    journalStorage =
        new PersistentJournalStorage(context, MoreExecutors.directExecutor(), threadUtils, null);
  }

  @After
  public void tearDown() {
    context.getFilesDir().delete();
  }

  @Test
  public void sanitize_and_desanitize() {
    String[] reservedChars = {"|", "\\", "?", "*", "<", "\"", ":", ">"};
    for (String c : reservedChars) {
      String test = "test" + c;
      String sanitized = ((PersistentJournalStorage) journalStorage).sanitize(test);
      assertThat(sanitized.contains(c)).isFalse();
      String unsanitized = ((PersistentJournalStorage) journalStorage).desanitize(sanitized);
      assertThat(unsanitized).isEqualTo(test);
    }
  }
}
