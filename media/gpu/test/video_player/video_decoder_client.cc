// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/video_player/video_decoder_client.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/waiting.h"
#include "media/gpu/macros.h"
#include "media/gpu/test/video_decode_accelerator_unittest_helpers.h"
#include "media/gpu/test/video_player/frame_renderer.h"
#include "media/gpu/test/video_player/test_vda_video_decoder.h"
#include "media/gpu/test/video_player/video.h"

#if defined(OS_CHROMEOS)
#include "media/gpu/chromeos/chromeos_video_decoder_factory.h"
#include "media/gpu/linux/platform_video_frame_pool.h"
#include "media/gpu/video_frame_converter.h"
#endif  // defined(OS_CHROMEOS)

namespace media {
namespace test {

VideoDecoderClient::VideoDecoderClient(
    const Video* video,
    const VideoPlayer::EventCallback& event_cb,
    std::unique_ptr<FrameRenderer> renderer,
    std::vector<std::unique_ptr<VideoFrameProcessor>> frame_processors,
    const VideoDecoderClientConfig& config)
    : event_cb_(event_cb),
      frame_renderer_(std::move(renderer)),
      frame_processors_(std::move(frame_processors)),
      decoder_client_config_(config),
      decoder_client_thread_("VDAClientDecoderThread"),
      decoder_client_state_(VideoDecoderClientState::kUninitialized),
      video_(video) {
  DETACH_FROM_SEQUENCE(decoder_client_sequence_checker_);

  weak_this_ = weak_this_factory_.GetWeakPtr();
}

VideoDecoderClient::~VideoDecoderClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(video_player_sequence_checker_);

  DestroyDecoder();

  // Wait until the renderer and frame processors are done before destroying
  // them. This needs to be done after destroying the decoder so no new frames
  // will be queued while waiting.
  WaitForRenderer();
  WaitForFrameProcessors();
  frame_renderer_ = nullptr;
  frame_processors_.clear();

  decoder_client_thread_.Stop();
}

// static
std::unique_ptr<VideoDecoderClient> VideoDecoderClient::Create(
    const Video* video,
    const VideoPlayer::EventCallback& event_cb,
    std::unique_ptr<FrameRenderer> frame_renderer,
    std::vector<std::unique_ptr<VideoFrameProcessor>> frame_processors,
    const VideoDecoderClientConfig& config) {
  auto decoder_client = base::WrapUnique(
      new VideoDecoderClient(video, event_cb, std::move(frame_renderer),
                             std::move(frame_processors), config));
  if (!decoder_client->Initialize()) {
    return nullptr;
  }
  return decoder_client;
}

bool VideoDecoderClient::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(video_player_sequence_checker_);
  DCHECK(!decoder_client_thread_.IsRunning());
  DCHECK(event_cb_ && frame_renderer_);
  DCHECK(video_);

  encoded_data_helper_ =
      std::make_unique<EncodedDataHelper>(video_->Data(), video_->Profile());

  if (!decoder_client_thread_.Start()) {
    VLOGF(1) << "Failed to start decoder thread";
    return false;
  }

  CreateDecoder();

  return true;
}

void VideoDecoderClient::CreateDecoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(video_player_sequence_checker_);

  base::WaitableEvent done;
  decoder_client_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&VideoDecoderClient::CreateDecoderTask,
                                weak_this_, &done));
  done.Wait();
}

void VideoDecoderClient::DestroyDecoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(video_player_sequence_checker_);

  base::WaitableEvent done;
  decoder_client_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&VideoDecoderClient::DestroyDecoderTask,
                                weak_this_, &done));
  done.Wait();
}

bool VideoDecoderClient::WaitForFrameProcessors() {
  bool success = true;
  for (auto& frame_processor : frame_processors_)
    success &= frame_processor->WaitUntilDone();
  return success;
}

void VideoDecoderClient::WaitForRenderer() {
  LOG_ASSERT(frame_renderer_);
  frame_renderer_->WaitUntilRenderingDone();
}

FrameRenderer* VideoDecoderClient::GetFrameRenderer() const {
  return frame_renderer_.get();
}

void VideoDecoderClient::Play() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(video_player_sequence_checker_);

  decoder_client_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&VideoDecoderClient::PlayTask, weak_this_));
}

void VideoDecoderClient::Flush() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(video_player_sequence_checker_);

  decoder_client_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&VideoDecoderClient::FlushTask, weak_this_));
}

void VideoDecoderClient::Reset() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(video_player_sequence_checker_);

  decoder_client_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&VideoDecoderClient::ResetTask, weak_this_));
}

void VideoDecoderClient::CreateDecoderTask(base::WaitableEvent* done) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_client_sequence_checker_);
  LOG_ASSERT(!decoder_) << "Can't create decoder: already created";
  LOG_ASSERT(video_);

  VideoDecoderConfig config(
      video_->Codec(), video_->Profile(),
      VideoDecoderConfig::AlphaMode::kIsOpaque, VideoColorSpace(),
      kNoTransformation, video_->Resolution(), gfx::Rect(video_->Resolution()),
      video_->Resolution(), std::vector<uint8_t>(0), EncryptionScheme());

  VideoDecoder::InitCB init_cb = BindToCurrentLoop(
      base::BindOnce(&VideoDecoderClient::DecoderInitializedTask, weak_this_));
  VideoDecoder::OutputCB output_cb = BindToCurrentLoop(
      base::BindRepeating(&VideoDecoderClient::FrameReadyTask, weak_this_));
  WaitingCB waiting_cb =
      base::BindRepeating([](WaitingReason) { NOTIMPLEMENTED(); });

  if (decoder_client_config_.use_vd) {
#if defined(OS_CHROMEOS)
    decoder_ = ChromeosVideoDecoderFactory::Create(
        base::ThreadTaskRunnerHandle::Get(),
        std::make_unique<PlatformVideoFramePool>(),
        std::make_unique<VideoFrameConverter>());
#endif  // defined(OS_CHROMEOS)
    LOG_ASSERT(decoder_) << "Failed to create decoder.";
  } else {
    // The video decoder client expects decoders to use the VD interface. We can
    // use the TestVDAVideoDecoder wrapper here to test VDA-based video
    // decoders.
    decoder_ = std::make_unique<TestVDAVideoDecoder>(
        decoder_client_config_.allocation_mode, gfx::ColorSpace(),
        frame_renderer_.get());
  }

  decoder_->Initialize(config, false, nullptr, std::move(init_cb), output_cb,
                       waiting_cb);

  DCHECK_LE(decoder_client_config_.max_outstanding_decode_requests,
            static_cast<size_t>(decoder_->GetMaxDecodeRequests()));

  done->Signal();
}

void VideoDecoderClient::DestroyDecoderTask(base::WaitableEvent* done) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_client_sequence_checker_);
  DCHECK_EQ(VideoDecoderClientState::kIdle, decoder_client_state_);
  DCHECK_EQ(0u, num_outstanding_decode_requests_);
  DVLOGF(4);

  // Invalidate all scheduled tasks.
  weak_this_factory_.InvalidateWeakPtrs();

  // Destroy the decoder. This will destroy all video frames.
  if (decoder_) {
    decoder_.reset();
  }

  decoder_client_state_ = VideoDecoderClientState::kUninitialized;
  done->Signal();
}

void VideoDecoderClient::PlayTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_client_sequence_checker_);
  DVLOGF(4);

  // This method should only be called when the decoder client is idle. If
  // called e.g. while flushing, the behavior is undefined.
  ASSERT_EQ(decoder_client_state_, VideoDecoderClientState::kIdle);

  // Start decoding the first fragments. While in the decoding state new
  // fragments will automatically be fed to the decoder, when the decoder
  // notifies us it reached the end of a bitstream buffer.
  decoder_client_state_ = VideoDecoderClientState::kDecoding;
  for (size_t i = 0; i < decoder_client_config_.max_outstanding_decode_requests;
       ++i) {
    DecodeNextFragmentTask();
  }
}

void VideoDecoderClient::DecodeNextFragmentTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_client_sequence_checker_);
  DVLOGF(4);

  // Stop decoding fragments if we're no longer in the decoding state.
  if (decoder_client_state_ != VideoDecoderClientState::kDecoding)
    return;

  // Flush immediately when we reached the end of the stream. This changes the
  // state to kFlushing so further decode tasks will be aborted.
  if (encoded_data_helper_->ReachEndOfStream()) {
    FlushTask();
    return;
  }

  std::string fragment_bytes = encoded_data_helper_->GetBytesForNextData();
  size_t fragment_size = fragment_bytes.size();
  if (fragment_size == 0) {
    LOG(ERROR) << "Stream fragment has size 0";
    return;
  }

  scoped_refptr<DecoderBuffer> bitstream_buffer = DecoderBuffer::CopyFrom(
      reinterpret_cast<const uint8_t*>(fragment_bytes.data()), fragment_size);
  bitstream_buffer->set_timestamp(base::TimeTicks::Now().since_origin());

  VideoDecoder::DecodeCB decode_cb = BindToCurrentLoop(
      base::BindOnce(&VideoDecoderClient::DecodeDoneTask, weak_this_));
  decoder_->Decode(std::move(bitstream_buffer), std::move(decode_cb));

  num_outstanding_decode_requests_++;

  // Throw event when we encounter a config info in a H.264 stream.
  if (media::test::EncodedDataHelper::HasConfigInfo(
          reinterpret_cast<const uint8_t*>(fragment_bytes.data()),
          fragment_size, video_->Profile())) {
    FireEvent(VideoPlayerEvent::kConfigInfo);
  }
}

void VideoDecoderClient::FlushTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_client_sequence_checker_);
  DVLOGF(4);

  // Changing the state to flushing will abort any pending decodes.
  decoder_client_state_ = VideoDecoderClientState::kFlushing;

  VideoDecoder::DecodeCB flush_done_cb = BindToCurrentLoop(
      base::BindOnce(&VideoDecoderClient::FlushDoneTask, weak_this_));
  decoder_->Decode(DecoderBuffer::CreateEOSBuffer(), std::move(flush_done_cb));

  FireEvent(VideoPlayerEvent::kFlushing);
}

void VideoDecoderClient::ResetTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_client_sequence_checker_);
  DVLOGF(4);

  // Changing the state to resetting will abort any pending decodes.
  decoder_client_state_ = VideoDecoderClientState::kResetting;
  // TODO(dstaessens@) Allow resetting to any point in the stream.
  encoded_data_helper_->Rewind();
  base::RepeatingClosure reset_done_cb = BindToCurrentLoop(
      base::BindRepeating(&VideoDecoderClient::ResetDoneTask, weak_this_));
  decoder_->Reset(reset_done_cb);
  FireEvent(VideoPlayerEvent::kResetting);
}

void VideoDecoderClient::DecoderInitializedTask(bool status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_client_sequence_checker_);
  DCHECK_EQ(VideoDecoderClientState::kUninitialized, decoder_client_state_);
  LOG_ASSERT(status) << "Initializing decoder failed";

  decoder_client_state_ = VideoDecoderClientState::kIdle;
  FireEvent(VideoPlayerEvent::kInitialized);
}

void VideoDecoderClient::DecodeDoneTask(media::DecodeStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_client_sequence_checker_);
  DCHECK_NE(VideoDecoderClientState::kIdle, decoder_client_state_);
  LOG_ASSERT((status == media::DecodeStatus::OK) ||
             (status == media::DecodeStatus::ABORTED &&
              decoder_client_state_ == VideoDecoderClientState::kResetting))
      << "Decoding failed: " << GetDecodeStatusString(status);

  DVLOGF(4);

  num_outstanding_decode_requests_--;

  // Queue the next fragment to be decoded.
  decoder_client_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoDecoderClient::DecodeNextFragmentTask, weak_this_));
}

void VideoDecoderClient::FrameReadyTask(scoped_refptr<VideoFrame> video_frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_client_sequence_checker_);

  frame_renderer_->RenderFrame(video_frame);

  for (auto& frame_processor : frame_processors_)
    frame_processor->ProcessVideoFrame(video_frame, current_frame_index_);

  // Notify the test a frame has been decoded. We should only do this after
  // scheduling the frame to be processed, so calling WaitForFrameProcessors()
  // after receiving this event will always guarantee the frame to be processed.
  FireEvent(VideoPlayerEvent::kFrameDecoded);

  current_frame_index_++;
}

void VideoDecoderClient::FlushDoneTask(media::DecodeStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_client_sequence_checker_);
  DCHECK_EQ(0u, num_outstanding_decode_requests_);

  // Send an EOS frame to the renderer, so it can reset any internal state it
  // might keep in preparation of the next stream of video frames.
  frame_renderer_->RenderFrame(VideoFrame::CreateEOSFrame());
  decoder_client_state_ = VideoDecoderClientState::kIdle;
  FireEvent(VideoPlayerEvent::kFlushDone);
}

void VideoDecoderClient::ResetDoneTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_client_sequence_checker_);
  DCHECK_EQ(0u, num_outstanding_decode_requests_);

  // We finished resetting to a different point in the stream, so we should
  // update the frame index. Currently only resetting to the start of the stream
  // is supported, so we can set the frame index to zero here.
  current_frame_index_ = 0;

  frame_renderer_->RenderFrame(VideoFrame::CreateEOSFrame());
  decoder_client_state_ = VideoDecoderClientState::kIdle;
  FireEvent(VideoPlayerEvent::kResetDone);
}

void VideoDecoderClient::FireEvent(VideoPlayerEvent event) {
  bool continue_decoding = event_cb_.Run(event);
  if (!continue_decoding) {
    // Changing the state to idle will abort any pending decodes.
    decoder_client_state_ = VideoDecoderClientState::kIdle;
  }
}

}  // namespace test
}  // namespace media
