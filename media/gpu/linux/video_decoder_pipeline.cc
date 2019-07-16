// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/linux/video_decoder_pipeline.h"

#include "base/bind.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "media/gpu/macros.h"
#include "media/gpu/video_frame_converter.h"

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

  frame_converter_->set_parent_task_runner(client_task_runner_);
}

VideoDecoderPipeline::~VideoDecoderPipeline() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void VideoDecoderPipeline::Destroy() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

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

  client_output_cb_ = std::move(output_cb);

  decoder_->Initialize(
      config, low_delay, cdm_context, std::move(init_cb),
      base::BindRepeating(&VideoDecoderPipeline::OnFrameDecodedThunk,
                          client_task_runner_, weak_this_factory_.GetWeakPtr()),
      std::move(waiting_cb));
}

void VideoDecoderPipeline::Reset(base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(acourbot) make the decoder jump into our own closure and only call
  // the client's when all parts of the pipeline are properly reset.
  decoder_->Reset(std::move(closure));
}

void VideoDecoderPipeline::Decode(scoped_refptr<DecoderBuffer> buffer,
                                  DecodeCB decode_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  decoder_->Decode(std::move(buffer), std::move(decode_cb));
}

// static
void VideoDecoderPipeline::OnFrameDecodedThunk(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    base::Optional<base::WeakPtr<VideoDecoderPipeline>> pipeline,
    scoped_refptr<VideoFrame> frame) {
  DCHECK(task_runner);
  DCHECK(pipeline);

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

  scoped_refptr<VideoFrame> converted_frame =
      frame_converter_->ConvertFrame(frame);
  if (!converted_frame) {
    // TODO(acourbot) we need to call the decode_cb with DECODE_ERROR here!
    VLOGF(1) << "Error converting frame!";
    return;
  }

  client_output_cb_.Run(converted_frame);
}

}  // namespace media
