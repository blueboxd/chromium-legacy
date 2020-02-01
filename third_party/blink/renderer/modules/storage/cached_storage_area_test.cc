// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/storage/cached_storage_area.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/modules/storage/testing/fake_area_source.h"
#include "third_party/blink/renderer/modules/storage/testing/mock_storage_area.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

using FormatOption = CachedStorageArea::FormatOption;

class CachedStorageAreaTest : public testing::Test,
                              public CachedStorageArea::InspectorEventListener {
 public:
  const scoped_refptr<SecurityOrigin> kOrigin =
      SecurityOrigin::CreateFromString("http://dom_storage/");
  const String kKey = "key";
  const String kValue = "value";
  const String kValue2 = "another value";
  const KURL kPageUrl = KURL("http://dom_storage/page");
  const KURL kPageUrl2 = KURL("http://dom_storage/other_page");
  const String kRemoteSourceId = "1234";
  const String kRemoteSource = kPageUrl2.GetString() + "\n" + kRemoteSourceId;

  void SetUp() override {
    if (IsSessionStorage()) {
      cached_area_ = CachedStorageArea::CreateForSessionStorage(
          kOrigin, mock_storage_area_.GetAssociatedInterfaceRemote(),
          scheduler::GetSingleThreadTaskRunnerForTesting(), this);
    } else {
      cached_area_ = CachedStorageArea::CreateForLocalStorage(
          kOrigin, mock_storage_area_.GetInterfaceRemote(),
          scheduler::GetSingleThreadTaskRunnerForTesting(), this);
    }
    source_area_ = MakeGarbageCollected<FakeAreaSource>(kPageUrl);
    source_area_id_ = cached_area_->RegisterSource(source_area_);
    source_ = kPageUrl.GetString() + "\n" + source_area_id_;
    source_area2_ = MakeGarbageCollected<FakeAreaSource>(kPageUrl2);
    cached_area_->RegisterSource(source_area2_);
  }

  void DidDispatchStorageEvent(const SecurityOrigin* origin,
                               const String& key,
                               const String& old_value,
                               const String& new_value) override {}

  virtual bool IsSessionStorage() { return false; }

  bool IsCacheLoaded() { return cached_area_->map_.get(); }

  bool IsIgnoringKeyMutations(const String& key) {
    return cached_area_->pending_mutations_by_key_.Contains(key);
  }

  static Vector<uint8_t> StringToUint8Vector(const String& input,
                                             FormatOption format) {
    return CachedStorageArea::StringToUint8Vector(input, format);
  }

  static String Uint8VectorToString(const Vector<uint8_t>& input,
                                    FormatOption format) {
    return CachedStorageArea::Uint8VectorToString(input, format);
  }

  Vector<uint8_t> KeyToUint8Vector(const String& key) {
    return StringToUint8Vector(
        key, IsSessionStorage() ? FormatOption::kSessionStorageForceUTF8
                                : FormatOption::kLocalStorageDetectFormat);
  }

  Vector<uint8_t> ValueToUint8Vector(const String& value) {
    return StringToUint8Vector(
        value, IsSessionStorage() ? FormatOption::kSessionStorageForceUTF16
                                  : FormatOption::kLocalStorageDetectFormat);
  }

  String KeyFromUint8Vector(const Vector<uint8_t>& key) {
    return Uint8VectorToString(
        key, IsSessionStorage() ? FormatOption::kSessionStorageForceUTF8
                                : FormatOption::kLocalStorageDetectFormat);
  }

  String ValueFromUint8Vector(const Vector<uint8_t>& value) {
    return Uint8VectorToString(
        value, IsSessionStorage() ? FormatOption::kSessionStorageForceUTF16
                                  : FormatOption::kLocalStorageDetectFormat);
  }

 protected:
  MockStorageArea mock_storage_area_;
  Persistent<FakeAreaSource> source_area_;
  Persistent<FakeAreaSource> source_area2_;
  scoped_refptr<CachedStorageArea> cached_area_;
  String source_area_id_;
  String source_;
};

class CachedStorageAreaTestWithParam
    : public CachedStorageAreaTest,
      public testing::WithParamInterface<bool> {
 public:
  bool IsSessionStorage() override { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(CachedStorageAreaTest,
                         CachedStorageAreaTestWithParam,
                         ::testing::Bool());

TEST_P(CachedStorageAreaTestWithParam, Basics) {
  EXPECT_FALSE(IsCacheLoaded());

  EXPECT_EQ(0u, cached_area_->GetLength());
  EXPECT_TRUE(cached_area_->SetItem(kKey, kValue, source_area_));
  EXPECT_EQ(1u, cached_area_->GetLength());
  EXPECT_EQ(kKey, cached_area_->GetKey(0));
  EXPECT_EQ(kValue, cached_area_->GetItem(kKey));
  cached_area_->RemoveItem(kKey, source_area_);
  EXPECT_EQ(0u, cached_area_->GetLength());

  mock_storage_area_.Flush();
  EXPECT_EQ(2u, mock_storage_area_.observer_count());
}

TEST_P(CachedStorageAreaTestWithParam, GetLength) {
  // Expect GetLength to load the cache.
  EXPECT_FALSE(IsCacheLoaded());
  EXPECT_EQ(0u, cached_area_->GetLength());
  EXPECT_TRUE(IsCacheLoaded());
  EXPECT_TRUE(mock_storage_area_.observed_get_all());
}

TEST_P(CachedStorageAreaTestWithParam, GetKey) {
  // Expect GetKey to load the cache.
  EXPECT_FALSE(IsCacheLoaded());
  EXPECT_TRUE(cached_area_->GetKey(2).IsNull());
  EXPECT_TRUE(IsCacheLoaded());
  EXPECT_TRUE(mock_storage_area_.observed_get_all());
}

TEST_P(CachedStorageAreaTestWithParam, GetItem) {
  // Expect GetItem to load the cache.
  EXPECT_FALSE(IsCacheLoaded());
  EXPECT_TRUE(cached_area_->GetItem(kKey).IsNull());
  EXPECT_TRUE(IsCacheLoaded());
  EXPECT_TRUE(mock_storage_area_.observed_get_all());
}

TEST_P(CachedStorageAreaTestWithParam, SetItem) {
  // Expect SetItem to load the cache and then generate a change event.
  EXPECT_FALSE(IsCacheLoaded());
  EXPECT_TRUE(cached_area_->SetItem(kKey, kValue, source_area_));
  EXPECT_TRUE(IsCacheLoaded());
  EXPECT_TRUE(mock_storage_area_.observed_get_all());

  mock_storage_area_.Flush();
  EXPECT_TRUE(mock_storage_area_.observed_put());
  EXPECT_EQ(source_, mock_storage_area_.observed_source());
  EXPECT_EQ(KeyToUint8Vector(kKey), mock_storage_area_.observed_key());
  EXPECT_EQ(ValueToUint8Vector(kValue), mock_storage_area_.observed_value());

  EXPECT_TRUE(source_area_->events.IsEmpty());
  if (IsSessionStorage()) {
    ASSERT_EQ(1u, source_area2_->events.size());
    EXPECT_EQ(kKey, source_area2_->events[0].key);
    EXPECT_TRUE(source_area2_->events[0].old_value.IsNull());
    EXPECT_EQ(kValue, source_area2_->events[0].new_value);
    EXPECT_EQ(kPageUrl, source_area2_->events[0].url);
  } else {
    EXPECT_TRUE(source_area2_->events.IsEmpty());
  }
}

TEST_P(CachedStorageAreaTestWithParam, Clear_AlreadyEmpty) {
  // Clear, we expect just the one call to clear in the db since
  // there's no need to load the data prior to deleting it.
  // Except if we're testing session storage, in which case we also expect a
  // load call first, since it needs that for event dispatching.
  EXPECT_FALSE(IsCacheLoaded());
  cached_area_->Clear(source_area_);
  mock_storage_area_.Flush();
  EXPECT_TRUE(IsCacheLoaded());
  EXPECT_TRUE(mock_storage_area_.observed_delete_all());
  EXPECT_EQ(source_, mock_storage_area_.observed_source());
  if (IsSessionStorage()) {
    EXPECT_TRUE(mock_storage_area_.observed_get_all());
  } else {
    EXPECT_FALSE(mock_storage_area_.observed_get_all());
  }

  // Neither should have events since area was already empty.
  EXPECT_TRUE(source_area_->events.IsEmpty());
  EXPECT_TRUE(source_area2_->events.IsEmpty());
}

TEST_P(CachedStorageAreaTestWithParam, Clear_WithData) {
  mock_storage_area_.mutable_get_all_return_values().push_back(
      mojom::blink::KeyValue::New(KeyToUint8Vector(kKey),
                                  ValueToUint8Vector(kValue)));

  EXPECT_FALSE(IsCacheLoaded());
  cached_area_->Clear(source_area_);
  mock_storage_area_.Flush();
  EXPECT_TRUE(IsCacheLoaded());
  EXPECT_TRUE(mock_storage_area_.observed_delete_all());
  EXPECT_EQ(source_, mock_storage_area_.observed_source());
  if (IsSessionStorage()) {
    EXPECT_TRUE(mock_storage_area_.observed_get_all());
  } else {
    EXPECT_FALSE(mock_storage_area_.observed_get_all());
  }

  EXPECT_TRUE(source_area_->events.IsEmpty());
  if (IsSessionStorage()) {
    ASSERT_EQ(1u, source_area2_->events.size());
    EXPECT_TRUE(source_area2_->events[0].key.IsNull());
    EXPECT_TRUE(source_area2_->events[0].old_value.IsNull());
    EXPECT_TRUE(source_area2_->events[0].new_value.IsNull());
    EXPECT_EQ(kPageUrl, source_area2_->events[0].url);
  } else {
    EXPECT_TRUE(source_area2_->events.IsEmpty());
  }
}

TEST_P(CachedStorageAreaTestWithParam, RemoveItem_NothingToRemove) {
  // RemoveItem with nothing to remove, expect just one call to load.
  EXPECT_FALSE(IsCacheLoaded());
  cached_area_->RemoveItem(kKey, source_area_);
  mock_storage_area_.Flush();
  EXPECT_TRUE(IsCacheLoaded());
  EXPECT_TRUE(mock_storage_area_.observed_get_all());
  EXPECT_FALSE(mock_storage_area_.observed_delete());

  // Neither should have events since area was already empty.
  EXPECT_TRUE(source_area_->events.IsEmpty());
  EXPECT_TRUE(source_area2_->events.IsEmpty());
}

TEST_P(CachedStorageAreaTestWithParam, RemoveItem) {
  // RemoveItem with something to remove, expect a call to load followed
  // by a call to remove.
  mock_storage_area_.mutable_get_all_return_values().push_back(
      mojom::blink::KeyValue::New(KeyToUint8Vector(kKey),
                                  ValueToUint8Vector(kValue)));
  EXPECT_FALSE(IsCacheLoaded());
  cached_area_->RemoveItem(kKey, source_area_);
  mock_storage_area_.Flush();
  EXPECT_TRUE(IsCacheLoaded());
  EXPECT_TRUE(mock_storage_area_.observed_get_all());
  EXPECT_TRUE(mock_storage_area_.observed_delete());
  EXPECT_EQ(source_, mock_storage_area_.observed_source());
  EXPECT_EQ(KeyToUint8Vector(kKey), mock_storage_area_.observed_key());

  EXPECT_TRUE(source_area_->events.IsEmpty());
  if (IsSessionStorage()) {
    ASSERT_EQ(1u, source_area2_->events.size());
    EXPECT_EQ(kKey, source_area2_->events[0].key);
    EXPECT_EQ(kValue, source_area2_->events[0].old_value);
    EXPECT_TRUE(source_area2_->events[0].new_value.IsNull());
    EXPECT_EQ(kPageUrl, source_area2_->events[0].url);
  } else {
    EXPECT_TRUE(source_area2_->events.IsEmpty());
  }
}

TEST_P(CachedStorageAreaTestWithParam, BrowserDisconnect) {
  // GetLength to prime the cache.
  mock_storage_area_.mutable_get_all_return_values().push_back(
      mojom::blink::KeyValue::New(KeyToUint8Vector(kKey),
                                  ValueToUint8Vector(kValue)));
  EXPECT_EQ(1u, cached_area_->GetLength());
  EXPECT_TRUE(IsCacheLoaded());
  mock_storage_area_.ResetObservations();

  // Now disconnect the pipe from the browser, simulating situations where the
  // browser might be forced to destroy the LevelDBWrapperImpl.
  mock_storage_area_.CloseAllBindings();

  // Getters should still function.
  EXPECT_EQ(1u, cached_area_->GetLength());
  EXPECT_EQ(kValue, cached_area_->GetItem(kKey));

  // And setters should also still function.
  cached_area_->RemoveItem(kKey, source_area_);
  EXPECT_EQ(0u, cached_area_->GetLength());
  EXPECT_TRUE(cached_area_->GetItem(kKey).IsNull());
}

TEST_F(CachedStorageAreaTest, KeyMutationsAreIgnoredUntilCompletion) {
  mojom::blink::StorageAreaObserver* observer = cached_area_.get();

  // SetItem
  EXPECT_TRUE(cached_area_->SetItem(kKey, kValue, source_area_));
  mock_storage_area_.Flush();
  EXPECT_TRUE(IsIgnoringKeyMutations(kKey));
  observer->KeyDeleted(KeyToUint8Vector(kKey), base::nullopt, kRemoteSource);
  EXPECT_TRUE(IsIgnoringKeyMutations(kKey));
  EXPECT_EQ(kValue, cached_area_->GetItem(kKey));
  observer->KeyChanged(KeyToUint8Vector(kKey), ValueToUint8Vector(kValue),
                       base::nullopt, source_);
  EXPECT_FALSE(IsIgnoringKeyMutations(kKey));

  // RemoveItem
  cached_area_->RemoveItem(kKey, source_area_);
  mock_storage_area_.Flush();
  EXPECT_TRUE(IsIgnoringKeyMutations(kKey));
  observer->KeyDeleted(KeyToUint8Vector(kKey), ValueToUint8Vector(kValue),
                       source_);
  EXPECT_FALSE(IsIgnoringKeyMutations(kKey));

  // Multiple mutations to the same key.
  EXPECT_TRUE(cached_area_->SetItem(kKey, kValue, source_area_));
  cached_area_->RemoveItem(kKey, source_area_);
  EXPECT_TRUE(IsIgnoringKeyMutations(kKey));
  mock_storage_area_.Flush();
  observer->KeyChanged(KeyToUint8Vector(kKey), ValueToUint8Vector(kValue),
                       base::nullopt, source_);
  observer->KeyDeleted(KeyToUint8Vector(kKey), ValueToUint8Vector(kValue),
                       source_);
  EXPECT_FALSE(IsIgnoringKeyMutations(kKey));

  // A failed set item operation should reset the key's cached value.
  EXPECT_TRUE(cached_area_->SetItem(kKey, kValue, source_area_));
  mock_storage_area_.Flush();
  EXPECT_TRUE(IsIgnoringKeyMutations(kKey));
  observer->KeyChangeFailed(KeyToUint8Vector(kKey), source_);
  EXPECT_TRUE(cached_area_->GetItem(kKey).IsNull());
}

TEST_F(CachedStorageAreaTest, ChangeEvents) {
  mojom::blink::StorageAreaObserver* observer = cached_area_.get();

  cached_area_->SetItem(kKey, kValue, source_area_);
  cached_area_->SetItem(kKey, kValue2, source_area_);
  cached_area_->RemoveItem(kKey, source_area_);
  observer->KeyChanged(KeyToUint8Vector(kKey), ValueToUint8Vector(kValue),
                       base::nullopt, source_);
  observer->KeyChanged(KeyToUint8Vector(kKey), ValueToUint8Vector(kValue2),
                       ValueToUint8Vector(kValue), source_);
  observer->KeyDeleted(KeyToUint8Vector(kKey), ValueToUint8Vector(kValue2),
                       source_);

  observer->KeyChanged(KeyToUint8Vector(kKey), ValueToUint8Vector(kValue),
                       base::nullopt, kRemoteSource);
  observer->AllDeleted(/*was_nonempty*/ true, kRemoteSource);

  // Source area should have ignored all but the last two events.
  ASSERT_EQ(2u, source_area_->events.size());

  EXPECT_EQ(kKey, source_area_->events[0].key);
  EXPECT_TRUE(source_area_->events[0].old_value.IsNull());
  EXPECT_EQ(kValue, source_area_->events[0].new_value);
  EXPECT_EQ(kPageUrl2, source_area_->events[0].url);

  EXPECT_TRUE(source_area_->events[1].key.IsNull());
  EXPECT_TRUE(source_area_->events[1].old_value.IsNull());
  EXPECT_TRUE(source_area_->events[1].new_value.IsNull());
  EXPECT_EQ(kPageUrl2, source_area_->events[1].url);

  // Second area should not have ignored any of the events.
  ASSERT_EQ(5u, source_area2_->events.size());

  EXPECT_EQ(kKey, source_area2_->events[0].key);
  EXPECT_TRUE(source_area2_->events[0].old_value.IsNull());
  EXPECT_EQ(kValue, source_area2_->events[0].new_value);
  EXPECT_EQ(kPageUrl, source_area2_->events[0].url);

  EXPECT_EQ(kKey, source_area2_->events[1].key);
  EXPECT_EQ(kValue, source_area2_->events[1].old_value);
  EXPECT_EQ(kValue2, source_area2_->events[1].new_value);
  EXPECT_EQ(kPageUrl, source_area2_->events[1].url);

  EXPECT_EQ(kKey, source_area2_->events[2].key);
  EXPECT_EQ(kValue2, source_area2_->events[2].old_value);
  EXPECT_TRUE(source_area2_->events[2].new_value.IsNull());
  EXPECT_EQ(kPageUrl, source_area2_->events[2].url);

  EXPECT_EQ(kKey, source_area2_->events[3].key);
  EXPECT_TRUE(source_area2_->events[3].old_value.IsNull());
  EXPECT_EQ(kValue, source_area2_->events[3].new_value);
  EXPECT_EQ(kPageUrl2, source_area2_->events[3].url);

  EXPECT_TRUE(source_area2_->events[4].key.IsNull());
  EXPECT_TRUE(source_area2_->events[4].old_value.IsNull());
  EXPECT_TRUE(source_area2_->events[4].new_value.IsNull());
  EXPECT_EQ(kPageUrl2, source_area2_->events[4].url);
}

TEST_F(CachedStorageAreaTest, RevertOnChangeFailed) {
  // Verifies that when local key changes fail, the cache is restored to an
  // appropriate state.
  mojom::blink::StorageAreaObserver* observer = cached_area_.get();
  cached_area_->SetItem(kKey, kValue, source_area_);
  EXPECT_EQ(kValue, cached_area_->GetItem(kKey));
  observer->KeyChangeFailed(KeyToUint8Vector(kKey), source_);
  EXPECT_TRUE(cached_area_->GetItem(kKey).IsNull());
}

TEST_F(CachedStorageAreaTest, RevertOnChangeFailedWithSubsequentChanges) {
  // Failure of an operation observed while another subsequent operation is
  // still queued. In this case, no revert should happen because the change that
  // would be reverted has already been overwritten.
  mojom::blink::StorageAreaObserver* observer = cached_area_.get();
  cached_area_->SetItem(kKey, kValue, source_area_);
  EXPECT_EQ(kValue, cached_area_->GetItem(kKey));
  cached_area_->SetItem(kKey, kValue2, source_area_);
  EXPECT_EQ(kValue2, cached_area_->GetItem(kKey));
  observer->KeyChangeFailed(KeyToUint8Vector(kKey), source_);
  EXPECT_EQ(kValue2, cached_area_->GetItem(kKey));
  observer->KeyChanged(KeyToUint8Vector(kKey), ValueToUint8Vector(kValue2),
                       base::nullopt, source_);
  EXPECT_EQ(kValue2, cached_area_->GetItem(kKey));
}

TEST_F(CachedStorageAreaTest, RevertOnConsecutiveChangeFailures) {
  mojom::blink::StorageAreaObserver* observer = cached_area_.get();
  // If two operations fail in a row, the cache should revert to the original
  // state before either |SetItem()|.
  cached_area_->SetItem(kKey, kValue, source_area_);
  cached_area_->SetItem(kKey, kValue2, source_area_);
  EXPECT_EQ(kValue2, cached_area_->GetItem(kKey));
  observer->KeyChangeFailed(KeyToUint8Vector(kKey), source_);
  // Still caching |kValue2| because that operation is still pending.
  EXPECT_EQ(kValue2, cached_area_->GetItem(kKey));
  observer->KeyChangeFailed(KeyToUint8Vector(kKey), source_);
  // Now that the second operation also failed, the cache should revert to the
  // value from before the first |SetItem()|, i.e. no value.
  EXPECT_TRUE(cached_area_->GetItem(kKey).IsNull());
}

TEST_F(CachedStorageAreaTest, RevertOnChangeFailedWithNonLocalChanges) {
  // If a non-local mutation is observed while a local mutation is pending
  // acknowledgement, and that local mutation ends up getting rejected, the
  // cache should revert to a state reflecting the non-local change that was
  // temporarily ignored.
  mojom::blink::StorageAreaObserver* observer = cached_area_.get();
  cached_area_->SetItem(kKey, kValue, source_area_);
  EXPECT_EQ(kValue, cached_area_->GetItem(kKey));
  // Should be ignored.
  observer->KeyChanged(KeyToUint8Vector(kKey), ValueToUint8Vector(kValue2),
                       base::nullopt, kRemoteSource);
  EXPECT_EQ(kValue, cached_area_->GetItem(kKey));
  // Now that we fail the pending |SetItem()|, the above remote change should be
  // reflected.
  observer->KeyChangeFailed(KeyToUint8Vector(kKey), source_);
  EXPECT_EQ(kValue2, cached_area_->GetItem(kKey));
}

TEST_F(CachedStorageAreaTest, RevertOnChangeFailedAfterNonLocalClear) {
  // If a non-local clear is observed while a local mutation is pending
  // acknowledgement and that local mutation ends up getting rejected, the cache
  // should revert the key to have no value, even if it had a value during the
  // corresponding |SetItem()| call.
  mojom::blink::StorageAreaObserver* observer = cached_area_.get();
  cached_area_->SetItem(kKey, kValue, source_area_);
  EXPECT_EQ(kValue, cached_area_->GetItem(kKey));
  cached_area_->SetItem(kKey, kValue2, source_area_);
  EXPECT_EQ(kValue2, cached_area_->GetItem(kKey));
  observer->KeyChanged(KeyToUint8Vector(kKey), ValueToUint8Vector(kValue),
                       base::nullopt, source_);
  // We still have |kValue2| cached since its mutation is still pending.
  EXPECT_EQ(kValue2, cached_area_->GetItem(kKey));

  // Even after a non-local clear is observed, |kValue2| remains cached because
  // pending local mutations are replayed over a non-local clear.
  observer->AllDeleted(true, kRemoteSource);
  EXPECT_EQ(kValue2, cached_area_->GetItem(kKey));

  // But if that pending mutation fails, we should "revert" to the cleared
  // value, as that's what the backend would have.
  observer->KeyChangeFailed(KeyToUint8Vector(kKey), source_);
  EXPECT_TRUE(cached_area_->GetItem(kKey).IsNull());
}

namespace {

class StringEncoding : public CachedStorageAreaTest,
                       public testing::WithParamInterface<FormatOption> {};

INSTANTIATE_TEST_SUITE_P(
    CachedStorageAreaTest,
    StringEncoding,
    ::testing::Values(FormatOption::kLocalStorageDetectFormat,
                      FormatOption::kSessionStorageForceUTF16,
                      FormatOption::kSessionStorageForceUTF8));

TEST_P(StringEncoding, RoundTrip_ASCII) {
  String key("simplekey");
  EXPECT_EQ(
      Uint8VectorToString(StringToUint8Vector(key, GetParam()), GetParam()),
      key);
}

TEST_P(StringEncoding, RoundTrip_Latin1) {
  String key("Test\xf6\xb5");
  EXPECT_TRUE(key.Is8Bit());
  EXPECT_EQ(
      Uint8VectorToString(StringToUint8Vector(key, GetParam()), GetParam()),
      key);
}

TEST_P(StringEncoding, RoundTrip_UTF16) {
  StringBuilder key;
  key.Append("key");
  key.Append(UChar(0xd83d));
  key.Append(UChar(0xde00));
  EXPECT_EQ(Uint8VectorToString(StringToUint8Vector(key.ToString(), GetParam()),
                                GetParam()),
            key);
}

TEST_P(StringEncoding, RoundTrip_InvalidUTF16) {
  StringBuilder key;
  key.Append("foo");
  key.Append(UChar(0xd83d));
  key.Append(UChar(0xde00));
  key.Append(UChar(0xdf01));
  key.Append("bar");
  if (GetParam() != FormatOption::kSessionStorageForceUTF8) {
    EXPECT_EQ(Uint8VectorToString(
                  StringToUint8Vector(key.ToString(), GetParam()), GetParam()),
              key);
  } else {
    StringBuilder validKey;
    validKey.Append("foo");
    validKey.Append(UChar(0xd83d));
    validKey.Append(UChar(0xde00));
    validKey.Append(UChar(0xfffd));
    validKey.Append("bar");
    EXPECT_EQ(Uint8VectorToString(
                  StringToUint8Vector(key.ToString(), GetParam()), GetParam()),
              validKey.ToString());
  }
}

}  // namespace

TEST_F(CachedStorageAreaTest, StringEncoding_LocalStorage) {
  String ascii_key("simplekey");
  StringBuilder non_ascii_key;
  non_ascii_key.Append("key");
  non_ascii_key.Append(UChar(0xd83d));
  non_ascii_key.Append(UChar(0xde00));
  EXPECT_EQ(
      StringToUint8Vector(ascii_key, FormatOption::kLocalStorageDetectFormat)
          .size(),
      ascii_key.length() + 1);
  EXPECT_EQ(StringToUint8Vector(non_ascii_key.ToString(),
                                FormatOption::kLocalStorageDetectFormat)
                .size(),
            non_ascii_key.length() * 2 + 1);
}

TEST_F(CachedStorageAreaTest, StringEncoding_UTF8) {
  String ascii_key("simplekey");
  StringBuilder non_ascii_key;
  non_ascii_key.Append("key");
  non_ascii_key.Append(UChar(0xd83d));
  non_ascii_key.Append(UChar(0xde00));
  EXPECT_EQ(
      StringToUint8Vector(ascii_key, FormatOption::kSessionStorageForceUTF8)
          .size(),
      ascii_key.length());
  EXPECT_EQ(StringToUint8Vector(non_ascii_key.ToString(),
                                FormatOption::kSessionStorageForceUTF8)
                .size(),
            7u);
}

TEST_F(CachedStorageAreaTest, StringEncoding_UTF16) {
  String ascii_key("simplekey");
  StringBuilder non_ascii_key;
  non_ascii_key.Append("key");
  non_ascii_key.Append(UChar(0xd83d));
  non_ascii_key.Append(UChar(0xde00));
  EXPECT_EQ(
      StringToUint8Vector(ascii_key, FormatOption::kSessionStorageForceUTF16)
          .size(),
      ascii_key.length() * 2);
  EXPECT_EQ(StringToUint8Vector(non_ascii_key.ToString(),
                                FormatOption::kSessionStorageForceUTF16)
                .size(),
            non_ascii_key.length() * 2);
}

}  // namespace blink
