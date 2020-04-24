// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_WEBCODECS_WC_DECODER_SELECTOR_H_
#define MEDIA_WEBCODECS_WC_DECODER_SELECTOR_H_

#include <memory>

#include "media/base/demuxer_stream.h"
#include "media/base/media_export.h"
#include "media/base/media_util.h"
#include "media/filters/decoder_selector.h"
#include "media/filters/decoder_stream_traits.h"

namespace media {

template <DemuxerStream::Type StreamType>
class ShimDemuxerStream;

template <DemuxerStream::Type StreamType>
class MEDIA_EXPORT WebCodecsDecoderSelector {
 public:
  typedef DecoderStreamTraits<StreamType> StreamTraits;
  typedef typename StreamTraits::DecoderType Decoder;
  typedef typename StreamTraits::DecoderConfigType DecoderConfig;

  // Callback to create a list of decoders to select from.
  using CreateDecodersCB =
      base::RepeatingCallback<std::vector<std::unique_ptr<Decoder>>()>;

  // Emits the result of a single call to SelectDecoder(). Parameter is
  // the initialized Decoder. nullptr if selection failed. The caller owns the
  // Decoder.
  using SelectDecoderCB = base::OnceCallback<void(std::unique_ptr<Decoder>)>;

  WebCodecsDecoderSelector(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      CreateDecodersCB create_decoders_cb,
      typename Decoder::OutputCB output_cb);

  // Aborts any pending decoder selection.
  ~WebCodecsDecoderSelector();

  // Selects and initializes a decoder using |config|. Decoder will
  // be returned via |select_decoder_cb| posted to |task_runner_|. Subsequent
  // calls will again select from the full list of decoders.
  void SelectDecoder(const DecoderConfig& config,
                     SelectDecoderCB select_decoder_cb);

 private:
  // Helper to create |stream_traits_|.
  std::unique_ptr<StreamTraits> CreateStreamTraits();

  // Proxy SelectDecoderCB from impl_ to our |select_decoder_cb|.
  void OnDecoderSelected(SelectDecoderCB select_decoder_cb,
                         std::unique_ptr<Decoder> decoder,
                         std::unique_ptr<DecryptingDemuxerStream>);

  // Implements heavy lifting for decoder selection.
  DecoderSelector<StreamType> impl_;

  // Shim to satisfy dependencies of |impl_|. Provides DecoderConfig to |impl_|.
  std::unique_ptr<ShimDemuxerStream<StreamType>> demuxer_stream_;

  // Helper to unify API for configuring audio/video decoders.
  std::unique_ptr<StreamTraits> stream_traits_;

  // Repeating callback for decoder outputs.
  typename Decoder::OutputCB output_cb_;

  // TODO(chcunningham): Route MEDIA_LOG for WebCodecs.
  NullMediaLog null_media_log_;
};

typedef WebCodecsDecoderSelector<DemuxerStream::VIDEO>
    WebCodecsVideoDecoderSelector;
typedef WebCodecsDecoderSelector<DemuxerStream::AUDIO>
    WebCodecsAudioDecoderSelector;

}  // namespace media

#endif  // MEDIA_WEBCODECS_WC_DECODER_SELECTOR_H_
