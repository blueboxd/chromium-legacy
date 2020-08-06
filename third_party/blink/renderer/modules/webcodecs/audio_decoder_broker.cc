// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/audio_decoder_broker.h"

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "build/buildflag.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_factory.h"
#include "media/base/media_util.h"
#include "media/base/status_codes.h"
#include "media/mojo/buildflags.h"
#include "media/mojo/clients/mojo_decoder_factory.h"
#include "media/mojo/mojom/interface_factory.mojom.h"
#include "media/renderers/default_decoder_factory.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/webcodecs/decoder_selector.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

using DecoderDetails = blink::AudioDecoderBroker::DecoderDetails;

namespace WTF {

template <>
struct CrossThreadCopier<media::AudioDecoderConfig>
    : public CrossThreadCopierPassThrough<media::AudioDecoderConfig> {
  STATIC_ONLY(CrossThreadCopier);
};

template <>
struct CrossThreadCopier<media::Status>
    : public CrossThreadCopierPassThrough<media::Status> {
  STATIC_ONLY(CrossThreadCopier);
};

template <>
struct CrossThreadCopier<base::Optional<DecoderDetails>>
    : public CrossThreadCopierPassThrough<base::Optional<DecoderDetails>> {
  STATIC_ONLY(CrossThreadCopier);
};

}  // namespace WTF

namespace blink {

// Wrapper class for state and API calls that must be made from the
// |media_task_runner_|. Construction must happen on blink main thread to safely
// make use of ExecutionContext and Document. These GC blink types must not be
// stored/referenced by any other method.
class MediaAudioTaskWrapper {
 public:
  using CrossThreadOnceInitCB =
      WTF::CrossThreadOnceFunction<void(media::Status status,
                                        base::Optional<DecoderDetails>)>;
  using CrossThreadOnceDecodeCB =
      WTF::CrossThreadOnceFunction<void(media::DecodeStatus)>;
  using CrossThreadOnceResetCB = WTF::CrossThreadOnceClosure;

  MediaAudioTaskWrapper(
      base::WeakPtr<CrossThreadAudioDecoderClient> weak_client,
      ExecutionContext& execution_context,
      scoped_refptr<base::SingleThreadTaskRunner> media_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner)
      : weak_client_(std::move(weak_client)),
        media_task_runner_(std::move(media_task_runner)),
        main_task_runner_(std::move(main_task_runner)) {
    DVLOG(2) << __func__;
    DETACH_FROM_SEQUENCE(sequence_checker_);

    // TODO(chcunningham): Enable this for workers. Currently only a
    // frame-binding (RenderFrameHostImpl) is exposed.
    // TODO(chcunningham): set_disconnect_handler?
    // Mojo connection setup must occur here on the main thread where its safe
    // to use |execution_context| APIs.
    mojo::PendingRemote<media::mojom::InterfaceFactory> media_interface_factory;
    execution_context.GetBrowserInterfaceBroker().GetInterface(
        media_interface_factory.InitWithNewPipeAndPassReceiver());

    // Mojo remote must be bound on media thread where it will be used.
    //|Unretained| is safe because |this| must be destroyed on the media task
    // runner.
    PostCrossThreadTask(
        *media_task_runner_, FROM_HERE,
        WTF::CrossThreadBindOnce(&MediaAudioTaskWrapper::BindOnTaskRunner,
                                 WTF::CrossThreadUnretained(this),
                                 std::move(media_interface_factory)));
  }

  virtual ~MediaAudioTaskWrapper() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  MediaAudioTaskWrapper(const MediaAudioTaskWrapper&) = delete;
  MediaAudioTaskWrapper& operator=(const MediaAudioTaskWrapper&) = delete;

  void Initialize(const media::AudioDecoderConfig& config,
                  CrossThreadOnceInitCB init_cb) {
    DVLOG(2) << __func__;
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    selector_ = std::make_unique<WebCodecsAudioDecoderSelector>(
        media_task_runner_,
        WTF::BindRepeating(&MediaAudioTaskWrapper::OnCreateDecoders,
                           WTF::Unretained(this)),
        WTF::BindRepeating(&MediaAudioTaskWrapper::OnDecodeOutput,
                           WTF::Unretained(this)));

    selector_->SelectDecoder(
        config, WTF::Bind(&MediaAudioTaskWrapper::OnDecoderSelected,
                          WTF::Unretained(this), std::move(init_cb)));
  }

  void Decode(scoped_refptr<media::DecoderBuffer> buffer,
              CrossThreadOnceDecodeCB decode_cb) {
    DVLOG(2) << __func__;
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (!decoder_) {
      std::move(decode_cb).Run(media::DecodeStatus::DECODE_ERROR);
      return;
    }

    decoder_->Decode(std::move(buffer),
                     WTF::Bind(&MediaAudioTaskWrapper::OnDecodeDone,
                               WTF::Unretained(this), std::move(decode_cb)));
  }

  void Reset(CrossThreadOnceResetCB reset_cb) {
    DVLOG(2) << __func__;
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (!decoder_) {
      std::move(reset_cb).Run();
      return;
    }

    decoder_->Reset(WTF::Bind(&MediaAudioTaskWrapper::OnReset,
                              WTF::Unretained(this), std::move(reset_cb)));
  }

 private:
  void BindOnTaskRunner(
      mojo::PendingRemote<media::mojom::InterfaceFactory> interface_factory) {
    DVLOG(2) << __func__;
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    media_interface_factory_.Bind(std::move(interface_factory));

    // Bind the |interface_factory_| above before passing to
    // |external_decoder_factory|.
    std::unique_ptr<media::DecoderFactory> external_decoder_factory;
#if BUILDFLAG(ENABLE_MOJO_AUDIO_DECODER)
    external_decoder_factory = std::make_unique<media::MojoDecoderFactory>(
        media_interface_factory_.get());
#endif
    decoder_factory_ = std::make_unique<media::DefaultDecoderFactory>(
        std::move(external_decoder_factory));
  }

  std::vector<std::unique_ptr<media::AudioDecoder>> OnCreateDecoders() {
    DVLOG(2) << __func__;
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    std::vector<std::unique_ptr<media::AudioDecoder>> audio_decoders;
    decoder_factory_->CreateAudioDecoders(media_task_runner_, &null_media_log_,
                                          &audio_decoders);

    return audio_decoders;
  }

  void OnDecoderSelected(CrossThreadOnceInitCB init_cb,
                         std::unique_ptr<media::AudioDecoder> decoder) {
    DVLOG(2) << __func__;
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // We're done with it.
    DCHECK(selector_);
    selector_.reset();

    decoder_ = std::move(decoder);

    media::Status status(media::StatusCode::kDecoderUnsupportedConfig);
    base::Optional<DecoderDetails> decoder_details;
    if (decoder_) {
      status = media::OkStatus();
      decoder_details = DecoderDetails({decoder_->GetDisplayName(),
                                        decoder_->IsPlatformDecoder(),
                                        decoder_->NeedsBitstreamConversion()});
    }

    // Fire |init_cb|.
    PostCrossThreadTask(
        *main_task_runner_, FROM_HERE,
        WTF::CrossThreadBindOnce(std::move(init_cb), status, decoder_details));
  }

  void OnDecodeOutput(scoped_refptr<media::AudioBuffer> buffer) {
    DVLOG(2) << __func__;
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    PostCrossThreadTask(
        *main_task_runner_, FROM_HERE,
        WTF::CrossThreadBindOnce(&CrossThreadAudioDecoderClient::OnDecodeOutput,
                                 weak_client_, std::move(buffer)));
  }

  void OnDecodeDone(CrossThreadOnceDecodeCB decode_cb,
                    media::DecodeStatus status) {
    DVLOG(2) << __func__;
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    PostCrossThreadTask(*main_task_runner_, FROM_HERE,
                        WTF::CrossThreadBindOnce(std::move(decode_cb), status));
  }

  void OnReset(CrossThreadOnceResetCB reset_cb) {
    DVLOG(2) << __func__;
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    PostCrossThreadTask(*main_task_runner_, FROM_HERE, std::move(reset_cb));
  }

  base::WeakPtr<CrossThreadAudioDecoderClient> weak_client_;
  scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  mojo::Remote<media::mojom::InterfaceFactory> media_interface_factory_;
  std::unique_ptr<WebCodecsAudioDecoderSelector> selector_;
  std::unique_ptr<media::DefaultDecoderFactory> decoder_factory_;
  std::unique_ptr<media::AudioDecoder> decoder_;
  gfx::ColorSpace target_color_space_;

  // TODO(chcunningham): Route MEDIA_LOG for WebCodecs.
  media::NullMediaLog null_media_log_;

  SEQUENCE_CHECKER(sequence_checker_);
};

constexpr char AudioDecoderBroker::kDefaultDisplayName[];

AudioDecoderBroker::AudioDecoderBroker(ExecutionContext& execution_context)
    : media_task_runner_(
          // TODO(chcunningham): This should use a separate thread from the
          // pool. http://crbug.com/1095786
          execution_context.GetTaskRunner(TaskType::kInternalMedia)) {
  DVLOG(2) << __func__;
  media_tasks_ = std::make_unique<MediaAudioTaskWrapper>(
      weak_factory_.GetWeakPtr(), execution_context, media_task_runner_,
      execution_context.GetTaskRunner(TaskType::kInternalMedia));
}

AudioDecoderBroker::~AudioDecoderBroker() {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  media_task_runner_->DeleteSoon(FROM_HERE, std::move(media_tasks_));
}

std::string AudioDecoderBroker::GetDisplayName() const {
  return decoder_details_ ? decoder_details_->display_name
                          : AudioDecoderBroker::kDefaultDisplayName;
}

bool AudioDecoderBroker::IsPlatformDecoder() const {
  return decoder_details_ ? decoder_details_->is_platform_decoder : false;
}

void AudioDecoderBroker::Initialize(const media::AudioDecoderConfig& config,
                                    media::CdmContext* cdm_context,
                                    InitCB init_cb,
                                    const OutputCB& output_cb,
                                    const media::WaitingCB& waiting_cb) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The following are not currently supported in WebCodecs.
  DCHECK(!cdm_context);
  DCHECK(!waiting_cb);

  output_cb_ = output_cb;

  // Clear details from previously initialized decoder. New values will arrive
  // via OnInitialize().
  decoder_details_.reset();

  MediaAudioTaskWrapper::CrossThreadOnceInitCB main_loop_init_cb(
      WTF::Bind(&AudioDecoderBroker::OnInitialize, weak_factory_.GetWeakPtr(),
                std::move(init_cb)));

  PostCrossThreadTask(
      *media_task_runner_, FROM_HERE,
      WTF::CrossThreadBindOnce(&MediaAudioTaskWrapper::Initialize,
                               WTF::CrossThreadUnretained(media_tasks_.get()),
                               config, std::move(main_loop_init_cb)));
}

void AudioDecoderBroker::OnInitialize(InitCB init_cb,
                                      media::Status status,
                                      base::Optional<DecoderDetails> details) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  decoder_details_ = details;
  std::move(init_cb).Run(status);
}

void AudioDecoderBroker::Decode(scoped_refptr<media::DecoderBuffer> buffer,
                                DecodeCB decode_cb) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  MediaAudioTaskWrapper::CrossThreadOnceDecodeCB main_loop_cb(
      WTF::Bind(&AudioDecoderBroker::OnDecodeDone, weak_factory_.GetWeakPtr(),
                std::move(decode_cb)));

  PostCrossThreadTask(
      *media_task_runner_, FROM_HERE,
      WTF::CrossThreadBindOnce(&MediaAudioTaskWrapper::Decode,
                               WTF::CrossThreadUnretained(media_tasks_.get()),
                               buffer, std::move(main_loop_cb)));
}

void AudioDecoderBroker::OnDecodeDone(DecodeCB decode_cb,
                                      media::DecodeStatus status) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(decode_cb).Run(status);
}

void AudioDecoderBroker::Reset(base::OnceClosure reset_cb) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  MediaAudioTaskWrapper::CrossThreadOnceResetCB main_loop_cb(
      WTF::Bind(&AudioDecoderBroker::OnReset, weak_factory_.GetWeakPtr(),
                std::move(reset_cb)));

  PostCrossThreadTask(
      *media_task_runner_, FROM_HERE,
      WTF::CrossThreadBindOnce(&MediaAudioTaskWrapper::Reset,
                               WTF::CrossThreadUnretained(media_tasks_.get()),
                               std::move(main_loop_cb)));
}

bool AudioDecoderBroker::NeedsBitstreamConversion() const {
  return decoder_details_ ? decoder_details_->needs_bitstream_conversion
                          : false;
}

void AudioDecoderBroker::OnReset(base::OnceClosure reset_cb) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(reset_cb).Run();
}

void AudioDecoderBroker::OnDecodeOutput(
    scoped_refptr<media::AudioBuffer> buffer) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(output_cb_);

  output_cb_.Run(std::move(buffer));
}

}  // namespace blink
