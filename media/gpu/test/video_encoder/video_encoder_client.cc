// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/video_encoder/video_encoder_client.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "gpu/config/gpu_preferences.h"
#include "media/gpu/gpu_video_encode_accelerator_factory.h"
#include "media/gpu/macros.h"
#include "media/gpu/test/bitstream_helpers.h"
#include "media/gpu/test/video.h"
#include "media/gpu/test/video_test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace test {

namespace {

// TODO(crbug.com/1045825): Support encoding parameter changes.

// Callbacks can be called from any thread, but WeakPtrs are not thread-safe.
// This helper thunk wraps a WeakPtr into an 'Optional' value, so the WeakPtr is
// only dereferenced after rescheduling the task on the specified task runner.
template <typename CallbackFunc, typename... CallbackArgs>
void CallbackThunk(
    base::Optional<base::WeakPtr<VideoEncoderClient>> encoder_client,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    CallbackFunc func,
    CallbackArgs... args) {
  DCHECK(encoder_client);
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(func, *encoder_client, args...));
}
}  // namespace

VideoEncoderClient::VideoEncoderClient(
    const VideoEncoder::EventCallback& event_cb,
    std::vector<std::unique_ptr<BitstreamProcessor>> bitstream_processors,
    const VideoEncoderClientConfig& config)
    : event_cb_(event_cb),
      bitstream_processors_(std::move(bitstream_processors)),
      encoder_client_config_(config),
      encoder_client_thread_("VDAClientEncoderThread"),
      encoder_client_state_(VideoEncoderClientState::kUninitialized) {
  DETACH_FROM_SEQUENCE(encoder_client_sequence_checker_);

  weak_this_ = weak_this_factory_.GetWeakPtr();
}

VideoEncoderClient::~VideoEncoderClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(test_sequence_checker_);

  Destroy();

  // Wait until the bitstream processors are done before destroying them. This
  // needs to be done after destroying the encoder so no new bitstream buffers
  // will be queued while waiting.
  WaitForBitstreamProcessors();
  bitstream_processors_.clear();
}

// static
std::unique_ptr<VideoEncoderClient> VideoEncoderClient::Create(
    const VideoEncoder::EventCallback& event_cb,
    std::vector<std::unique_ptr<BitstreamProcessor>> bitstream_processors,
    const VideoEncoderClientConfig& config) {
  return base::WrapUnique(new VideoEncoderClient(
      event_cb, std::move(bitstream_processors), config));
}

bool VideoEncoderClient::Initialize(const Video* video) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(test_sequence_checker_);
  DCHECK(video);

  if (!encoder_client_thread_.Start()) {
    VLOGF(1) << "Failed to start encoder thread";
    return false;
  }
  encoder_client_task_runner_ = encoder_client_thread_.task_runner();

  bool success = false;
  base::WaitableEvent done;
  encoder_client_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VideoEncoderClient::CreateEncoderTask,
                                weak_this_, video, &success, &done));
  done.Wait();
  return success;
}

void VideoEncoderClient::Destroy() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(test_sequence_checker_);

  if (!encoder_client_thread_.IsRunning())
    return;

  base::WaitableEvent done;
  encoder_client_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VideoEncoderClient::DestroyEncoderTask,
                                weak_this_, &done));
  done.Wait();

  encoder_client_thread_.Stop();
}

void VideoEncoderClient::Encode() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(test_sequence_checker_);

  encoder_client_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VideoEncoderClient::EncodeTask, weak_this_));
}

void VideoEncoderClient::Flush() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(test_sequence_checker_);

  encoder_client_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VideoEncoderClient::FlushTask, weak_this_));
}

bool VideoEncoderClient::WaitForBitstreamProcessors() {
  bool success = true;
  for (auto& bitstream_processor : bitstream_processors_)
    success &= bitstream_processor->WaitUntilDone();
  return success;
}

void VideoEncoderClient::RequireBitstreamBuffers(
    unsigned int input_count,
    const gfx::Size& input_coded_size,
    size_t output_buffer_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_client_sequence_checker_);
  ASSERT_EQ(encoder_client_state_, VideoEncoderClientState::kUninitialized);
  ASSERT_EQ(bitstream_buffers_.size(), 0u);
  ASSERT_GT(input_count, 0UL);
  ASSERT_GT(output_buffer_size, 0UL);
  DVLOGF(4);

  // TODO(crbug.com/1045825): Add support for videos with a visible rectangle
  // not starting at (0,0).
  aligned_data_helper_ = std::make_unique<AlignedDataHelper>(
      video_->Data(), video_->NumFrames(), video_->PixelFormat(),
      gfx::Rect(video_->Resolution()), input_coded_size);

  output_buffer_size_ = output_buffer_size;

  // Create output buffers.
  // TODO(dstaessens): Replace with hiroh@'s upcoming bitstream buffer manager.
  for (unsigned int i = 0; i < input_count; ++i) {
    auto shm = std::make_unique<base::UnsafeSharedMemoryRegion>();
    *shm = base::UnsafeSharedMemoryRegion::Create(output_buffer_size_);
    LOG_ASSERT(shm->IsValid());

    BitstreamBuffer bitstream_buffer(
        GetNextBitstreamBufferId(),
        base::UnsafeSharedMemoryRegion::TakeHandleForSerialization(
            shm->Duplicate()),
        output_buffer_size_);

    bitstream_buffers_.insert(
        std::make_pair(bitstream_buffer.id(), std::move(shm)));

    encoder_->UseOutputBitstreamBuffer(std::move(bitstream_buffer));
  }

  // Notify the test video encoder that initialization is now complete.
  encoder_client_state_ = VideoEncoderClientState::kIdle;
  FireEvent(VideoEncoder::EncoderEvent::kInitialized);
}

void VideoEncoderClient::BitstreamBufferReady(
    int32_t bitstream_buffer_id,
    const BitstreamBufferMetadata& metadata) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_client_sequence_checker_);

  auto it = bitstream_buffers_.find(bitstream_buffer_id);
  ASSERT_NE(it, bitstream_buffers_.end());

  for (auto& bitstream_processor : bitstream_processors_) {
    bitstream_processor->ProcessBitstreamBuffer(bitstream_buffer_id, metadata,
                                                it->second.get());
  }

  // Notify the test an encoded bitstream buffer is ready. We should only do
  // this after scheduling the bitstream to be processed, so calling
  // WaitForBitstreamProcessors() after receiving this event will always
  // guarantee the bitstream to be processed.
  FireEvent(VideoEncoder::EncoderEvent::kBitstreamReady);

  // Currently output buffers are returned immediately to the encoder.
  // TODO(dstaessens): Add support for asynchronous bitstream buffer processing.
  BitstreamBuffer bitstream_buffer(
      bitstream_buffer_id,
      base::UnsafeSharedMemoryRegion::TakeHandleForSerialization(
          it->second->Duplicate()),
      output_buffer_size_);
  encoder_->UseOutputBitstreamBuffer(std::move(bitstream_buffer));
}

void VideoEncoderClient::NotifyError(VideoEncodeAccelerator::Error error) {}

void VideoEncoderClient::NotifyEncoderInfoChange(const VideoEncoderInfo& info) {
}

void VideoEncoderClient::CreateEncoderTask(const Video* video,
                                           bool* success,
                                           base::WaitableEvent* done) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_client_sequence_checker_);
  DCHECK_EQ(encoder_client_state_, VideoEncoderClientState::kUninitialized);
  ASSERT_TRUE(!encoder_) << "Can't create encoder: already created";
  ASSERT_TRUE(video);

  video_ = video;

  const VideoEncodeAccelerator::Config config(
      video_->PixelFormat(), video_->Resolution(),
      encoder_client_config_.output_profile, encoder_client_config_.bitrate,
      encoder_client_config_.framerate);
  encoder_ = GpuVideoEncodeAcceleratorFactory::CreateVEA(config, this,
                                                         gpu::GpuPreferences());
  *success = (encoder_ != nullptr);

  // Initialization is continued once the encoder notifies us of the coded size
  // in RequireBitstreamBuffers().
  done->Signal();
}

void VideoEncoderClient::DestroyEncoderTask(base::WaitableEvent* done) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_client_sequence_checker_);
  DCHECK_EQ(0u, num_outstanding_encode_requests_);
  DVLOGF(4);

  // Invalidate all scheduled tasks.
  weak_this_factory_.InvalidateWeakPtrs();

  // Destroy the encoder. This will destroy all video frames.
  encoder_ = nullptr;

  encoder_client_state_ = VideoEncoderClientState::kUninitialized;
  done->Signal();
}

void VideoEncoderClient::EncodeTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_client_sequence_checker_);
  ASSERT_EQ(encoder_client_state_, VideoEncoderClientState::kIdle);
  DVLOGF(4);

  // Start encoding the first frames. While in the encoding state new frames
  // will automatically be fed to the encoder, when an input frame is returned
  // to us in EncodeDoneTask().
  encoder_client_state_ = VideoEncoderClientState::kEncoding;
  for (size_t i = 0; i < encoder_client_config_.max_outstanding_encode_requests;
       ++i) {
    EncodeNextFrameTask();
  }
}

void VideoEncoderClient::EncodeNextFrameTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_client_sequence_checker_);
  DVLOGF(4);

  // Stop encoding frames if we're no longer in the encoding state.
  if (encoder_client_state_ != VideoEncoderClientState::kEncoding)
    return;

  // Flush immediately when we reached the end of the stream. This changes the
  // state to kFlushing so further encode tasks will be aborted.
  if (aligned_data_helper_->AtEndOfStream()) {
    FlushTask();
    return;
  }

  scoped_refptr<VideoFrame> video_frame = aligned_data_helper_->GetNextFrame();
  video_frame->AddDestructionObserver(base::BindOnce(
      CallbackThunk<decltype(&VideoEncoderClient::EncodeDoneTask),
                    base::TimeDelta>,
      weak_this_, encoder_client_task_runner_,
      &VideoEncoderClient::EncodeDoneTask, video_frame->timestamp()));

  // TODO(dstaessens): Add support for forcing key frames.
  bool force_keyframe = false;
  encoder_->Encode(video_frame, force_keyframe);

  num_outstanding_encode_requests_++;
}

void VideoEncoderClient::FlushTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_client_sequence_checker_);
  DVLOGF(4);

  // Changing the state to flushing will abort any pending encodes.
  encoder_client_state_ = VideoEncoderClientState::kFlushing;

  // If the encoder does not support flush, immediately consider flushing done.
  if (!encoder_->IsFlushSupported()) {
    FireEvent(VideoEncoder::EncoderEvent::kFlushing);
    FlushDoneTask(true);
    return;
  }

  auto flush_done_cb = base::BindOnce(
      CallbackThunk<decltype(&VideoEncoderClient::FlushDoneTask), bool>,
      weak_this_, encoder_client_task_runner_,
      &VideoEncoderClient::FlushDoneTask);
  encoder_->Flush(std::move(flush_done_cb));

  FireEvent(VideoEncoder::EncoderEvent::kFlushing);
}

void VideoEncoderClient::EncodeDoneTask(base::TimeDelta timestamp) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_client_sequence_checker_);
  DCHECK_NE(VideoEncoderClientState::kIdle, encoder_client_state_);
  DVLOGF(4);

  num_outstanding_encode_requests_--;

  FireEvent(VideoEncoder::EncoderEvent::kFrameReleased);

  // Queue the next frame to be encoded.
  encoder_client_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoEncoderClient::EncodeNextFrameTask, weak_this_));
}

void VideoEncoderClient::FlushDoneTask(bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_client_sequence_checker_);
  DCHECK_EQ(0u, num_outstanding_encode_requests_);
  ASSERT_TRUE(success) << "Failed to flush encoder";

  encoder_client_state_ = VideoEncoderClientState::kIdle;
  FireEvent(VideoEncoder::EncoderEvent::kFlushDone);
}

void VideoEncoderClient::FireEvent(VideoEncoder::EncoderEvent event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_client_sequence_checker_);

  bool continue_encoding = event_cb_.Run(event);
  if (!continue_encoding) {
    // Changing the state to idle will abort any pending encodes.
    encoder_client_state_ = VideoEncoderClientState::kIdle;
  }
}

int32_t VideoEncoderClient::GetNextBitstreamBufferId() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_client_sequence_checker_);
  // The bitstream buffer ID should always be positive, negative values are
  // reserved for uninitialized buffers.
  next_bitstream_buffer_id_ = (next_bitstream_buffer_id_ + 1) & 0x7FFFFFFF;
  return next_bitstream_buffer_id_;
}

}  // namespace test
}  // namespace media
