// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIA_CAPABILITIES_WEB_MEDIA_DECODING_CONFIGURATION_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIA_CAPABILITIES_WEB_MEDIA_DECODING_CONFIGURATION_H_

#include "base/optional.h"
#include "third_party/blink/renderer/platform/media_capabilities/web_media_capabilities_key_system_configuration.h"
#include "third_party/blink/renderer/platform/media_capabilities/web_media_configuration.h"

namespace blink {

// Represents a MediaDecodingConfiguration dictionary to be used outside of
// Blink. The added `key_system_configuration` is optional and, if set, can be
// assumed to match the requirements set by the specification.
struct WebMediaDecodingConfiguration : public WebMediaConfiguration {
  WebMediaDecodingConfiguration() = default;

  WebMediaDecodingConfiguration(
      MediaConfigurationType type,
      base::Optional<WebAudioConfiguration> audio_configuration,
      base::Optional<WebVideoConfiguration> video_configuration,
      base::Optional<WebMediaCapabilitiesKeySystemConfiguration>
          key_system_configuration)
      : WebMediaConfiguration(type, audio_configuration, video_configuration),
        key_system_configuration(key_system_configuration) {}

  base::Optional<WebMediaCapabilitiesKeySystemConfiguration>
      key_system_configuration;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIA_CAPABILITIES_WEB_MEDIA_DECODING_CONFIGURATION_H_
