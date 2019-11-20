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

package com.google.android.libraries.feed.testing.conformance.storage;

import static com.google.common.truth.Truth.assertThat;

import com.google.android.libraries.feed.api.host.storage.CommitResult;
import com.google.android.libraries.feed.api.host.storage.ContentMutation;
import com.google.android.libraries.feed.api.host.storage.ContentStorageDirect;
import com.google.android.libraries.feed.common.Result;
import java.nio.charset.Charset;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Map;
import org.junit.Test;

/**
 * Conformance test for {@link ContentStorage}. Hosts who wish to test against this should extend
 * this class and set {@code storage} to the Host implementation.
 */
public abstract class ContentStorageDirectConformanceTest {

  private static final String KEY = "key";
  private static final String KEY_0 = KEY + " 0";
  private static final String KEY_1 = KEY + " 1";
  private static final String OTHER_KEY = "other";
  private static final byte[] DATA_0 = "data 0".getBytes(Charset.forName("UTF-8"));
  private static final byte[] DATA_1 = "data 1".getBytes(Charset.forName("UTF-8"));
  private static final byte[] OTHER_DATA = "other data".getBytes(Charset.forName("UTF-8"));

  protected ContentStorageDirect storage;

  @Test
  public void missingKey() {
    Result<Map<String, byte[]>> result = storage.get(Collections.singletonList(KEY_0));
    Map<String, byte[]> valueMap = result.getValue();
    assertThat(valueMap.get(KEY_0)).isNull();
  }

  @Test
  public void missingKey_multipleKeys() {
    Result<Map<String, byte[]>> result = storage.get(Arrays.asList(KEY_0, KEY_1));
    Map<String, byte[]> valueMap = result.getValue();
    assertThat(valueMap.get(KEY_0)).isNull();
    assertThat(valueMap.get(KEY_1)).isNull();
  }

  @Test
  public void storeAndRetrieve() {
    CommitResult commitResult =
        storage.commit(new ContentMutation.Builder().upsert(KEY_0, DATA_0).build());
    assertThat(commitResult).isEqualTo(CommitResult.SUCCESS);

    Result<Map<String, byte[]>> result = storage.get(Collections.singletonList(KEY_0));
    Map<String, byte[]> input = result.getValue();
    assertThat(input.get(KEY_0)).isEqualTo(DATA_0);
  }

  @Test
  public void storeAndRetrieve_multipleKeys() {
    CommitResult commitResult =
        storage.commit(
            new ContentMutation.Builder().upsert(KEY_0, DATA_0).upsert(KEY_1, DATA_1).build());
    assertThat(commitResult).isEqualTo(CommitResult.SUCCESS);

    Result<Map<String, byte[]>> result = storage.get(Arrays.asList(KEY_0, KEY_1));
    Map<String, byte[]> input = result.getValue();
    assertThat(input.get(KEY_0)).isEqualTo(DATA_0);
    assertThat(input.get(KEY_1)).isEqualTo(DATA_1);
  }

  @Test
  public void storeAndOverwrite_chained() {
    CommitResult commitResult =
        storage.commit(
            new ContentMutation.Builder().upsert(KEY_0, DATA_0).upsert(KEY_0, DATA_1).build());
    assertThat(commitResult).isEqualTo(CommitResult.SUCCESS);

    Result<Map<String, byte[]>> result = storage.get(Collections.singletonList(KEY_0));
    Map<String, byte[]> valueMap = result.getValue();
    assertThat(valueMap.get(KEY_0)).isEqualTo(DATA_1);
  }

  @Test
  public void storeAndOverwrite_separate() {
    CommitResult commitResult =
        storage.commit(new ContentMutation.Builder().upsert(KEY_0, DATA_0).build());
    assertThat(commitResult).isEqualTo(CommitResult.SUCCESS);

    Result<Map<String, byte[]>> result = storage.get(Collections.singletonList(KEY_0));
    Map<String, byte[]> valueMap = result.getValue();
    assertThat(valueMap.get(KEY_0)).isEqualTo(DATA_0);

    commitResult = storage.commit(new ContentMutation.Builder().upsert(KEY_0, DATA_1).build());
    assertThat(commitResult).isEqualTo(CommitResult.SUCCESS);

    result = storage.get(Collections.singletonList(KEY_0));
    valueMap = result.getValue();
    assertThat(valueMap.get(KEY_0)).isEqualTo(DATA_1);
  }

  @Test
  public void storeAndDelete() {
    CommitResult commitResult =
        storage.commit(
            new ContentMutation.Builder().upsert(KEY_0, DATA_0).upsert(KEY_1, DATA_1).build());
    assertThat(commitResult).isEqualTo(CommitResult.SUCCESS);

    // Confirm Key 0 and 1 are present
    Result<Map<String, byte[]>> result = storage.get(Arrays.asList(KEY_0, KEY_1));
    Map<String, byte[]> input = result.getValue();
    assertThat(input.get(KEY_0)).isEqualTo(DATA_0);
    assertThat(input.get(KEY_1)).isEqualTo(DATA_1);

    // Delete Key 0
    commitResult = storage.commit(new ContentMutation.Builder().delete(KEY_0).build());
    assertThat(commitResult).isEqualTo(CommitResult.SUCCESS);

    // Confirm that Key 0 is deleted and Key 1 is present
    Result<Map<String, byte[]>> result2 = storage.get(Arrays.asList(KEY_0, KEY_1));
    Map<String, byte[]> valueMap = result2.getValue();
    assertThat(valueMap.get(KEY_0)).isNull();
  }

  @Test
  public void storeAndDeleteByPrefix() {
    CommitResult commitResult =
        storage.commit(
            new ContentMutation.Builder()
                .upsert(KEY_0, DATA_0)
                .upsert(KEY_1, DATA_1)
                .upsert(OTHER_KEY, OTHER_DATA)
                .build());
    assertThat(commitResult).isEqualTo(CommitResult.SUCCESS);

    // Confirm Key 0, Key 1, and Other are present
    Result<Map<String, byte[]>> result = storage.get(Arrays.asList(KEY_0, KEY_1, OTHER_KEY));
    Map<String, byte[]> valueMap = result.getValue();
    assertThat(valueMap.get(KEY_0)).isEqualTo(DATA_0);
    assertThat(valueMap.get(KEY_1)).isEqualTo(DATA_1);
    assertThat(valueMap.get(OTHER_KEY)).isEqualTo(OTHER_DATA);

    // Delete by prefix Key
    commitResult = storage.commit(new ContentMutation.Builder().deleteByPrefix(KEY).build());
    assertThat(commitResult).isEqualTo(CommitResult.SUCCESS);

    // Confirm Key 0 and Key 1 are deleted, and Other is present
    result = storage.get(Arrays.asList(KEY_0, KEY_1, OTHER_KEY));
    valueMap = result.getValue();
    assertThat(valueMap.get(KEY_0)).isNull();
    assertThat(valueMap.get(KEY_1)).isNull();
    assertThat(valueMap.get(OTHER_KEY)).isEqualTo(OTHER_DATA);
  }

  @Test
  public void storeAndDeleteAll() {
    CommitResult commitResult =
        storage.commit(
            new ContentMutation.Builder()
                .upsert(KEY_0, DATA_0)
                .upsert(KEY_1, DATA_1)
                .upsert(OTHER_KEY, OTHER_DATA)
                .build());
    assertThat(commitResult).isEqualTo(CommitResult.SUCCESS);

    // Confirm Key 0, Key 1, and Other are present
    Result<Map<String, byte[]>> result = storage.get(Arrays.asList(KEY_0, KEY_1, OTHER_KEY));
    Map<String, byte[]> valueMap = result.getValue();
    assertThat(valueMap.get(KEY_0)).isEqualTo(DATA_0);
    assertThat(valueMap.get(KEY_1)).isEqualTo(DATA_1);
    assertThat(valueMap.get(OTHER_KEY)).isEqualTo(OTHER_DATA);

    // Delete all
    commitResult = storage.commit(new ContentMutation.Builder().deleteAll().build());
    assertThat(commitResult).isEqualTo(CommitResult.SUCCESS);

    // Confirm all keys are deleted
    result = storage.get(Arrays.asList(KEY_0, KEY_1, OTHER_KEY));
    valueMap = result.getValue();
    assertThat(valueMap.get(KEY_0)).isNull();
    assertThat(valueMap.get(KEY_1)).isNull();
    assertThat(valueMap.get(OTHER_KEY)).isNull();
  }

  @Test
  public void multipleValues_getAll() {
    CommitResult commitResult =
        storage.commit(
            new ContentMutation.Builder().upsert(KEY_0, DATA_0).upsert(KEY_1, DATA_1).build());
    assertThat(commitResult).isEqualTo(CommitResult.SUCCESS);

    Result<Map<String, byte[]>> result = storage.getAll(KEY);
    Map<String, byte[]> input = result.getValue();
    assertThat(input.get(KEY_0)).isEqualTo(DATA_0);
    assertThat(input.get(KEY_1)).isEqualTo(DATA_1);
  }

  @Test
  public void multipleValues_getAllKeys() {
    CommitResult commitResult =
        storage.commit(
            new ContentMutation.Builder().upsert(KEY_0, DATA_0).upsert(KEY_1, DATA_1).build());
    assertThat(commitResult).isEqualTo(CommitResult.SUCCESS);

    Result<List<String>> result = storage.getAllKeys();
    assertThat(result.getValue()).containsExactly(KEY_0, KEY_1);
  }
}
