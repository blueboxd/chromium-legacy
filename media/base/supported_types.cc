// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/supported_types.h"

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "media/base/media.h"
#include "media/base/media_client.h"
#include "media/base/media_switches.h"
#include "media/media_buildflags.h"
#include "ui/display/display_switches.h"

#if BUILDFLAG(ENABLE_LIBVPX)
// TODO(dalecurtis): This technically should not be allowed in media/base. See
// TODO below about moving outside of base.
#include "third_party/libvpx/source/libvpx/vpx/vp8dx.h"
#include "third_party/libvpx/source/libvpx/vpx/vpx_codec.h"
#endif

#if defined(OS_ANDROID)
#include "base/android/build_info.h"

// TODO(dalecurtis): This include is not allowed by media/base since
// media/base/android is technically a different component. We should move
// supported_types*.{cc,h} out of media/base to fix this.
#include "media/base/android/media_codec_util.h"  // nogncheck
#endif

namespace media {

bool IsSupportedAudioType(const AudioType& type) {
  MediaClient* media_client = GetMediaClient();
  if (media_client)
    return media_client->IsSupportedAudioType(type);

  return IsDefaultSupportedAudioType(type);
}

bool IsSupportedVideoType(const VideoType& type) {
  MediaClient* media_client = GetMediaClient();
  if (media_client)
    return media_client->IsSupportedVideoType(type);

  return IsDefaultSupportedVideoType(type);
}

bool IsColorSpaceSupported(const media::VideoColorSpace& color_space) {
  switch (color_space.primaries) {
    case media::VideoColorSpace::PrimaryID::EBU_3213_E:
    case media::VideoColorSpace::PrimaryID::INVALID:
      return false;

    // Transfers supported before color management.
    case media::VideoColorSpace::PrimaryID::BT709:
    case media::VideoColorSpace::PrimaryID::UNSPECIFIED:
    case media::VideoColorSpace::PrimaryID::BT470M:
    case media::VideoColorSpace::PrimaryID::BT470BG:
    case media::VideoColorSpace::PrimaryID::SMPTE170M:
      break;

    // Supported with color management.
    case media::VideoColorSpace::PrimaryID::SMPTE240M:
    case media::VideoColorSpace::PrimaryID::FILM:
    case media::VideoColorSpace::PrimaryID::BT2020:
    case media::VideoColorSpace::PrimaryID::SMPTEST428_1:
    case media::VideoColorSpace::PrimaryID::SMPTEST431_2:
    case media::VideoColorSpace::PrimaryID::SMPTEST432_1:
      break;
  }

  switch (color_space.transfer) {
    // Transfers supported before color management.
    case media::VideoColorSpace::TransferID::UNSPECIFIED:
    case media::VideoColorSpace::TransferID::GAMMA22:
    case media::VideoColorSpace::TransferID::BT709:
    case media::VideoColorSpace::TransferID::SMPTE170M:
    case media::VideoColorSpace::TransferID::BT2020_10:
    case media::VideoColorSpace::TransferID::BT2020_12:
    case media::VideoColorSpace::TransferID::IEC61966_2_1:
      break;

    // Supported with color management.
    case media::VideoColorSpace::TransferID::GAMMA28:
    case media::VideoColorSpace::TransferID::SMPTE240M:
    case media::VideoColorSpace::TransferID::LINEAR:
    case media::VideoColorSpace::TransferID::LOG:
    case media::VideoColorSpace::TransferID::LOG_SQRT:
    case media::VideoColorSpace::TransferID::BT1361_ECG:
    case media::VideoColorSpace::TransferID::SMPTEST2084:
    case media::VideoColorSpace::TransferID::IEC61966_2_4:
    case media::VideoColorSpace::TransferID::SMPTEST428_1:
    case media::VideoColorSpace::TransferID::ARIB_STD_B67:
      break;

    // Never supported.
    case media::VideoColorSpace::TransferID::INVALID:
      return false;
  }

  switch (color_space.matrix) {
    // Supported before color management.
    case media::VideoColorSpace::MatrixID::BT709:
    case media::VideoColorSpace::MatrixID::UNSPECIFIED:
    case media::VideoColorSpace::MatrixID::BT470BG:
    case media::VideoColorSpace::MatrixID::SMPTE170M:
    case media::VideoColorSpace::MatrixID::BT2020_NCL:
      break;

    // Supported with color management.
    case media::VideoColorSpace::MatrixID::RGB:
    case media::VideoColorSpace::MatrixID::FCC:
    case media::VideoColorSpace::MatrixID::SMPTE240M:
    case media::VideoColorSpace::MatrixID::YCOCG:
    case media::VideoColorSpace::MatrixID::YDZDX:
    case media::VideoColorSpace::MatrixID::BT2020_CL:
      break;

    // Never supported.
    case media::VideoColorSpace::MatrixID::INVALID:
      return false;
  }

  if (color_space.range == gfx::ColorSpace::RangeID::INVALID)
    return false;

  return true;
}

bool IsVp9ProfileSupportedByLibvpx(VideoCodecProfile profile) {
#if BUILDFLAG(ENABLE_LIBVPX)
  // High bit depth capabilities may be toggled via LibVPX config flags.
  static const bool vpx_supports_hbd = (vpx_codec_get_caps(vpx_codec_vp9_dx()) &
                                        VPX_CODEC_CAP_HIGHBITDEPTH) != 0;
  switch (profile) {
    // LibVPX always supports Profiles 0 and 1.
    case VP9PROFILE_PROFILE0:
    case VP9PROFILE_PROFILE1:
      return true;
    case VP9PROFILE_PROFILE2:
    case VP9PROFILE_PROFILE3:
      return vpx_supports_hbd;
    default:
      NOTREACHED();
  }
#endif
  return false;
}

bool IsVp9ProfileSupported(VideoCodecProfile profile) {
  if (IsVp9ProfileSupportedByLibvpx(profile))
    return true;

#if defined(OS_ANDROID)
  // Support for VP9.2, VP9.3 was not added until Nougat and requires that we
  // have the platform decoder (MediaCodec) available.
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
          base::android::SDK_VERSION_NOUGAT ||
      !HasPlatformDecoderSupport()) {
    return false;
  }

  struct Vp9Support {
    bool has_profile2 = false;
    bool has_profile3 = false;
  };
  static const base::NoDestructor<Vp9Support> vp9_support([]() {
    std::vector<CodecProfileLevel> profiles;
    MediaCodecUtil::AddSupportedCodecProfileLevels(&profiles);

    Vp9Support vp9_support;
    for (const auto& c : profiles) {
      if (c.profile == VP9PROFILE_PROFILE2)
        vp9_support.has_profile2 = true;
      else if (c.profile == VP9PROFILE_PROFILE3)
        vp9_support.has_profile3 = true;
    }

    return vp9_support;
  }());

  if (vp9_support->has_profile2 && profile == VP9PROFILE_PROFILE2)
    return true;
  if (vp9_support->has_profile3 && profile == VP9PROFILE_PROFILE3)
    return true;
#endif

  return false;
}

bool IsAudioCodecProprietary(AudioCodec codec) {
  switch (codec) {
    case media::kCodecAAC:
    case media::kCodecAC3:
    case media::kCodecEAC3:
    case media::kCodecAMR_NB:
    case media::kCodecAMR_WB:
    case media::kCodecGSM_MS:
    case media::kCodecALAC:
    case media::kCodecMpegHAudio:
      return true;

    case media::kCodecFLAC:
    case media::kCodecMP3:
    case media::kCodecOpus:
    case media::kCodecVorbis:
    case media::kCodecPCM:
    case media::kCodecPCM_MULAW:
    case media::kCodecPCM_S16BE:
    case media::kCodecPCM_S24BE:
    case media::kCodecPCM_ALAW:
    case media::kUnknownAudioCodec:
      return false;
  }
}

bool IsDefaultSupportedAudioType(const AudioType& type) {
#if !BUILDFLAG(USE_PROPRIETARY_CODECS)
  if (IsAudioCodecProprietary(type.codec))
    return false;
#endif

  switch (type.codec) {
    case media::kCodecAAC:
    case media::kCodecFLAC:
    case media::kCodecMP3:
    case media::kCodecOpus:
    case media::kCodecPCM:
    case media::kCodecPCM_MULAW:
    case media::kCodecPCM_S16BE:
    case media::kCodecPCM_S24BE:
    case media::kCodecPCM_ALAW:
    case media::kCodecVorbis:
      return true;

    case media::kCodecAMR_NB:
    case media::kCodecAMR_WB:
    case media::kCodecGSM_MS:
#if defined(OS_CHROMEOS)
      return true;
#else
      return false;
#endif

    case media::kCodecEAC3:
    case media::kCodecALAC:
    case media::kCodecAC3:
    case media::kCodecMpegHAudio:
    case media::kUnknownAudioCodec:
      return false;
  }

  NOTREACHED();
  return false;
}

bool IsVideoCodecProprietary(VideoCodec codec) {
  switch (codec) {
    case kCodecVC1:
    case kCodecH264:
    case kCodecMPEG2:
    case kCodecMPEG4:
    case kCodecHEVC:
    case kCodecDolbyVision:
      return true;
    case kUnknownVideoCodec:
    case kCodecTheora:
    case kCodecVP8:
    case kCodecVP9:
    case kCodecAV1:
      return false;
  }
}

// TODO(chcunningham): Add platform specific logic for Android (move from
// MimeUtilIntenral).
bool IsDefaultSupportedVideoType(const VideoType& type) {
#if !BUILDFLAG(USE_PROPRIETARY_CODECS)
  if (IsVideoCodecProprietary(type.codec))
    return false;
#endif

  switch (type.codec) {
    case media::kCodecAV1:
      // If the AV1 decoder is enabled, or if we're on Q or later, yes.
#if BUILDFLAG(ENABLE_AV1_DECODER)
      return IsColorSpaceSupported(type.color_space);
#elif defined(OS_ANDROID)
      if (base::android::BuildInfo::GetInstance()->is_at_least_q() &&
          IsColorSpaceSupported(type.color_space)) {
        return true;
      }
#endif
      return false;

    case media::kCodecVP9:
      // Color management required for HDR to not look terrible.
      return IsColorSpaceSupported(type.color_space) &&
             IsVp9ProfileSupported(type.profile);
    case media::kCodecH264:
    case media::kCodecVP8:
    case media::kCodecTheora:
      return true;

    case media::kUnknownVideoCodec:
    case media::kCodecVC1:
    case media::kCodecMPEG2:
    case media::kCodecHEVC:
    case media::kCodecDolbyVision:
      return false;

    case media::kCodecMPEG4:
#if defined(OS_CHROMEOS)
      return true;
#else
      return false;
#endif
  }

  NOTREACHED();
  return false;
}

bool IsVp9ProfileSupportedForTesting(VideoCodecProfile profile) {
  return IsVp9ProfileSupported(profile);
}

}  // namespace media
