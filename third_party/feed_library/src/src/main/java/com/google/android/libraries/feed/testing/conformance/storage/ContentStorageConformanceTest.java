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
import com.google.android.libraries.feed.api.host.storage.ContentStorage;
import com.google.android.libraries.feed.common.Result;
import com.google.android.libraries.feed.common.functional.Consumer;
import com.google.android.libraries.feed.common.testing.RequiredConsumer;
import java.nio.charset.Charset;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Map;
import org.junit.Before;
import org.junit.Test;

/**
 * Conformance test for {@link ContentStorage}. Hosts who wish to test against this should extend
 * this class and set {@code storage} to the Host implementation.
 */
public abstract class ContentStorageConformanceTest {

  private static final String KEY = "key";
  private static final String KEY_0 = KEY + " 0";
  private static final String KEY_1 = KEY + " 1";
  private static final String OTHER_KEY = "other";
  private static final byte[] DATA_0 = "data 0".getBytes(Charset.forName("UTF-8"));
  private static final byte[] DATA_1 = "data 1".getBytes(Charset.forName("UTF-8"));
  private static final byte[] OTHER_DATA = "other data".getBytes(Charset.forName("UTF-8"));

  // Helper consumers to make tests cleaner
  private final Consumer<Result<Map<String, byte[]>>> isKey0Data0 =
      input -> {
        assertThat(input.isSuccessful()).isTrue();
        Map<String, byte[]> valueMap = input.getValue();
        assertThat(valueMap.get(KEY_0)).isEqualTo(DATA_0);
      };
  private final Consumer<Result<Map<String, byte[]>>> isKey0EmptyData =
      input -> {
        assertThat(input.isSuccessful()).isTrue();
        Map<String, byte[]> valueMap = input.getValue();
        assertThat(valueMap.get(KEY_0)).isNull();
      };
  private final Consumer<Result<Map<String, byte[]>>> isKey0Data1 =
      input -> {
        assertThat(input.isSuccessful()).isTrue();
        Map<String, byte[]> valueMap = input.getValue();
        assertThat(valueMap.get(KEY_0)).isEqualTo(DATA_1);
      };

  private final Consumer<Result<Map<String, byte[]>>> isKey0Data0Key1Data1 =
      result -> {
        assertThat(result.isSuccessful()).isTrue();
        Map<String, byte[]> input = result.getValue();
        assertThat(input.get(KEY_0)).isEqualTo(DATA_0);
        assertThat(input.get(KEY_1)).isEqualTo(DATA_1);
      };
  private final Consumer<Result<Map<String, byte[]>>> isKey0EmptyDataKey1EmptyData =
      input -> {
        assertThat(input.isSuccessful()).isTrue();
        Map<String, byte[]> valueMap = input.getValue();
        assertThat(valueMap.get(KEY_0)).isNull();
        assertThat(valueMap.get(KEY_1)).isNull();
      };
  private final Consumer<Result<Map<String, byte[]>>>
      isKey0EmptyDataKey1EmptyDataOtherKeyEmptyData =
          input -> {
            assertThat(input.isSuccessful()).isTrue();
            Map<String, byte[]> valueMap = input.getValue();
            assertThat(valueMap.get(KEY_0)).isNull();
            assertThat(valueMap.get(KEY_1)).isNull();
            assertThat(valueMap.get(OTHER_KEY)).isNull();
          };

  private final Consumer<Result<Map<String, byte[]>>> isKey0Data0Key1Data1OtherKeyOtherData =
      result -> {
        assertThat(result.isSuccessful()).isTrue();
        Map<String, byte[]> input = result.getValue();
        assertThat(input.get(KEY_0)).isEqualTo(DATA_0);
        assertThat(input.get(KEY_1)).isEqualTo(DATA_1);
        assertThat(input.get(OTHER_KEY)).isEqualTo(OTHER_DATA);
      };
  private final Consumer<Result<Map<String, byte[]>>>
      isKey0EmptyDataKey1EmptyDataOtherKeyOtherData =
          result -> {
            assertThat(result.isSuccessful()).isTrue();
            Map<String, byte[]> input = result.getValue();
            assertThat(input.get(KEY_0)).isNull();
            assertThat(input.get(KEY_1)).isNull();
            assertThat(input.get(OTHER_KEY)).isEqualTo(OTHER_DATA);
          };

  private final Consumer<CommitResult> isSuccess =
      input -> assertThat(input).isEqualTo(CommitResult.SUCCESS);

  private final Consumer<Result<List<String>>> isKey0Key1 =
      result -> {
        assertThat(result.isSuccessful()).isTrue();
        assertThat(result.getValue()).containsExactly(KEY_0, KEY_1);
      };

  protected ContentStorage storage;

  private RequiredConsumer<CommitResult> assertSuccessConsumer;

  @Before
  public final void testSetup() {
    assertSuccessConsumer = new RequiredConsumer<>(isSuccess);
  }

  @Test
  public void missingKey() {
    RequiredConsumer<Result<Map<String, byte[]>>> consumer =
        new RequiredConsumer<>(isKey0EmptyData);
    storage.get(Collections.singletonList(KEY_0), consumer);
    assertThat(consumer.isCalled()).isTrue();
  }

  @Test
  public void missingKey_multipleKeys() {
    RequiredConsumer<Result<Map<String, byte[]>>> consumer =
        new RequiredConsumer<>(isKey0EmptyDataKey1EmptyData);
    storage.get(Arrays.asList(KEY_0, KEY_1), consumer);
    assertThat(consumer.isCalled()).isTrue();
  }

  @Test
  public void storeAndRetrieve() {
    storage.commit(
        new ContentMutation.Builder().upsert(KEY_0, DATA_0).build(), assertSuccessConsumer);
    assertThat(assertSuccessConsumer.isCalled()).isTrue();
    RequiredConsumer<Result<Map<String, byte[]>>> byteConsumer =
        new RequiredConsumer<>(isKey0Data0);
    storage.get(Collections.singletonList(KEY_0), byteConsumer);
    assertThat(byteConsumer.isCalled()).isTrue();
  }

  @Test
  public void storeAndRetrieve_multipleKeys() {
    storage.commit(
        new ContentMutation.Builder().upsert(KEY_0, DATA_0).upsert(KEY_1, DATA_1).build(),
        assertSuccessConsumer);
    assertThat(assertSuccessConsumer.isCalled()).isTrue();
    RequiredConsumer<Result<Map<String, byte[]>>> byteConsumer =
        new RequiredConsumer<>(isKey0Data0Key1Data1);
    storage.get(Arrays.asList(KEY_0, KEY_1), byteConsumer);
    assertThat(byteConsumer.isCalled()).isTrue();
  }

  @Test
  public void storeAndOverwrite_chained() {
    storage.commit(
        new ContentMutation.Builder().upsert(KEY_0, DATA_0).upsert(KEY_0, DATA_1).build(),
        assertSuccessConsumer);
    assertThat(assertSuccessConsumer.isCalled()).isTrue();
    RequiredConsumer<Result<Map<String, byte[]>>> byteConsumer =
        new RequiredConsumer<>(isKey0Data1);
    storage.get(Collections.singletonList(KEY_0), byteConsumer);
    assertThat(byteConsumer.isCalled()).isTrue();
  }

  @Test
  public void storeAndOverwrite_separate() {
    storage.commit(
        new ContentMutation.Builder().upsert(KEY_0, DATA_0).build(), assertSuccessConsumer);
    assertThat(assertSuccessConsumer.isCalled()).isTrue();

    RequiredConsumer<Result<Map<String, byte[]>>> byteConsumer =
        new RequiredConsumer<>(isKey0Data0);
    storage.get(Collections.singletonList(KEY_0), byteConsumer);
    assertThat(byteConsumer.isCalled()).isTrue();

    // Reset assertSuccessConsumer
    assertSuccessConsumer =
        new RequiredConsumer<>(
            input -> {
              assertThat(input).isEqualTo(CommitResult.SUCCESS);
            });
    storage.commit(
        new ContentMutation.Builder().upsert(KEY_0, DATA_1).build(), assertSuccessConsumer);
    assertThat(assertSuccessConsumer.isCalled()).isTrue();

    RequiredConsumer<Result<Map<String, byte[]>>> byteConsumer2 =
        new RequiredConsumer<>(isKey0Data1);
    storage.get(Collections.singletonList(KEY_0), byteConsumer2);
    assertThat(byteConsumer2.isCalled()).isTrue();
  }

  @Test
  public void storeAndDelete() {
    storage.commit(
        new ContentMutation.Builder().upsert(KEY_0, DATA_0).upsert(KEY_1, DATA_1).build(),
        assertSuccessConsumer);
    assertThat(assertSuccessConsumer.isCalled()).isTrue();

    // Confirm Key 0 and 1 are present
    RequiredConsumer<Result<Map<String, byte[]>>> byteConsumer =
        new RequiredConsumer<>(isKey0Data0Key1Data1);
    storage.get(Arrays.asList(KEY_0, KEY_1), byteConsumer);
    assertThat(byteConsumer.isCalled()).isTrue();

    // Delete Key 0
    RequiredConsumer<CommitResult> deleteConsumer = new RequiredConsumer<>(isSuccess);
    storage.commit(new ContentMutation.Builder().delete(KEY_0).build(), deleteConsumer);
    assertThat(deleteConsumer.isCalled()).isTrue();

    // Confirm that Key 0 is deleted and Key 1 is present
    byteConsumer = new RequiredConsumer<>(isKey0EmptyData);
    storage.get(Arrays.asList(KEY_0, KEY_1), byteConsumer);
    assertThat(byteConsumer.isCalled()).isTrue();
  }

  @Test
  public void storeAndDeleteByPrefix() {
    storage.commit(
        new ContentMutation.Builder()
            .upsert(KEY_0, DATA_0)
            .upsert(KEY_1, DATA_1)
            .upsert(OTHER_KEY, OTHER_DATA)
            .build(),
        assertSuccessConsumer);
    assertThat(assertSuccessConsumer.isCalled()).isTrue();

    // Confirm Key 0, Key 1, and Other are present
    RequiredConsumer<Result<Map<String, byte[]>>> byteConsumer =
        new RequiredConsumer<>(isKey0Data0Key1Data1OtherKeyOtherData);
    storage.get(Arrays.asList(KEY_0, KEY_1, OTHER_KEY), byteConsumer);
    assertThat(byteConsumer.isCalled()).isTrue();

    // Delete by prefix Key
    RequiredConsumer<CommitResult> deleteConsumer = new RequiredConsumer<>(isSuccess);
    storage.commit(new ContentMutation.Builder().deleteByPrefix(KEY).build(), deleteConsumer);

    // Confirm Key 0 and Key 1 are deleted, and Other is present
    byteConsumer = new RequiredConsumer<>(isKey0EmptyDataKey1EmptyDataOtherKeyOtherData);
    storage.get(Arrays.asList(KEY_0, KEY_1, OTHER_KEY), byteConsumer);
    assertThat(byteConsumer.isCalled()).isTrue();
  }

  @Test
  public void storeAndDeleteAll() {
    storage.commit(
        new ContentMutation.Builder()
            .upsert(KEY_0, DATA_0)
            .upsert(KEY_1, DATA_1)
            .upsert(OTHER_KEY, OTHER_DATA)
            .build(),
        assertSuccessConsumer);
    assertThat(assertSuccessConsumer.isCalled()).isTrue();

    // Confirm Key 0, Key 1, and Other are present
    RequiredConsumer<Result<Map<String, byte[]>>> byteConsumer =
        new RequiredConsumer<>(isKey0Data0Key1Data1OtherKeyOtherData);
    storage.get(Arrays.asList(KEY_0, KEY_1, OTHER_KEY), byteConsumer);
    assertThat(byteConsumer.isCalled()).isTrue();

    // Delete all
    RequiredConsumer<CommitResult> deleteConsumer = new RequiredConsumer<>(isSuccess);
    storage.commit(new ContentMutation.Builder().deleteAll().build(), deleteConsumer);

    // Confirm all keys are deleted
    byteConsumer = new RequiredConsumer<>(isKey0EmptyDataKey1EmptyDataOtherKeyEmptyData);
    storage.get(Arrays.asList(KEY_0, KEY_1, OTHER_KEY), byteConsumer);
    assertThat(byteConsumer.isCalled()).isTrue();
  }

  @Test
  public void multipleValues_getAll() {
    storage.commit(
        new ContentMutation.Builder().upsert(KEY_0, DATA_0).upsert(KEY_1, DATA_1).build(),
        assertSuccessConsumer);
    assertThat(assertSuccessConsumer.isCalled()).isTrue();

    RequiredConsumer<Result<Map<String, byte[]>>> mapConsumer =
        new RequiredConsumer<>(isKey0Data0Key1Data1);
    storage.getAll(KEY, mapConsumer);
    assertThat(mapConsumer.isCalled()).isTrue();
  }

  @Test
  public void multipleValues_getAllKeys() {
    storage.commit(
        new ContentMutation.Builder().upsert(KEY_0, DATA_0).upsert(KEY_1, DATA_1).build(),
        assertSuccessConsumer);
    assertThat(assertSuccessConsumer.isCalled()).isTrue();

    RequiredConsumer<Result<List<String>>> listConsumer = new RequiredConsumer<>(isKey0Key1);
    storage.getAllKeys(listConsumer);
    assertThat(listConsumer.isCalled()).isTrue();
  }
}
