// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/renderer/cast_streaming_demuxer.h"

#include "base/bind.h"
#include "base/sequence_checker.h"
#include "base/single_thread_task_runner.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_decoder_config.h"
#include "media/mojo/common/mojo_decoder_buffer_converter.h"

namespace {

// media::DemuxerStream shared audio/video implementation for Cast Streaming.
// Receives buffers on the main thread and sends them to the media thread.
class CastStreamingDemuxerStream : public media::DemuxerStream {
 public:
  explicit CastStreamingDemuxerStream(
      mojo::ScopedDataPipeConsumerHandle consumer)
      : decoder_buffer_reader_(std::move(consumer)) {}
  ~CastStreamingDemuxerStream() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  // TODO(crbug.com/1042501): Receive |buffer| through a Mojo interface.
  void ReceiveBuffer(media::mojom::DecoderBufferPtr buffer) {
    DVLOG(3) << __func__;
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    pending_buffer_metadata_.push_back(std::move(buffer));
    GetNextBuffer();
  }

  void AbortPendingRead() {
    DVLOG(3) << __func__;
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (pending_read_cb_)
      std::move(pending_read_cb_).Run(Status::kAborted, nullptr);
  }

 private:
  void CompletePendingRead() {
    DVLOG(3) << __func__;
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!pending_read_cb_ || !current_buffer_)
      return;

    std::move(pending_read_cb_).Run(Status::kOk, std::move(current_buffer_));
    GetNextBuffer();
  }

  void GetNextBuffer() {
    DVLOG(3) << __func__;
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (current_buffer_ || pending_buffer_metadata_.empty())
      return;

    media::mojom::DecoderBufferPtr buffer =
        std::move(pending_buffer_metadata_.front());
    pending_buffer_metadata_.pop_front();
    decoder_buffer_reader_.ReadDecoderBuffer(
        std::move(buffer),
        base::BindOnce(&CastStreamingDemuxerStream::OnBufferRead,
                       base::Unretained(this)));
  }

  void OnBufferRead(scoped_refptr<media::DecoderBuffer> buffer) {
    DVLOG(3) << __func__;
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(!current_buffer_);

    current_buffer_ = buffer;
    CompletePendingRead();
  }

  // DemuxerStream implementation.
  void Read(ReadCB read_cb) final {
    DVLOG(3) << __func__;
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(pending_read_cb_.is_null());
    pending_read_cb_ = std::move(read_cb);
    CompletePendingRead();
  }
  bool IsReadPending() const final { return !pending_read_cb_.is_null(); }
  Liveness liveness() const final { return Liveness::LIVENESS_LIVE; }
  bool SupportsConfigChanges() final { return false; }

  media::MojoDecoderBufferReader decoder_buffer_reader_;

  ReadCB pending_read_cb_;
  base::circular_deque<media::mojom::DecoderBufferPtr> pending_buffer_metadata_;
  scoped_refptr<media::DecoderBuffer> current_buffer_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace

class CastStreamingAudioDemuxerStream : public CastStreamingDemuxerStream {
 public:
  CastStreamingAudioDemuxerStream(media::AudioDecoderConfig decoder_config,
                                  mojo::ScopedDataPipeConsumerHandle consumer)
      : CastStreamingDemuxerStream(std::move(consumer)),
        config_(decoder_config) {}
  ~CastStreamingAudioDemuxerStream() final = default;

 private:
  // DemuxerStream implementation.
  media::AudioDecoderConfig audio_decoder_config() final { return config_; }
  media::VideoDecoderConfig video_decoder_config() final {
    NOTREACHED();
    return media::VideoDecoderConfig();
  }
  Type type() const final { return Type::AUDIO; }

  media::AudioDecoderConfig config_;
};

class CastStreamingVideoDemuxerStream : public CastStreamingDemuxerStream {
 public:
  CastStreamingVideoDemuxerStream(media::VideoDecoderConfig decoder_config,
                                  mojo::ScopedDataPipeConsumerHandle consumer)
      : CastStreamingDemuxerStream(std::move(consumer)),
        config_(decoder_config) {}
  ~CastStreamingVideoDemuxerStream() final = default;

 private:
  // DemuxerStream implementation.
  media::AudioDecoderConfig audio_decoder_config() final {
    NOTREACHED();
    return media::AudioDecoderConfig();
  }
  media::VideoDecoderConfig video_decoder_config() final { return config_; }
  Type type() const final { return Type::VIDEO; }

  media::VideoDecoderConfig config_;
};

CastStreamingDemuxer::CastStreamingDemuxer(
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner)
    : media_task_runner_(media_task_runner) {
  DVLOG(1) << __func__;
}

CastStreamingDemuxer::~CastStreamingDemuxer() = default;

std::vector<media::DemuxerStream*> CastStreamingDemuxer::GetAllStreams() {
  DVLOG(1) << __func__;
  DCHECK(media_task_runner_->BelongsToCurrentThread());

  std::vector<media::DemuxerStream*> streams;
  if (video_stream_)
    streams.push_back(video_stream_.get());
  if (audio_stream_)
    streams.push_back(audio_stream_.get());
  return streams;
}

std::string CastStreamingDemuxer::GetDisplayName() const {
  return "CastStreamingDemuxer";
}

void CastStreamingDemuxer::Initialize(media::DemuxerHost* host,
                                      media::PipelineStatusCallback status_cb) {
  DVLOG(1) << __func__;
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  host_ = host;

  // Live streams have infinite duration.
  host_->SetDuration(media::kInfiniteDuration);

  // TODO(crbug.com/1042501): Properly initialize the demuxer once the mojo
  // service has been implemented.
  std::move(status_cb).Run(media::PipelineStatus::PIPELINE_OK);
}

void CastStreamingDemuxer::AbortPendingReads() {
  if (audio_stream_)
    audio_stream_->AbortPendingRead();
  if (video_stream_)
    video_stream_->AbortPendingRead();
}

// Not supported.
void CastStreamingDemuxer::StartWaitingForSeek(base::TimeDelta seek_time) {}

// Not supported.
void CastStreamingDemuxer::CancelPendingSeek(base::TimeDelta seek_time) {}

// Not supported.
void CastStreamingDemuxer::Seek(base::TimeDelta time,
                                media::PipelineStatusCallback status_cb) {
  std::move(status_cb).Run(media::PipelineStatus::PIPELINE_OK);
}

void CastStreamingDemuxer::Stop() {
  DVLOG(1) << __func__;

  if (audio_stream_)
    audio_stream_.reset();
  if (video_stream_)
    video_stream_.reset();
}

base::TimeDelta CastStreamingDemuxer::GetStartTime() const {
  return base::TimeDelta();
}

// Not supported.
base::Time CastStreamingDemuxer::GetTimelineOffset() const {
  return base::Time();
}

// Not supported.
int64_t CastStreamingDemuxer::GetMemoryUsage() const {
  return 0;
}

base::Optional<media::container_names::MediaContainerName>
CastStreamingDemuxer::GetContainerForMetrics() const {
  // Cast Streaming frames have no container.
  return base::nullopt;
}

// Not supported.
void CastStreamingDemuxer::OnEnabledAudioTracksChanged(
    const std::vector<media::MediaTrack::Id>& track_ids,
    base::TimeDelta curr_time,
    TrackChangeCB change_completed_cb) {
  DLOG(WARNING) << "Track changes are not supported.";
  std::vector<media::DemuxerStream*> streams;
  std::move(change_completed_cb).Run(media::DemuxerStream::AUDIO, streams);
}

// Not supported.
void CastStreamingDemuxer::OnSelectedVideoTrackChanged(
    const std::vector<media::MediaTrack::Id>& track_ids,
    base::TimeDelta curr_time,
    TrackChangeCB change_completed_cb) {
  DLOG(WARNING) << "Track changes are not supported.";
  std::vector<media::DemuxerStream*> streams;
  std::move(change_completed_cb).Run(media::DemuxerStream::VIDEO, streams);
}
