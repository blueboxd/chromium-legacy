// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>
#include <string>

#include "base/json/json_writer.h"
#include "base/macros.h"
#include "media/base/media_error.h"
#include "media/base/media_serializers.h"
#include "media/base/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::HasSubstr;

namespace media {

class UselessThingToBeSerialized {
 public:
  explicit UselessThingToBeSerialized(const char* name) : name_(name) {}
  const char* name_;
};

namespace internal {

template <>
struct MediaSerializer<UselessThingToBeSerialized> {
  static base::Value Serialize(const UselessThingToBeSerialized& t) {
    return base::Value(t.name_);
  }
};

}  // namespace internal

// Friend class of MediaLog for access to internal constants.
class MediaErrorTest : public testing::Test {
 public:
  MediaError DontFail() { return MediaError::Ok(); }

  MediaError FailEasily() {
    return MEDIA_ERROR(kCodeOnlyForTesting, "Message");
  }

  MediaError FailRecursively(unsigned int count) {
    if (!count) {
      return FailEasily();
    }
    return FailRecursively(count - 1).AddHere();
  }

  template <typename T>
  MediaError FailWithData(const char* key, const T& t) {
    return MediaError(ErrorCode::kCodeOnlyForTesting, "Message", FROM_HERE)
        .WithData(key, t);
  }

  MediaError FailWithCause() {
    MediaError err = FailEasily();
    return FailEasily().AddCause(std::move(err));
  }

  MediaError DoSomethingGiveItBack(MediaError me) {
    me.WithData("data", "Hey you! psst! Help me outta here! I'm trapped!");
    return me;
  }
};

TEST_F(MediaErrorTest, StaticOKMethodGivesCorrectSerialization) {
  MediaError ok = DontFail();
  base::Value actual = MediaSerialize(ok);
  ASSERT_EQ(actual.GetString(), "Ok");
}

TEST_F(MediaErrorTest, SingleLayerError) {
  MediaError failed = FailEasily();
  base::Value actual = MediaSerialize(failed);
  ASSERT_EQ(actual.DictSize(), 5ul);
  ASSERT_EQ(actual.FindIntPath("error_code"),
            static_cast<int>(ErrorCode::kCodeOnlyForTesting));
  ASSERT_EQ(*actual.FindStringPath("error_message"), "Message");
  ASSERT_EQ(actual.FindListPath("stack")->GetList().size(), 1ul);
  ASSERT_EQ(actual.FindListPath("causes")->GetList().size(), 0ul);
  ASSERT_EQ(actual.FindDictPath("data")->DictSize(), 0ul);

  const auto& stack = actual.FindListPath("stack")->GetList();
  ASSERT_EQ(stack[0].DictSize(), 2ul);  // line and file

  // This is a bit fragile, since it's dependent on the file layout.
  ASSERT_EQ(stack[0].FindIntPath("line").value_or(-1), 42);
  ASSERT_THAT(*stack[0].FindStringPath("file"),
              HasSubstr("media_error_unittest.cc"));
}

TEST_F(MediaErrorTest, MultipleErrorLayer) {
  MediaError failed = FailRecursively(3);
  base::Value actual = MediaSerialize(failed);
  ASSERT_EQ(actual.DictSize(), 5ul);
  ASSERT_EQ(actual.FindIntPath("error_code"),
            static_cast<int>(ErrorCode::kCodeOnlyForTesting));
  ASSERT_EQ(*actual.FindStringPath("error_message"), "Message");
  ASSERT_EQ(actual.FindListPath("stack")->GetList().size(), 4ul);
  ASSERT_EQ(actual.FindListPath("causes")->GetList().size(), 0ul);
  ASSERT_EQ(actual.FindDictPath("data")->DictSize(), 0ul);

  const auto& stack = actual.FindListPath("stack")->GetList();
  ASSERT_EQ(stack[0].DictSize(), 2ul);  // line and file
}

TEST_F(MediaErrorTest, CanHaveData) {
  MediaError failed = FailWithData("example", "data");
  base::Value actual = MediaSerialize(failed);
  ASSERT_EQ(actual.DictSize(), 5ul);
  ASSERT_EQ(actual.FindIntPath("error_code"),
            static_cast<int>(ErrorCode::kCodeOnlyForTesting));
  ASSERT_EQ(*actual.FindStringPath("error_message"), "Message");
  ASSERT_EQ(actual.FindListPath("stack")->GetList().size(), 1ul);
  ASSERT_EQ(actual.FindListPath("causes")->GetList().size(), 0ul);
  ASSERT_EQ(actual.FindDictPath("data")->DictSize(), 1ul);

  const auto& stack = actual.FindListPath("stack")->GetList();
  ASSERT_EQ(stack[0].DictSize(), 2ul);  // line and file

  ASSERT_EQ(*actual.FindDictPath("data")->FindStringPath("example"), "data");
}

TEST_F(MediaErrorTest, CanUseCustomSerializer) {
  MediaError failed = FailWithData("example", UselessThingToBeSerialized("F"));
  base::Value actual = MediaSerialize(failed);
  ASSERT_EQ(actual.DictSize(), 5ul);
  ASSERT_EQ(actual.FindIntPath("error_code"),
            static_cast<int>(ErrorCode::kCodeOnlyForTesting));
  ASSERT_EQ(*actual.FindStringPath("error_message"), "Message");
  ASSERT_EQ(actual.FindListPath("stack")->GetList().size(), 1ul);
  ASSERT_EQ(actual.FindListPath("causes")->GetList().size(), 0ul);
  ASSERT_EQ(actual.FindDictPath("data")->DictSize(), 1ul);

  const auto& stack = actual.FindListPath("stack")->GetList();
  ASSERT_EQ(stack[0].DictSize(), 2ul);  // line and file

  ASSERT_EQ(*actual.FindDictPath("data")->FindStringPath("example"), "F");
}

TEST_F(MediaErrorTest, CausedByHasVector) {
  MediaError causal = FailWithCause();
  base::Value actual = MediaSerialize(causal);
  ASSERT_EQ(actual.DictSize(), 5ul);
  ASSERT_EQ(actual.FindIntPath("error_code"),
            static_cast<int>(ErrorCode::kCodeOnlyForTesting));
  ASSERT_EQ(*actual.FindStringPath("error_message"), "Message");
  ASSERT_EQ(actual.FindListPath("stack")->GetList().size(), 1ul);
  ASSERT_EQ(actual.FindListPath("causes")->GetList().size(), 1ul);
  ASSERT_EQ(actual.FindDictPath("data")->DictSize(), 0ul);

  base::Value& nested = actual.FindListPath("causes")->GetList()[0];
  ASSERT_EQ(nested.DictSize(), 5ul);
  ASSERT_EQ(nested.FindIntPath("error_code"),
            static_cast<int>(ErrorCode::kCodeOnlyForTesting));
  ASSERT_EQ(*nested.FindStringPath("error_message"), "Message");
  ASSERT_EQ(nested.FindListPath("stack")->GetList().size(), 1ul);
  ASSERT_EQ(nested.FindListPath("causes")->GetList().size(), 0ul);
  ASSERT_EQ(nested.FindDictPath("data")->DictSize(), 0ul);
}

TEST_F(MediaErrorTest, CanCopyEasily) {
  MediaError failed = FailEasily();
  MediaError withData = DoSomethingGiveItBack(failed);

  base::Value actual = MediaSerialize(failed);
  ASSERT_EQ(actual.DictSize(), 5ul);
  ASSERT_EQ(actual.FindIntPath("error_code"),
            static_cast<int>(ErrorCode::kCodeOnlyForTesting));
  ASSERT_EQ(*actual.FindStringPath("error_message"), "Message");
  ASSERT_EQ(actual.FindListPath("stack")->GetList().size(), 1ul);
  ASSERT_EQ(actual.FindListPath("causes")->GetList().size(), 0ul);
  ASSERT_EQ(actual.FindDictPath("data")->DictSize(), 0ul);

  actual = MediaSerialize(withData);
  ASSERT_EQ(actual.DictSize(), 5ul);
  ASSERT_EQ(actual.FindIntPath("error_code"),
            static_cast<int>(ErrorCode::kCodeOnlyForTesting));
  ASSERT_EQ(*actual.FindStringPath("error_message"), "Message");
  ASSERT_EQ(actual.FindListPath("stack")->GetList().size(), 1ul);
  ASSERT_EQ(actual.FindListPath("causes")->GetList().size(), 0ul);
  ASSERT_EQ(actual.FindDictPath("data")->DictSize(), 1ul);
}

}  // namespace media
