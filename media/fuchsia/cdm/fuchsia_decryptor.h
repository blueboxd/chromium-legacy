// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FUCHSIA_CDM_FUCHSIA_DECRYPTOR_H_
#define MEDIA_FUCHSIA_CDM_FUCHSIA_DECRYPTOR_H_

#include <memory>

#include "media/base/decryptor.h"

namespace fuchsia {
namespace media {
namespace drm {
class ContentDecryptionModule;
}  // namespace drm
}  // namespace media
}  // namespace fuchsia

namespace media {

class FuchsiaClearStreamDecryptor;

class FuchsiaDecryptor : public Decryptor {
 public:
  // Caller should make sure |cdm| lives longer than this class.
  explicit FuchsiaDecryptor(fuchsia::media::drm::ContentDecryptionModule* cdm);
  ~FuchsiaDecryptor() override;

  // media::Decryptor implementation:
  void RegisterNewKeyCB(StreamType stream_type,
                        const NewKeyCB& key_added_cb) override;
  void Decrypt(StreamType stream_type,
               scoped_refptr<DecoderBuffer> encrypted,
               const DecryptCB& decrypt_cb) override;
  void CancelDecrypt(StreamType stream_type) override;
  void InitializeAudioDecoder(const AudioDecoderConfig& config,
                              const DecoderInitCB& init_cb) override;
  void InitializeVideoDecoder(const VideoDecoderConfig& config,
                              const DecoderInitCB& init_cb) override;
  void DecryptAndDecodeAudio(scoped_refptr<DecoderBuffer> encrypted,
                             const AudioDecodeCB& audio_decode_cb) override;
  void DecryptAndDecodeVideo(scoped_refptr<DecoderBuffer> encrypted,
                             const VideoDecodeCB& video_decode_cb) override;
  void ResetDecoder(StreamType stream_type) override;
  void DeinitializeDecoder(StreamType stream_type) override;
  bool CanAlwaysDecrypt() override;

 private:
  fuchsia::media::drm::ContentDecryptionModule* const cdm_;

  std::unique_ptr<FuchsiaClearStreamDecryptor> audio_decryptor_;

  DISALLOW_COPY_AND_ASSIGN(FuchsiaDecryptor);
};

}  // namespace media

#endif  // MEDIA_FUCHSIA_CDM_FUCHSIA_DECRYPTOR_H_
