// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/linux/video_decoder_pipeline.h"

#include "base/bind.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "media/base/video_decoder_config.h"
#include "media/gpu/macros.h"

namespace media {

VideoDecoderPipeline::VideoDecoderPipeline(
    scoped_refptr<base::SequencedTaskRunner> client_task_runner,
    std::unique_ptr<VideoDecoder> decoder,
    std::unique_ptr<VideoFrameConverter> frame_converter)
    : client_task_runner_(std::move(client_task_runner)),
      decoder_(std::move(decoder)),
      frame_converter_(std::move(frame_converter)),
      weak_this_factory_(this) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(decoder_);
  DCHECK(frame_converter_);
  DCHECK(client_task_runner_);
  DVLOGF(2);

  frame_converter_->Initialize(
      client_task_runner_,
      base::BindRepeating(&VideoDecoderPipeline::OnFrameConverted,
                          weak_this_factory_.GetWeakPtr()));
}

VideoDecoderPipeline::~VideoDecoderPipeline() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(3);
}

void VideoDecoderPipeline::Destroy() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(2);

  delete this;
}

std::string VideoDecoderPipeline::GetDisplayName() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return decoder_->GetDisplayName();
}

bool VideoDecoderPipeline::IsPlatformDecoder() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return decoder_->IsPlatformDecoder();
}

int VideoDecoderPipeline::GetMaxDecodeRequests() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return decoder_->GetMaxDecodeRequests();
}

bool VideoDecoderPipeline::NeedsBitstreamConversion() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return decoder_->NeedsBitstreamConversion();
}

bool VideoDecoderPipeline::CanReadWithoutStalling() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return decoder_->CanReadWithoutStalling();
}

void VideoDecoderPipeline::Initialize(const VideoDecoderConfig& config,
                                      bool low_delay,
                                      CdmContext* cdm_context,
                                      InitCB init_cb,
                                      const OutputCB& output_cb,
                                      const WaitingCB& waiting_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOGF(2) << "config: " << config.AsHumanReadableString();

  client_output_cb_ = std::move(output_cb);

  decoder_->Initialize(
      config, low_delay, cdm_context, std::move(init_cb),
      base::BindRepeating(&VideoDecoderPipeline::OnFrameDecodedThunk,
                          client_task_runner_, weak_this_factory_.GetWeakPtr()),
      std::move(waiting_cb));
}

void VideoDecoderPipeline::Reset(base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!client_reset_cb_);
  DVLOGF(3);

  client_reset_cb_ = std::move(closure);
  decoder_->Reset(base::BindOnce(&VideoDecoderPipeline::OnResetDone,
                                 weak_this_factory_.GetWeakPtr()));
}

void VideoDecoderPipeline::OnResetDone() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(client_reset_cb_);
  DVLOGF(3);

  frame_converter_->AbortPendingFrames();

  CallFlushCbIfNeeded(DecodeStatus::ABORTED);

  std::move(client_reset_cb_).Run();
}

void VideoDecoderPipeline::Decode(scoped_refptr<DecoderBuffer> buffer,
                                  DecodeCB decode_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(4);

  bool is_flush = buffer->end_of_stream();
  decoder_->Decode(std::move(buffer),
                   base::BindOnce(&VideoDecoderPipeline::OnDecodeDone,
                                  weak_this_factory_.GetWeakPtr(), is_flush,
                                  std::move(decode_cb)));
}

void VideoDecoderPipeline::OnDecodeDone(bool is_flush,
                                        DecodeCB decode_cb,
                                        DecodeStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(4) << "is_flush: " << is_flush << ", status: " << status;

  if (has_error_)
    status = DecodeStatus::DECODE_ERROR;

  if (is_flush && status == DecodeStatus::OK) {
    client_flush_cb_ = std::move(decode_cb);
    // TODO(akahuang): The order between flush cb and output cb is preserved
    // only when OnFrameDecodedThunk() run on |client_task_runner_|. Remove
    // OnFrameDecodedThunk() when we make sure all VD callbacks are called on
    // the same thread.
    CallFlushCbIfNeeded(DecodeStatus::OK);
    return;
  }

  std::move(decode_cb).Run(status);
}

// static
void VideoDecoderPipeline::OnFrameDecodedThunk(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    base::Optional<base::WeakPtr<VideoDecoderPipeline>> pipeline,
    scoped_refptr<VideoFrame> frame) {
  DCHECK(task_runner);
  DCHECK(pipeline);
  DVLOGF(4);

  // Workaround for some decoder's non-conformant behavior:
  // Decoders are supposed to call the output callback "as soon as possible",
  // i.e. directly in their own thread. If they do so the OnFrameDecoded task is
  // scheduled on the client task queue, and we have no race condition if we
  // are destroyed after that.
  //
  // But some decoders will run the output callback on their client thread, i.e.
  // our own task runner. If we get destroyed before that task is processed,
  // then OnFrameDecoded would be scheduled after our destruction and thus
  // would never be run, making the client miss a frame.
  //
  // So we first check whether we already are running on our task runner, and
  // execute OnFrameDecoded without delay in that case. Hopefully this can be
  // removed in the future.
  //
  // TODO fix the Mojo service so we don't need to do this dance anymore.
  if (task_runner->RunsTasksInCurrentSequence()) {
    if (*pipeline)
      (*pipeline)->OnFrameDecoded(std::move(frame));
    return;
  }

  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(&VideoDecoderPipeline::OnFrameDecoded,
                                       *pipeline, std::move(frame)));
}

void VideoDecoderPipeline::OnFrameDecoded(scoped_refptr<VideoFrame> frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(frame_converter_);
  DVLOGF(4);

  frame_converter_->ConvertFrame(std::move(frame));
}

void VideoDecoderPipeline::OnFrameConverted(scoped_refptr<VideoFrame> frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(4);

  if (!frame)
    return OnError("Frame converter returns null frame.");
  if (has_error_) {
    DVLOGF(2) << "Skip returning frames after error occurs.";
    return;
  }

  client_output_cb_.Run(std::move(frame));

  // After outputting a frame, flush might be completed.
  CallFlushCbIfNeeded(DecodeStatus::OK);
}

void VideoDecoderPipeline::OnError(const std::string& msg) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOGF(1) << msg;

  has_error_ = true;
  CallFlushCbIfNeeded(DecodeStatus::DECODE_ERROR);
}

void VideoDecoderPipeline::CallFlushCbIfNeeded(DecodeStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(3) << "status: " << status;

  if (!client_flush_cb_)
    return;

  // Flush is not completed yet.
  if (status == DecodeStatus::OK && frame_converter_->HasPendingFrames())
    return;

  std::move(client_flush_cb_).Run(status);
}

}  // namespace media
