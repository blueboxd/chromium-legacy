// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MEDIA_LOG_PROPERTIES_HELPER_H_
#define MEDIA_BASE_MEDIA_LOG_PROPERTIES_HELPER_H_

#include <string>
#include <vector>

#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/video_decoder_config.h"
#include "ui/gfx/geometry/size.h"

namespace media {

namespace internal {

// Converter struct.
template <typename T>
struct MediaLogPropertyTypeConverter {};

// Some types can be passed to the base::Value constructor.
#define _VALUE_CONSTRUCTOR_TYPE(TEMPLATE_TYPE, PARAM_TYPE) \
  template <>                                              \
  struct MediaLogPropertyTypeConverter<TEMPLATE_TYPE> {    \
    static base::Value Convert(PARAM_TYPE value) {         \
      return base::Value(value);                           \
    }                                                      \
  }

_VALUE_CONSTRUCTOR_TYPE(std::string, const std::string&);
_VALUE_CONSTRUCTOR_TYPE(bool, bool);
_VALUE_CONSTRUCTOR_TYPE(int, int);
#undef _VALUE_CONSTRUCTOR_TYPE

// Can't send non-finite double values to a base::Value.
template <>
struct MediaLogPropertyTypeConverter<double> {
  static base::Value Convert(double value) {
    return std::isfinite(value) ? base::Value(value) : base::Value("unknown");
  }
};

// Just upcast this to get the NaN check.
template <>
struct MediaLogPropertyTypeConverter<float> {
  static base::Value Convert(float value) {
    return MediaLogPropertyTypeConverter<double>::Convert(value);
  }
};

/* Support serializing for a selection of types */
// support 64 bit ints, this is a weird workaround for the base::Value
// int type only being 32 bit, as specified in the base/values.h header comment.
template <>
struct MediaLogPropertyTypeConverter<int64_t> {
  static base::Value Convert(int64_t value) {
    return base::Value(static_cast<double>(value));
  }
};

// Support gfx::Size -> "{x}x{y}"
template <>
struct MediaLogPropertyTypeConverter<gfx::Size> {
  static base::Value Convert(const gfx::Size& value) {
    return base::Value(value.ToString());
  }
};

// Support vectors of anything else thiat can be serialized
template <typename T>
struct MediaLogPropertyTypeConverter<std::vector<T>> {
  static base::Value Convert(const std::vector<T>& value) {
    base::Value result(base::Value::Type::LIST);
    for (const auto& entry : value)
      result.Append(MediaLogPropertyTypeConverter<T>::Convert(entry));
    return result;
  }
};

// Specializer for sending AudioDecoderConfigs to the media tab in devtools.
template <>
struct internal::MediaLogPropertyTypeConverter<media::AudioDecoderConfig> {
  static base::Value Convert(const AudioDecoderConfig& value) {
    base::Value result(base::Value::Type::DICTIONARY);
    result.SetStringKey("codec", GetCodecName(value.codec()));
    result.SetIntKey("bytes per channel", value.bytes_per_channel());
    result.SetStringKey("channel layout",
                        ChannelLayoutToString(value.channel_layout()));
    result.SetIntKey("channels", value.channels());
    result.SetIntKey("samples per second", value.samples_per_second());
    result.SetStringKey("sample format",
                        SampleFormatToString(value.sample_format()));
    result.SetIntKey("bytes per frame", value.bytes_per_frame());
    // TODO(tmathmeyer) drop the units, let the frontend handle it.
    // use ostringstreams because windows & linux have _different types_
    // defined for int64_t, (long vs long long) so format specifiers dont work.
    std::ostringstream preroll;
    preroll << value.seek_preroll().InMicroseconds() << "us";
    result.SetStringKey("seek preroll", preroll.str());
    result.SetIntKey("codec delay", value.codec_delay());
    result.SetBoolKey("has extra data", !value.extra_data().empty());
    std::ostringstream encryptionSchemeString;
    encryptionSchemeString << value.encryption_scheme();
    result.SetStringKey("encryption scheme", encryptionSchemeString.str());
    result.SetBoolKey("discard decoder delay",
                      value.should_discard_decoder_delay());
    return result;
  }
};

// Specializer for sending VideoDecoderConfigs to the media tab in devtools.
template <>
struct internal::MediaLogPropertyTypeConverter<VideoDecoderConfig> {
  static base::Value Convert(const VideoDecoderConfig& value) {
    base::Value result(base::Value::Type::DICTIONARY);
    result.SetStringKey("codec", GetCodecName(value.codec()));
    result.SetStringKey("profile", GetProfileName(value.profile()));
    result.SetStringKey(
        "alpha mode",
        (value.alpha_mode() == VideoDecoderConfig::AlphaMode::kHasAlpha
             ? "has_alpha"
             : "is_opaque"));
    result.SetStringKey("coded size", value.coded_size().ToString());
    result.SetStringKey("visible rect", value.visible_rect().ToString());
    result.SetStringKey("natural size", value.natural_size().ToString());
    result.SetBoolKey("has_extra_data", !value.extra_data().empty());
    std::ostringstream encryptionSchemeString;
    encryptionSchemeString << value.encryption_scheme();
    result.SetStringKey("encryption scheme", encryptionSchemeString.str());
    result.SetStringKey("rotation", VideoRotationToString(
                                        value.video_transformation().rotation));
    result.SetBoolKey("flipped", value.video_transformation().mirrored);
    result.SetStringKey("color space",
                        value.color_space_info().ToGfxColorSpace().ToString());

    if (value.hdr_metadata().has_value()) {
      result.SetKey(
          "luminance range",
          MediaLogPropertyTypeConverter<float>::Convert(
              value.hdr_metadata()->mastering_metadata.luminance_min));
      result.SetStringKey(
          "primaries",
          base::StringPrintf(
              "[r:%.4f,%.4f, g:%.4f,%.4f, b:%.4f,%.4f, wp:%.4f,%.4f]",
              value.hdr_metadata()->mastering_metadata.primary_r.x(),
              value.hdr_metadata()->mastering_metadata.primary_r.y(),
              value.hdr_metadata()->mastering_metadata.primary_g.x(),
              value.hdr_metadata()->mastering_metadata.primary_g.y(),
              value.hdr_metadata()->mastering_metadata.primary_b.x(),
              value.hdr_metadata()->mastering_metadata.primary_b.y(),
              value.hdr_metadata()->mastering_metadata.white_point.x(),
              value.hdr_metadata()->mastering_metadata.white_point.y()));
    }
    return result;
  }
};

}  // namespace internal

// Forward declare the enum.
enum class MediaLogProperty;

// Allow only specific types for an individual property.
template <MediaLogProperty PROP, typename T>
struct MediaLogPropertyTypeSupport {};

// Lets us define the supported type in a single line in media_log_properties.h.
#define MEDIA_LOG_PROPERTY_SUPPORTS_TYPE(PROPERTY, TYPE)                   \
  template <>                                                              \
  struct MediaLogPropertyTypeSupport<MediaLogProperty::PROPERTY, TYPE> {   \
    static base::Value Convert(const TYPE& type) {                         \
      return internal::MediaLogPropertyTypeConverter<TYPE>::Convert(type); \
    }                                                                      \
  }

}  // namespace media

#endif  // MEDIA_BASE_MEDIA_LOG_PROPERTIES_HELPER_H_
