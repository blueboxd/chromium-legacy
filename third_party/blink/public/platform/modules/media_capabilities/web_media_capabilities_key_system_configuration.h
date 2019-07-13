// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_MEDIA_CAPABILITIES_WEB_MEDIA_CAPABILITIES_KEY_SYSTEM_CONFIGURATION_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_MEDIA_CAPABILITIES_WEB_MEDIA_CAPABILITIES_KEY_SYSTEM_CONFIGURATION_H_

#include "media/base/eme_constants.h"
#include "third_party/blink/public/platform/web_encrypted_media_types.h"
#include "third_party/blink/public/platform/web_media_key_system_configuration.h"
#include "third_party/blink/public/platform/web_vector.h"

namespace blink {

// Represents a MediaCapabilitiesKeySystemConfiguration dictionary to be used
// outside of Blink. When possible, the properties will be converted from string
// to Web types. The default values and behavior when invalid or missing is
// specific to each property.
struct WebMediaCapabilitiesKeySystemConfiguration {
  // This property is mandatory and should always contain a value. However, it
  // is not guaranteed that the value is valid.
  WebString key_system;

  // This is optional and will contain media::EmeInitDataType::UNKNOWN
  // if invalid or not present.
  media::EmeInitDataType init_data_type;

  // Robustness properties are optional and will contain the empty string if not
  // set. These values are not verified.
  WebString audio_robustness = "";
  WebString video_robustness = "";

  // These two properties are optional and will be set to
  // WebMediaKeySystemConfiguration::Requirement::kOptional if invalid or not
  // present.
  WebMediaKeySystemConfiguration::Requirement distinctive_identifier =
      WebMediaKeySystemConfiguration::Requirement::kOptional;
  WebMediaKeySystemConfiguration::Requirement persistent_state =
      WebMediaKeySystemConfiguration::Requirement::kOptional;

  // This is optional and will be an empty vector if not set. Each item in the
  // vector will be parsed and will be set to
  // WebEncryptedMediaSessionType::kUnknown if invalid.
  WebVector<WebEncryptedMediaSessionType> session_types;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_MEDIA_CAPABILITIES_WEB_MEDIA_CAPABILITIES_KEY_SYSTEM_CONFIGURATION_H_
