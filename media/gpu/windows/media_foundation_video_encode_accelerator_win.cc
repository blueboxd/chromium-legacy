// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/media_foundation_video_encode_accelerator_win.h"

#pragma warning(push)
#pragma warning(disable : 4800)  // Disable warning for added padding.

#include <codecapi.h>
#include <mferror.h>
#include <mftransform.h>
#include <objbase.h>

#include <iterator>
#include <utility>
#include <vector>

#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/scoped_variant.h"
#include "base/win/windows_version.h"
#include "media/base/win/mf_helpers.h"
#include "media/base/win/mf_initializer.h"
#include "third_party/libyuv/include/libyuv.h"

using media::MediaBufferScopedPointer;

namespace media {

namespace {

const int32_t kDefaultTargetBitrate = 5000000;
const size_t kMaxFrameRateNumerator = 30;
const size_t kMaxFrameRateDenominator = 1;
const size_t kMaxResolutionWidth = 1920;
const size_t kMaxResolutionHeight = 1088;
const size_t kNumInputBuffers = 3;
// Media Foundation uses 100 nanosecond units for time, see
// https://msdn.microsoft.com/en-us/library/windows/desktop/ms697282(v=vs.85).aspx.
const size_t kOneMicrosecondInMFSampleTimeUnits = 10;

constexpr const wchar_t* const kMediaFoundationVideoEncoderDLLs[] = {
    L"mf.dll", L"mfplat.dll",
};

// Resolutions that some platforms support, should be listed in ascending order.
constexpr const gfx::Size kOptionalMaxResolutions[] = {gfx::Size(3840, 2176)};

eAVEncH264VProfile GetH264VProfile(VideoCodecProfile profile) {
  switch (profile) {
    case H264PROFILE_BASELINE:
      return eAVEncH264VProfile_Base;
    case H264PROFILE_MAIN:
      return eAVEncH264VProfile_Main;
    case H264PROFILE_HIGH: {
      // eAVEncH264VProfile_High requires Windows 8.
      if (base::win::GetVersion() < base::win::Version::WIN8) {
        return eAVEncH264VProfile_unknown;
      }
      return eAVEncH264VProfile_High;
    }
    default:
      return eAVEncH264VProfile_unknown;
  }
}

}  // namespace

class MediaFoundationVideoEncodeAccelerator::EncodeOutput {
 public:
  EncodeOutput(uint32_t size, bool key_frame, base::TimeDelta timestamp)
      : keyframe(key_frame), capture_timestamp(timestamp), data_(size) {}

  uint8_t* memory() { return data_.data(); }

  int size() const { return static_cast<int>(data_.size()); }

  const bool keyframe;
  const base::TimeDelta capture_timestamp;

 private:
  std::vector<uint8_t> data_;

  DISALLOW_COPY_AND_ASSIGN(EncodeOutput);
};

struct MediaFoundationVideoEncodeAccelerator::BitstreamBufferRef {
  BitstreamBufferRef(int32_t id,
                     base::WritableSharedMemoryMapping mapping,
                     size_t size)
      : id(id), mapping(std::move(mapping)), size(size) {}
  const int32_t id;
  const base::WritableSharedMemoryMapping mapping;
  const size_t size;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(BitstreamBufferRef);
};

// TODO(zijiehe): Respect |compatible_with_win7_| in the implementation. Some
// attributes are not supported by Windows 7, setting them will return errors.
// See bug: http://crbug.com/777659.
MediaFoundationVideoEncodeAccelerator::MediaFoundationVideoEncodeAccelerator(
    bool compatible_with_win7)
    : compatible_with_win7_(compatible_with_win7),
      input_required_(false),
      main_client_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      encoder_thread_("MFEncoderThread"),
      encoder_task_weak_factory_(this) {}

MediaFoundationVideoEncodeAccelerator::
    ~MediaFoundationVideoEncodeAccelerator() {
  DVLOG(3) << __func__;
  DCHECK(main_client_task_runner_->BelongsToCurrentThread());

  DCHECK(!encoder_thread_.IsRunning());
  DCHECK(!encoder_task_weak_factory_.HasWeakPtrs());
}

VideoEncodeAccelerator::SupportedProfiles
MediaFoundationVideoEncodeAccelerator::GetSupportedProfiles() {
  TRACE_EVENT0("gpu,startup",
               "MediaFoundationVideoEncodeAccelerator::GetSupportedProfiles");
  DVLOG(3) << __func__;
  DCHECK(main_client_task_runner_->BelongsToCurrentThread());

  SupportedProfiles profiles;
  target_bitrate_ = kDefaultTargetBitrate;
  frame_rate_ = kMaxFrameRateNumerator / kMaxFrameRateDenominator;
  input_visible_size_ = gfx::Size(kMaxResolutionWidth, kMaxResolutionHeight);
  if (!CreateHardwareEncoderMFT() || !SetEncoderModes() ||
      !InitializeInputOutputParameters(H264PROFILE_BASELINE)) {
    ReleaseEncoderResources();
    DVLOG(1)
        << "Hardware encode acceleration is not available on this platform.";

    return profiles;
  }

  gfx::Size highest_supported_resolution = input_visible_size_;
  for (const auto& resolution : kOptionalMaxResolutions) {
    DCHECK_GT(resolution.GetArea(), highest_supported_resolution.GetArea());
    if (!IsResolutionSupported(resolution)) {
      break;
    }
    highest_supported_resolution = resolution;
  }
  ReleaseEncoderResources();

  SupportedProfile profile;
  // More profiles can be supported here, but they should be available in SW
  // fallback as well.
  profile.profile = H264PROFILE_BASELINE;
  profile.max_framerate_numerator = kMaxFrameRateNumerator;
  profile.max_framerate_denominator = kMaxFrameRateDenominator;
  profile.max_resolution = highest_supported_resolution;
  profiles.push_back(profile);

  profile.profile = H264PROFILE_MAIN;
  profiles.push_back(profile);

  profile.profile = H264PROFILE_HIGH;
  profiles.push_back(profile);

  return profiles;
}

bool MediaFoundationVideoEncodeAccelerator::Initialize(const Config& config,
                                                       Client* client) {
  DVLOG(3) << __func__ << ": " << config.AsHumanReadableString();
  DCHECK(main_client_task_runner_->BelongsToCurrentThread());

  if (PIXEL_FORMAT_I420 != config.input_format) {
    DLOG(ERROR) << "Input format not supported= "
                << VideoPixelFormatToString(config.input_format);
    return false;
  }

  if (GetH264VProfile(config.output_profile) == eAVEncH264VProfile_unknown) {
    DLOG(ERROR) << "Output profile not supported= " << config.output_profile;
    return false;
  }

  encoder_thread_.init_com_with_mta(false);
  if (!encoder_thread_.Start()) {
    DLOG(ERROR) << "Failed spawning encoder thread.";
    return false;
  }
  encoder_thread_task_runner_ = encoder_thread_.task_runner();

  if (!CreateHardwareEncoderMFT()) {
    DLOG(ERROR) << "Failed creating a hardware encoder MFT.";
    return false;
  }

  main_client_weak_factory_.reset(new base::WeakPtrFactory<Client>(client));
  main_client_ = main_client_weak_factory_->GetWeakPtr();
  input_visible_size_ = config.input_visible_size;
  frame_rate_ = kMaxFrameRateNumerator / kMaxFrameRateDenominator;
  target_bitrate_ = config.initial_bitrate;
  bitstream_buffer_size_ = config.input_visible_size.GetArea();

  if (!SetEncoderModes()) {
    DLOG(ERROR) << "Failed setting encoder parameters.";
    return false;
  }

  if (!InitializeInputOutputParameters(config.output_profile)) {
    DLOG(ERROR) << "Failed initializing input-output samples.";
    return false;
  }

  MFT_INPUT_STREAM_INFO input_stream_info;
  HRESULT hr =
      encoder_->GetInputStreamInfo(input_stream_id_, &input_stream_info);
  RETURN_ON_HR_FAILURE(hr, "Couldn't get input stream info", false);
  input_sample_ = CreateEmptySampleWithBuffer(
      input_stream_info.cbSize
          ? input_stream_info.cbSize
          : VideoFrame::AllocationSize(PIXEL_FORMAT_NV12, input_visible_size_),
      input_stream_info.cbAlignment);

  main_client_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Client::RequireBitstreamBuffers, main_client_,
                                kNumInputBuffers, input_visible_size_,
                                bitstream_buffer_size_));

  hr = encoder_->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0);
  RETURN_ON_HR_FAILURE(
      hr, "Couldn't set ProcessMessage MFT_MESSAGE_COMMAND_FLUSH", false);
  hr = encoder_->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
  RETURN_ON_HR_FAILURE(
      hr, "Couldn't set ProcessMessage MFT_MESSAGE_NOTIFY_BEGIN_STREAMING",
      false);
  hr = encoder_->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);
  RETURN_ON_HR_FAILURE(
      hr, "Couldn't set ProcessMessage MFT_MESSAGE_NOTIFY_START_OF_STREAM",
      false);
  hr = encoder_->QueryInterface(IID_PPV_ARGS(&event_generator_));
  RETURN_ON_HR_FAILURE(hr, "Couldn't get event generator", false);

  return SUCCEEDED(hr);
}

void MediaFoundationVideoEncodeAccelerator::Encode(
    scoped_refptr<VideoFrame> frame,
    bool force_keyframe) {
  DVLOG(3) << __func__;
  DCHECK(main_client_task_runner_->BelongsToCurrentThread());

  encoder_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&MediaFoundationVideoEncodeAccelerator::EncodeTask,
                     encoder_task_weak_factory_.GetWeakPtr(), std::move(frame),
                     force_keyframe));
}

void MediaFoundationVideoEncodeAccelerator::UseOutputBitstreamBuffer(
    BitstreamBuffer buffer) {
  DVLOG(3) << __func__ << ": buffer size=" << buffer.size();
  DCHECK(main_client_task_runner_->BelongsToCurrentThread());

  if (buffer.size() < bitstream_buffer_size_) {
    DLOG(ERROR) << "Output BitstreamBuffer isn't big enough: " << buffer.size()
                << " vs. " << bitstream_buffer_size_;
    NotifyError(kInvalidArgumentError);
    return;
  }

  auto region =
      base::UnsafeSharedMemoryRegion::Deserialize(buffer.TakeRegion());
  auto mapping = region.Map();
  if (!region.IsValid() || !mapping.IsValid()) {
    DLOG(ERROR) << "Failed mapping shared memory.";
    NotifyError(kPlatformFailureError);
    return;
  }
  // After mapping, |region| is no longer necessary and it can be
  // destroyed. |mapping| will keep the shared memory region open.

  std::unique_ptr<BitstreamBufferRef> buffer_ref(
      new BitstreamBufferRef(buffer.id(), std::move(mapping), buffer.size()));
  encoder_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &MediaFoundationVideoEncodeAccelerator::UseOutputBitstreamBufferTask,
          encoder_task_weak_factory_.GetWeakPtr(), base::Passed(&buffer_ref)));
}

void MediaFoundationVideoEncodeAccelerator::RequestEncodingParametersChange(
    uint32_t bitrate,
    uint32_t framerate) {
  DVLOG(3) << __func__ << ": bitrate=" << bitrate
           << ": framerate=" << framerate;
  DCHECK(main_client_task_runner_->BelongsToCurrentThread());

  encoder_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&MediaFoundationVideoEncodeAccelerator::
                                    RequestEncodingParametersChangeTask,
                                encoder_task_weak_factory_.GetWeakPtr(),
                                bitrate, framerate));
}

void MediaFoundationVideoEncodeAccelerator::Destroy() {
  DVLOG(3) << __func__;
  DCHECK(main_client_task_runner_->BelongsToCurrentThread());

  // Cancel all callbacks.
  main_client_weak_factory_.reset();

  if (encoder_thread_.IsRunning()) {
    encoder_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MediaFoundationVideoEncodeAccelerator::DestroyTask,
                       encoder_task_weak_factory_.GetWeakPtr()));
    encoder_thread_.Stop();
  }

  delete this;
}

// static
bool MediaFoundationVideoEncodeAccelerator::PreSandboxInitialization() {
  bool result = true;
  for (const wchar_t* mfdll : kMediaFoundationVideoEncoderDLLs) {
    if (::LoadLibrary(mfdll) == nullptr) {
      result = false;
    }
  }
  return result;
}

bool MediaFoundationVideoEncodeAccelerator::CreateHardwareEncoderMFT() {
  DVLOG(3) << __func__;
  DCHECK(main_client_task_runner_->BelongsToCurrentThread());

  if (!compatible_with_win7_ &&
      base::win::GetVersion() < base::win::Version::WIN8) {
    DVLOG(ERROR) << "Windows versions earlier than 8 are not supported.";
    return false;
  }

  for (const wchar_t* mfdll : kMediaFoundationVideoEncoderDLLs) {
    if (!::GetModuleHandle(mfdll)) {
      DVLOG(ERROR) << mfdll << " is required for encoding";
      return false;
    }
  }

  if (!(session_ = InitializeMediaFoundation())) {
    return false;
  }

  uint32_t flags = MFT_ENUM_FLAG_HARDWARE | MFT_ENUM_FLAG_SORTANDFILTER;
  MFT_REGISTER_TYPE_INFO input_info;
  input_info.guidMajorType = MFMediaType_Video;
  input_info.guidSubtype = MFVideoFormat_NV12;
  MFT_REGISTER_TYPE_INFO output_info;
  output_info.guidMajorType = MFMediaType_Video;
  output_info.guidSubtype = MFVideoFormat_H264;

  uint32_t count = 0;
  base::win::ScopedCoMem<IMFActivate*> pp_activate;
  HRESULT hr = MFTEnumEx(MFT_CATEGORY_VIDEO_ENCODER, flags, &input_info,
                         &output_info, &pp_activate, &count);
  RETURN_ON_HR_FAILURE(hr, "Couldn't enumerate hardware encoder", false);
  RETURN_ON_FAILURE((count > 0), "No hardware encoder found", false);
  DVLOG(3) << "Hardware encoder(s) found: " << count;

  // Try to create the encoder with priority according to merit value.
  hr = E_FAIL;
  for (UINT32 i = 0; i < count; i++) {
    if (FAILED(hr)) {
      DCHECK(!encoder_);
      DCHECK(!activate_);
      hr = pp_activate[i]->ActivateObject(IID_PPV_ARGS(&encoder_));
      if (encoder_.Get() != nullptr) {
        DCHECK(SUCCEEDED(hr));

        activate_ = pp_activate[i];
        pp_activate[i] = nullptr;

        // Print the friendly name.
        base::win::ScopedCoMem<WCHAR> friendly_name;
        UINT32 name_length;
        activate_->GetAllocatedString(MFT_FRIENDLY_NAME_Attribute,
                                      &friendly_name, &name_length);
        DVLOG(3) << "Selected hardware encoder's friendly name: "
                 << friendly_name;
      } else {
        DCHECK(FAILED(hr));

        // The component that calls ActivateObject is
        // responsible for calling ShutdownObject,
        // https://docs.microsoft.com/en-us/windows/win32/api/mfobjects/nf-mfobjects-imfactivate-shutdownobject.
        pp_activate[i]->ShutdownObject();
      }
    }

    // Release the enumerated instances. According to Windows Dev Center,
    // https://docs.microsoft.com/en-us/windows/win32/api/mfapi/nf-mfapi-mftenumex
    // The caller must release the pointers.
    if (pp_activate[i]) {
      pp_activate[i]->Release();
      pp_activate[i] = nullptr;
    }
  }

  RETURN_ON_HR_FAILURE(hr, "Couldn't activate hardware encoder", false);
  RETURN_ON_FAILURE((encoder_.Get() != nullptr),
                    "No hardware encoder instance created", false);

  Microsoft::WRL::ComPtr<IMFAttributes> all_attributes;
  hr = encoder_->GetAttributes(&all_attributes);
  if (SUCCEEDED(hr)) {
    // An asynchronous MFT must support dynamic format changes,
    // https://docs.microsoft.com/en-us/windows/win32/medfound/asynchronous-mfts#format-changes.
    UINT32 dynamic = FALSE;
    hr = all_attributes->GetUINT32(MFT_SUPPORT_DYNAMIC_FORMAT_CHANGE, &dynamic);
    if (!dynamic) {
      DLOG(ERROR) << "Couldn't support dynamic format change.";
      return false;
    }

    // Unlock the selected asynchronous MFTs,
    // https://docs.microsoft.com/en-us/windows/win32/medfound/asynchronous-mfts#unlocking-asynchronous-mfts.
    UINT32 async = FALSE;
    hr = all_attributes->GetUINT32(MF_TRANSFORM_ASYNC, &async);
    if (!async) {
      DLOG(ERROR) << "MFT encoder is not asynchronous.";
      return false;
    }

    hr = all_attributes->SetUINT32(MF_TRANSFORM_ASYNC_UNLOCK, TRUE);
    RETURN_ON_HR_FAILURE(hr, "Couldn't unlock transform async", false);
  }

  return true;
}

bool MediaFoundationVideoEncodeAccelerator::InitializeInputOutputParameters(
    VideoCodecProfile output_profile) {
  DCHECK(main_client_task_runner_->BelongsToCurrentThread());
  DCHECK(encoder_);

  DWORD input_count = 0;
  DWORD output_count = 0;
  HRESULT hr = encoder_->GetStreamCount(&input_count, &output_count);
  RETURN_ON_HR_FAILURE(hr, "Couldn't get stream count", false);
  if (input_count < 1 || output_count < 1) {
    DLOG(ERROR) << "Stream count too few: input " << input_count << ", output "
                << output_count;
    return false;
  }

  std::vector<DWORD> input_ids(input_count, 0);
  std::vector<DWORD> output_ids(output_count, 0);
  hr = encoder_->GetStreamIDs(input_count, input_ids.data(), output_count,
                              output_ids.data());
  if (hr == S_OK) {
    input_stream_id_ = input_ids[0];
    output_stream_id_ = output_ids[0];
  } else if (hr == E_NOTIMPL) {
    input_stream_id_ = 0;
    output_stream_id_ = 0;
  } else {
    DLOG(ERROR) << "Couldn't find stream ids from hardware encoder.";
    return false;
  }

  // Initialize output parameters.
  hr = MFCreateMediaType(&imf_output_media_type_);
  RETURN_ON_HR_FAILURE(hr, "Couldn't create output media type", false);
  hr = imf_output_media_type_->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set media type", false);
  hr = imf_output_media_type_->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set video format", false);
  hr = imf_output_media_type_->SetUINT32(MF_MT_AVG_BITRATE, target_bitrate_);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set bitrate", false);
  hr = MFSetAttributeRatio(imf_output_media_type_.Get(), MF_MT_FRAME_RATE,
                           frame_rate_, 1);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set frame rate", false);
  hr = MFSetAttributeSize(imf_output_media_type_.Get(), MF_MT_FRAME_SIZE,
                          input_visible_size_.width(),
                          input_visible_size_.height());
  RETURN_ON_HR_FAILURE(hr, "Couldn't set frame size", false);
  hr = imf_output_media_type_->SetUINT32(MF_MT_INTERLACE_MODE,
                                         MFVideoInterlace_Progressive);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set interlace mode", false);
  hr = imf_output_media_type_->SetUINT32(MF_MT_MPEG2_PROFILE,
                                         GetH264VProfile(output_profile));
  RETURN_ON_HR_FAILURE(hr, "Couldn't set codec profile", false);
  hr = encoder_->SetOutputType(output_stream_id_, imf_output_media_type_.Get(),
                               0);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set output media type", false);

  // Initialize input parameters.
  hr = MFCreateMediaType(&imf_input_media_type_);
  RETURN_ON_HR_FAILURE(hr, "Couldn't create input media type", false);
  hr = imf_input_media_type_->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set media type", false);
  hr = imf_input_media_type_->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set video format", false);
  hr = MFSetAttributeRatio(imf_input_media_type_.Get(), MF_MT_FRAME_RATE,
                           frame_rate_, 1);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set frame rate", false);
  hr = MFSetAttributeSize(imf_input_media_type_.Get(), MF_MT_FRAME_SIZE,
                          input_visible_size_.width(),
                          input_visible_size_.height());
  RETURN_ON_HR_FAILURE(hr, "Couldn't set frame size", false);
  hr = imf_input_media_type_->SetUINT32(MF_MT_INTERLACE_MODE,
                                        MFVideoInterlace_Progressive);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set interlace mode", false);
  hr = encoder_->SetInputType(input_stream_id_, imf_input_media_type_.Get(), 0);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set input media type", false);

  return true;
}

bool MediaFoundationVideoEncodeAccelerator::SetEncoderModes() {
  DCHECK(main_client_task_runner_->BelongsToCurrentThread());
  DCHECK(encoder_);

  HRESULT hr = encoder_.As(&codec_api_);
  RETURN_ON_HR_FAILURE(hr, "Couldn't get ICodecAPI", false);

  VARIANT var;
  var.vt = VT_UI4;
  var.ulVal = eAVEncCommonRateControlMode_CBR;
  hr = codec_api_->SetValue(&CODECAPI_AVEncCommonRateControlMode, &var);
  if (!compatible_with_win7_) {
    // Though CODECAPI_AVEncCommonRateControlMode is supported by Windows 7, but
    // according to a discussion on MSDN,
    // https://social.msdn.microsoft.com/Forums/windowsdesktop/en-US/6da521e9-7bb3-4b79-a2b6-b31509224638/win7-h264-encoder-imfsinkwriter-cant-use-quality-vbr-encoding?forum=mediafoundationdevelopment
    // setting it on Windows 7 returns error.
    RETURN_ON_HR_FAILURE(hr, "Couldn't set CommonRateControlMode", false);
  }

  if (S_OK ==
      codec_api_->IsModifiable(&CODECAPI_AVEncVideoTemporalLayerCount)) {
    var.ulVal = 1;
    hr = codec_api_->SetValue(&CODECAPI_AVEncVideoTemporalLayerCount, &var);
    if (!compatible_with_win7_) {
      RETURN_ON_HR_FAILURE(hr, "Couldn't set temporal layer count", false);
    }
  }

  var.ulVal = target_bitrate_;
  hr = codec_api_->SetValue(&CODECAPI_AVEncCommonMeanBitRate, &var);
  if (!compatible_with_win7_) {
    RETURN_ON_HR_FAILURE(hr, "Couldn't set bitrate", false);
  }

  if (S_OK == codec_api_->IsModifiable(&CODECAPI_AVEncAdaptiveMode)) {
    var.ulVal = eAVEncAdaptiveMode_Resolution;
    hr = codec_api_->SetValue(&CODECAPI_AVEncAdaptiveMode, &var);
    if (!compatible_with_win7_) {
      RETURN_ON_HR_FAILURE(hr, "Couldn't set adaptive mode", false);
    }
  }

  if (S_OK == codec_api_->IsModifiable(&CODECAPI_AVLowLatencyMode)) {
    var.vt = VT_BOOL;
    var.boolVal = VARIANT_TRUE;
    hr = codec_api_->SetValue(&CODECAPI_AVLowLatencyMode, &var);
    if (!compatible_with_win7_) {
      RETURN_ON_HR_FAILURE(hr, "Couldn't set low latency mode", false);
    }
  }

  return true;
}

bool MediaFoundationVideoEncodeAccelerator::IsResolutionSupported(
    const gfx::Size& resolution) {
  DCHECK(main_client_task_runner_->BelongsToCurrentThread());
  DCHECK(encoder_);

  HRESULT hr =
      MFSetAttributeSize(imf_output_media_type_.Get(), MF_MT_FRAME_SIZE,
                         resolution.width(), resolution.height());
  RETURN_ON_HR_FAILURE(hr, "Couldn't set frame size", false);
  hr = encoder_->SetOutputType(output_stream_id_, imf_output_media_type_.Get(),
                               0);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set output media type", false);

  hr = MFSetAttributeSize(imf_input_media_type_.Get(), MF_MT_FRAME_SIZE,
                          resolution.width(), resolution.height());
  RETURN_ON_HR_FAILURE(hr, "Couldn't set frame size", false);
  hr = encoder_->SetInputType(input_stream_id_, imf_input_media_type_.Get(), 0);
  RETURN_ON_HR_FAILURE(hr, "Couldn't set input media type", false);

  return true;
}

void MediaFoundationVideoEncodeAccelerator::NotifyError(
    VideoEncodeAccelerator::Error error) {
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread() ||
         main_client_task_runner_->BelongsToCurrentThread());

  main_client_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Client::NotifyError, main_client_, error));
}

void MediaFoundationVideoEncodeAccelerator::EncodeTask(
    scoped_refptr<VideoFrame> frame,
    bool force_keyframe) {
  DVLOG(3) << __func__;
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());

  bool input_delivered = false;
  if (input_required_) {
    // HMFT is waiting for this coming input.
    ProcessInput(frame, force_keyframe);
    input_delivered = true;
    input_required_ = false;
  } else {
    Microsoft::WRL::ComPtr<IMFMediaEvent> media_event;
    HRESULT hr =
        event_generator_->GetEvent(MF_EVENT_FLAG_NO_WAIT, &media_event);
    if (FAILED(hr)) {
      DLOG(WARNING) << "Abandoned input frame for video encoder.";
      return;
    }

    MediaEventType event_type;
    hr = media_event->GetType(&event_type);
    if (FAILED(hr)) {
      DLOG(ERROR) << "Failed to get the type of media event.";
      return;
    }

    // Always deliver the current input into HMFT.
    if (event_type == METransformNeedInput) {
      ProcessInput(frame, force_keyframe);
      input_delivered = true;
    } else if (event_type == METransformHaveOutput) {
      ProcessOutput();
      input_delivered =
          TryToDeliverInputFrame(std::move(frame), force_keyframe);
    }
  }

  if (!input_delivered) {
    DLOG(ERROR) << "Failed to deliver input frame to video encoder";
    return;
  }

  TryToReturnBitstreamBuffer();
}

void MediaFoundationVideoEncodeAccelerator::ProcessInput(
    scoped_refptr<VideoFrame> frame,
    bool force_keyframe) {
  DVLOG(3) << __func__;
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());

  // Convert I420 to NV12 as input.
  Microsoft::WRL::ComPtr<IMFMediaBuffer> input_buffer;
  input_sample_->GetBufferByIndex(0, &input_buffer);

  {
    MediaBufferScopedPointer scoped_buffer(input_buffer.Get());
    DCHECK(scoped_buffer.get());
    int dst_stride_y = frame->stride(VideoFrame::kYPlane);
    uint8_t* dst_uv =
        scoped_buffer.get() +
        frame->stride(VideoFrame::kYPlane) * frame->rows(VideoFrame::kYPlane);
    int dst_stride_uv = frame->stride(VideoFrame::kUPlane) * 2;
    libyuv::I420ToNV12(frame->visible_data(VideoFrame::kYPlane),
                       frame->stride(VideoFrame::kYPlane),
                       frame->visible_data(VideoFrame::kUPlane),
                       frame->stride(VideoFrame::kUPlane),
                       frame->visible_data(VideoFrame::kVPlane),
                       frame->stride(VideoFrame::kVPlane), scoped_buffer.get(),
                       dst_stride_y, dst_uv, dst_stride_uv,
                       input_visible_size_.width(),
                       input_visible_size_.height());
  }

  input_sample_->SetSampleTime(frame->timestamp().InMicroseconds() *
                               kOneMicrosecondInMFSampleTimeUnits);
  UINT64 sample_duration = 0;
  HRESULT hr =
      MFFrameRateToAverageTimePerFrame(frame_rate_, 1, &sample_duration);
  RETURN_ON_HR_FAILURE(hr, "Couldn't calculate sample duration", );
  input_sample_->SetSampleDuration(sample_duration);

  // Release frame after input is copied.
  frame = nullptr;

  if (force_keyframe) {
    VARIANT var;
    var.vt = VT_UI4;
    var.ulVal = 1;
    hr = codec_api_->SetValue(&CODECAPI_AVEncVideoForceKeyFrame, &var);
    if (!compatible_with_win7_ && FAILED(hr)) {
      LOG(WARNING) << "Failed to set CODECAPI_AVEncVideoForceKeyFrame, "
                      "HRESULT: 0x" << std::hex << hr;
    }
  }

  hr = encoder_->ProcessInput(input_stream_id_, input_sample_.Get(), 0);
  if (FAILED(hr)) {
    NotifyError(kPlatformFailureError);
    RETURN_ON_HR_FAILURE(hr, "Couldn't encode", );
  }

  DVLOG(3) << "Sent for encode " << hr;
}

void MediaFoundationVideoEncodeAccelerator::ProcessOutput() {
  DVLOG(3) << __func__;
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());

  MFT_OUTPUT_DATA_BUFFER output_data_buffer = {0};
  output_data_buffer.dwStreamID = output_stream_id_;
  output_data_buffer.dwStatus = 0;
  output_data_buffer.pEvents = nullptr;
  output_data_buffer.pSample = nullptr;
  DWORD status = 0;
  HRESULT hr = encoder_->ProcessOutput(0, 1, &output_data_buffer, &status);
  if (hr == MF_E_TRANSFORM_STREAM_CHANGE) {
    hr = S_OK;
    Microsoft::WRL::ComPtr<IMFMediaType> media_type;
    for (DWORD type_index = 0; SUCCEEDED(hr); ++type_index) {
      hr = encoder_->GetOutputAvailableType(output_stream_id_, type_index,
                                            &media_type);
      if (SUCCEEDED(hr)) {
        break;
      }
    }
    hr = encoder_->SetOutputType(output_stream_id_, media_type.Get(), 0);
    return;
  }

  RETURN_ON_HR_FAILURE(hr, "Couldn't get encoded data", );
  DVLOG(3) << "Got encoded data " << hr;

  Microsoft::WRL::ComPtr<IMFMediaBuffer> output_buffer;
  hr = output_data_buffer.pSample->GetBufferByIndex(0, &output_buffer);
  RETURN_ON_HR_FAILURE(hr, "Couldn't get buffer by index", );

  DWORD size = 0;
  hr = output_buffer->GetCurrentLength(&size);
  RETURN_ON_HR_FAILURE(hr, "Couldn't get buffer length", );

  base::TimeDelta timestamp;
  LONGLONG sample_time;
  hr = output_data_buffer.pSample->GetSampleTime(&sample_time);
  if (SUCCEEDED(hr)) {
    timestamp = base::TimeDelta::FromMicroseconds(
        sample_time / kOneMicrosecondInMFSampleTimeUnits);
  }

  const bool keyframe = MFGetAttributeUINT32(
      output_data_buffer.pSample, MFSampleExtension_CleanPoint, false);
  DVLOG(3) << "Encoded data with size:" << size << " keyframe " << keyframe;

  // If no bit stream buffer presents, queue the output first.
  if (bitstream_buffer_queue_.empty()) {
    DVLOG(3) << "No bitstream buffers.";
    // We need to copy the output so that encoding can continue.
    std::unique_ptr<EncodeOutput> encode_output(
        new EncodeOutput(size, keyframe, timestamp));
    {
      MediaBufferScopedPointer scoped_buffer(output_buffer.Get());
      memcpy(encode_output->memory(), scoped_buffer.get(), size);
    }
    encoder_output_queue_.push_back(std::move(encode_output));
    output_data_buffer.pSample->Release();
    output_data_buffer.pSample = nullptr;
    return;
  }

  // Immediately return encoded buffer with BitstreamBuffer to client.
  std::unique_ptr<MediaFoundationVideoEncodeAccelerator::BitstreamBufferRef>
      buffer_ref = std::move(bitstream_buffer_queue_.front());
  bitstream_buffer_queue_.pop_front();

  {
    MediaBufferScopedPointer scoped_buffer(output_buffer.Get());
    memcpy(buffer_ref->mapping.memory(), scoped_buffer.get(), size);
  }

  output_data_buffer.pSample->Release();
  output_data_buffer.pSample = nullptr;

  main_client_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&Client::BitstreamBufferReady, main_client_,
                     buffer_ref->id,
                     BitstreamBufferMetadata(size, keyframe, timestamp)));
}

bool MediaFoundationVideoEncodeAccelerator::TryToDeliverInputFrame(
    scoped_refptr<VideoFrame> frame,
    bool force_keyframe) {
  bool input_delivered = false;
  Microsoft::WRL::ComPtr<IMFMediaEvent> media_event;
  MediaEventType event_type;
  do {
    HRESULT hr =
        event_generator_->GetEvent(MF_EVENT_FLAG_NO_WAIT, &media_event);
    if (FAILED(hr)) {
      break;
    }

    hr = media_event->GetType(&event_type);
    if (FAILED(hr)) {
      DLOG(ERROR) << "Failed to get the type of media event.";
      break;
    }

    switch (event_type) {
      case METransformHaveOutput: {
        ProcessOutput();
        continue;
      }
      case METransformNeedInput: {
        ProcessInput(frame, force_keyframe);
        return true;
      }
      default:
        break;
    }
  } while (true);

  return input_delivered;
}

void MediaFoundationVideoEncodeAccelerator::TryToReturnBitstreamBuffer() {
  // Try to fetch the encoded frame in time.
  bool output_processed = false;
  do {
    Microsoft::WRL::ComPtr<IMFMediaEvent> media_event;
    MediaEventType event_type;
    HRESULT hr =
        event_generator_->GetEvent(MF_EVENT_FLAG_NO_WAIT, &media_event);
    if (FAILED(hr)) {
      if (!output_processed) {
        continue;
      } else {
        break;
      }
    }

    hr = media_event->GetType(&event_type);
    if (FAILED(hr)) {
      DLOG(ERROR) << "Failed to get the type of media event.";
      break;
    }

    switch (event_type) {
      case METransformHaveOutput: {
        ProcessOutput();
        output_processed = true;
        break;
      }
      case METransformNeedInput: {
        input_required_ = true;
        continue;
      }
      default:
        break;
    }
  } while (true);
}

void MediaFoundationVideoEncodeAccelerator::UseOutputBitstreamBufferTask(
    std::unique_ptr<BitstreamBufferRef> buffer_ref) {
  DVLOG(3) << __func__;
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());

  // If there is already EncodeOutput waiting, copy its output first.
  if (!encoder_output_queue_.empty()) {
    std::unique_ptr<MediaFoundationVideoEncodeAccelerator::EncodeOutput>
        encode_output = std::move(encoder_output_queue_.front());
    encoder_output_queue_.pop_front();
    memcpy(buffer_ref->mapping.memory(), encode_output->memory(),
           encode_output->size());

    main_client_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&Client::BitstreamBufferReady, main_client_,
                       buffer_ref->id,
                       BitstreamBufferMetadata(
                           encode_output->size(), encode_output->keyframe,
                           encode_output->capture_timestamp)));
    return;
  }

  bitstream_buffer_queue_.push_back(std::move(buffer_ref));
}

void MediaFoundationVideoEncodeAccelerator::RequestEncodingParametersChangeTask(
    uint32_t bitrate,
    uint32_t framerate) {
  DVLOG(3) << __func__;
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());

  frame_rate_ =
      framerate
          ? std::min(framerate, static_cast<uint32_t>(kMaxFrameRateNumerator))
          : 1;

  if (target_bitrate_ != bitrate) {
    target_bitrate_ = bitrate ? bitrate : 1;
    VARIANT var;
    var.vt = VT_UI4;
    var.ulVal = target_bitrate_;
    HRESULT hr = codec_api_->SetValue(&CODECAPI_AVEncCommonMeanBitRate, &var);
    if (!compatible_with_win7_) {
      RETURN_ON_HR_FAILURE(hr, "Couldn't update bitrate", );
    }
  }
}

void MediaFoundationVideoEncodeAccelerator::DestroyTask() {
  DVLOG(3) << __func__;
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());

  // Cancel all encoder thread callbacks.
  encoder_task_weak_factory_.InvalidateWeakPtrs();

  ReleaseEncoderResources();
}

void MediaFoundationVideoEncodeAccelerator::ReleaseEncoderResources() {
  while (!bitstream_buffer_queue_.empty())
    bitstream_buffer_queue_.pop_front();
  while (!encoder_output_queue_.empty())
    encoder_output_queue_.pop_front();

  if (activate_.Get() != nullptr) {
    activate_->ShutdownObject();
    activate_->Release();
    activate_.Reset();
  }
  encoder_.Reset();
  codec_api_.Reset();
  event_generator_.Reset();
  imf_input_media_type_.Reset();
  imf_output_media_type_.Reset();
  input_sample_.Reset();
}

}  // namespace media
