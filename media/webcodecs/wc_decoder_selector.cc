// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webcodecs/wc_decoder_selector.h"

#include "base/bind.h"
#include "base/check_op.h"
#include "base/notreached.h"
#include "base/single_thread_task_runner.h"
#include "media/base/channel_layout.h"
#include "media/base/demuxer_stream.h"
#include "media/filters/decrypting_demuxer_stream.h"

namespace media {

// Demuxing isn't part of WebCodecs. This shim allows us to reuse decoder
// selection logic from <video>.
// TODO(chcunningham): Maybe refactor DecoderSelector to separate dependency on
// DemuxerStream. DecoderSelection doesn't conceptually require a Demuxer. The
// tough part is re-working DecryptingDemuxerStream.
template <DemuxerStream::Type StreamType>
class ShimDemuxerStream : public DemuxerStream {
 public:
  using DecoderConfigType =
      typename DecoderStreamTraits<StreamType>::DecoderConfigType;

  ~ShimDemuxerStream() override = default;

  void Read(ReadCB read_cb) override { NOTREACHED(); }
  bool IsReadPending() const override {
    NOTREACHED();
    return false;
  }

  void Configure(DecoderConfigType config);

  AudioDecoderConfig audio_decoder_config() override {
    DCHECK_EQ(type(), DemuxerStream::AUDIO);
    return audio_decoder_config_;
  }

  VideoDecoderConfig video_decoder_config() override {
    DCHECK_EQ(type(), DemuxerStream::VIDEO);
    return video_decoder_config_;
  }

  Type type() const override { return stream_type; }

  bool SupportsConfigChanges() override {
    NOTREACHED();
    return true;
  }

 private:
  static const DemuxerStream::Type stream_type = StreamType;

  AudioDecoderConfig audio_decoder_config_;
  VideoDecoderConfig video_decoder_config_;
};

template <>
void ShimDemuxerStream<DemuxerStream::AUDIO>::Configure(
    DecoderConfigType config) {
  audio_decoder_config_ = config;
}

template <>
void ShimDemuxerStream<DemuxerStream::VIDEO>::Configure(
    DecoderConfigType config) {
  video_decoder_config_ = config;
}

template <DemuxerStream::Type StreamType>
WebCodecsDecoderSelector<StreamType>::WebCodecsDecoderSelector(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    CreateDecodersCB create_decoders_cb,
    typename Decoder::OutputCB output_cb)
    : impl_(std::move(task_runner),
            std::move(create_decoders_cb),
            &null_media_log_),
      demuxer_stream_(new ShimDemuxerStream<StreamType>()),
      stream_traits_(CreateStreamTraits()),
      output_cb_(output_cb) {
  impl_.Initialize(stream_traits_.get(), demuxer_stream_.get(),
                   nullptr /*CdmContext*/, WaitingCB());
}

template <DemuxerStream::Type StreamType>
WebCodecsDecoderSelector<StreamType>::~WebCodecsDecoderSelector() {}

template <DemuxerStream::Type StreamType>
void WebCodecsDecoderSelector<StreamType>::SelectDecoder(
    const DecoderConfig& config,
    SelectDecoderCB select_decoder_cb) {
  // |impl_| will internally use this the |config| from our ShimDemuxerStream.
  demuxer_stream_->Configure(config);

  // |impl_| uses a WeakFactory for its SelectDecoderCB, so we're safe to use
  // Unretained here.
  impl_.SelectDecoder(
      base::BindOnce(&WebCodecsDecoderSelector<StreamType>::OnDecoderSelected,
                     base::Unretained(this), std::move(select_decoder_cb)),
      output_cb_);
}

template <>
std::unique_ptr<WebCodecsAudioDecoderSelector::StreamTraits>
WebCodecsDecoderSelector<DemuxerStream::AUDIO>::CreateStreamTraits() {
  // TODO(chcunningham): Consider plumbing real hw channel layout.
  return std::make_unique<WebCodecsDecoderSelector::StreamTraits>(
      &null_media_log_, CHANNEL_LAYOUT_NONE);
}

template <>
std::unique_ptr<WebCodecsVideoDecoderSelector::StreamTraits>
WebCodecsDecoderSelector<DemuxerStream::VIDEO>::CreateStreamTraits() {
  return std::make_unique<WebCodecsDecoderSelector::StreamTraits>(
      &null_media_log_);
}

template <DemuxerStream::Type StreamType>
void WebCodecsDecoderSelector<StreamType>::OnDecoderSelected(
    SelectDecoderCB select_decoder_cb,
    std::unique_ptr<Decoder> decoder,
    std::unique_ptr<DecryptingDemuxerStream> decrypting_demuxer_stream) {
  DCHECK(!decrypting_demuxer_stream);

  // We immediately finalize decoder selection. From a spec POV we strongly
  // prefer to avoid replicating our internal design of having to wait for the
  // first frame to arrive before we consider configuration successful.
  // TODO(chcunningham): Measure first frame decode failures and find other ways
  // to solve (or minimize) the problem.
  impl_.FinalizeDecoderSelection();

  std::move(select_decoder_cb).Run(std::move(decoder));
}

template class WebCodecsDecoderSelector<DemuxerStream::VIDEO>;
template class WebCodecsDecoderSelector<DemuxerStream::AUDIO>;

}  // namespace media
