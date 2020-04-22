// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_frame_metadata.h"

#include <stdint.h>
#include <utility>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/value_conversions.h"
#include "ui/gfx/geometry/rect.h"

namespace media {

namespace {

// Map enum key to internal std::string key used by base::DictionaryValue.
inline std::string ToInternalKey(VideoFrameMetadata::Key key) {
  DCHECK_LT(key, VideoFrameMetadata::NUM_KEYS);
  return base::NumberToString(static_cast<int>(key));
}

}  // namespace

VideoFrameMetadata::VideoFrameMetadata() = default;

VideoFrameMetadata::~VideoFrameMetadata() = default;

bool VideoFrameMetadata::HasKey(Key key) const {
  return dictionary_.HasKey(ToInternalKey(key));
}

void VideoFrameMetadata::SetBoolean(Key key, bool value) {
  dictionary_.SetKey(ToInternalKey(key), base::Value(value));
}

void VideoFrameMetadata::SetInteger(Key key, int value) {
  dictionary_.SetKey(ToInternalKey(key), base::Value(value));
}

void VideoFrameMetadata::SetDouble(Key key, double value) {
  dictionary_.SetKey(ToInternalKey(key), base::Value(value));
}

void VideoFrameMetadata::SetRotation(Key key, VideoRotation value) {
  DCHECK_EQ(ROTATION, key);
  dictionary_.SetKey(ToInternalKey(key), base::Value(value));
}

void VideoFrameMetadata::SetString(Key key, const std::string& value) {
  dictionary_.SetKey(
      ToInternalKey(key),

      // Using BlobStorage since we don't want the |value| interpreted as having
      // any particular character encoding (e.g., UTF-8) by
      // base::DictionaryValue.
      base::Value(base::Value::BlobStorage(value.begin(), value.end())));
}

void VideoFrameMetadata::SetTimeDelta(Key key, const base::TimeDelta& value) {
  dictionary_.SetKey(ToInternalKey(key), base::CreateTimeDeltaValue(value));
}

void VideoFrameMetadata::SetTimeTicks(Key key, const base::TimeTicks& value) {
  // Serialize TimeTicks as TimeDeltas.
  dictionary_.SetKey(ToInternalKey(key),
                     base::CreateTimeDeltaValue(value - base::TimeTicks()));
}

void VideoFrameMetadata::SetUnguessableToken(
    Key key,
    const base::UnguessableToken& value) {
  dictionary_.SetKey(ToInternalKey(key),
                     base::CreateUnguessableTokenValue(value));
}

void VideoFrameMetadata::SetRect(Key key, const gfx::Rect& value) {
  base::Value init[] = {base::Value(value.x()), base::Value(value.y()),
                        base::Value(value.width()),
                        base::Value(value.height())};
  dictionary_.SetKey(ToInternalKey(key),
                     base::Value(base::Value::ListStorage(
                         std::make_move_iterator(std::begin(init)),
                         std::make_move_iterator(std::end(init)))));
}

bool VideoFrameMetadata::GetBoolean(Key key, bool* value) const {
  DCHECK(value);
  auto opt_bool = dictionary_.FindBoolKey(ToInternalKey(key));
  if (opt_bool)
    *value = opt_bool.value();

  return opt_bool.has_value();
}

bool VideoFrameMetadata::GetInteger(Key key, int* value) const {
  DCHECK(value);
  auto opt_int = dictionary_.FindIntKey(ToInternalKey(key));
  if (opt_int)
    *value = opt_int.value();

  return opt_int.has_value();
}

bool VideoFrameMetadata::GetDouble(Key key, double* value) const {
  DCHECK(value);
  auto opt_double = dictionary_.FindDoubleKey(ToInternalKey(key));
  if (opt_double)
    *value = opt_double.value();

  return opt_double.has_value();
}

bool VideoFrameMetadata::GetRotation(Key key, VideoRotation* value) const {
  DCHECK_EQ(ROTATION, key);
  DCHECK(value);
  auto opt_int = dictionary_.FindIntKey(ToInternalKey(key));
  if (opt_int)
    *value = static_cast<VideoRotation>(opt_int.value());
  return opt_int.has_value();
}

bool VideoFrameMetadata::GetString(Key key, std::string* value) const {
  DCHECK(value);
  const base::Value::BlobStorage* const binary_value =
      dictionary_.FindBlobKey(ToInternalKey(key));

  if (!!binary_value)
    value->assign(binary_value->begin(), binary_value->end());

  return !!binary_value;
}

bool VideoFrameMetadata::GetTimeDelta(Key key, base::TimeDelta* value) const {
  const base::Value* internal_value = dictionary_.FindKey(ToInternalKey(key));
  if (!internal_value)
    return false;
  return base::GetValueAsTimeDelta(*internal_value, value);
}

bool VideoFrameMetadata::GetTimeTicks(Key key, base::TimeTicks* value) const {
  // Deserialize TimeTicks from TimeDelta.
  const base::Value* internal_value = dictionary_.FindKey(ToInternalKey(key));
  base::TimeDelta delta;

  if (!internal_value || !base::GetValueAsTimeDelta(*internal_value, &delta))
    return false;

  *value = base::TimeTicks() + delta;
  return true;
}

bool VideoFrameMetadata::GetUnguessableToken(
    Key key,
    base::UnguessableToken* value) const {
  const base::Value* internal_value = dictionary_.FindKey(ToInternalKey(key));
  if (!internal_value)
    return false;
  return base::GetValueAsUnguessableToken(*internal_value, value);
}

bool VideoFrameMetadata::GetRect(Key key, gfx::Rect* value) const {
  const base::Value* internal_value =
      dictionary_.FindListKey(ToInternalKey(key));
  if (!internal_value || internal_value->GetList().size() != 4)
    return false;
  *value = gfx::Rect(internal_value->GetList()[0].GetInt(),
                     internal_value->GetList()[1].GetInt(),
                     internal_value->GetList()[2].GetInt(),
                     internal_value->GetList()[3].GetInt());
  return true;
}

bool VideoFrameMetadata::IsTrue(Key key) const {
  bool value = false;
  return GetBoolean(key, &value) && value;
}

void VideoFrameMetadata::MergeInternalValuesFrom(const base::Value& in) {
  // This function CHECKs if |in| is a dictionary.
  dictionary_.MergeDictionary(&in);
}

void VideoFrameMetadata::MergeMetadataFrom(
    const VideoFrameMetadata* metadata_source) {
  dictionary_.MergeDictionary(&metadata_source->dictionary_);
}

}  // namespace media
