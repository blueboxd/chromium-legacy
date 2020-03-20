// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SODA_SODA_RECOGNIZER_IMPL_H_
#define CHROME_SERVICES_SODA_SODA_RECOGNIZER_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "build/branding_buildflags.h"
#include "chrome/services/soda/buildflags.h"
#include "media/mojo/mojom/soda_service.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace soda {

class SodaClient;

class SodaRecognizerImpl : public media::mojom::SodaRecognizer {
 public:
  using OnRecognitionEventCallback =
      base::RepeatingCallback<void(const std::string& result)>;

  ~SodaRecognizerImpl() override;

  static void Create(
      mojo::PendingReceiver<media::mojom::SodaRecognizer> receiver,
      mojo::PendingRemote<media::mojom::SodaRecognizerClient> remote);

  OnRecognitionEventCallback recognition_event_callback() const {
    return recognition_event_callback_;
  }

 private:
  explicit SodaRecognizerImpl(
      mojo::PendingRemote<media::mojom::SodaRecognizerClient> remote);

  // Convert the audio buffer into the appropriate format and feed the raw audio
  // into the SODA instance.
  void SendAudioToSoda(media::mojom::AudioDataS16Ptr buffer) final;

  // Return the transcribed audio from the recognition event back to the caller
  // via the recognition event client.
  void OnRecognitionEvent(const std::string& result);

  // The remote endpoint for the mojo pipe used to return transcribed audio from
  // the SODA service back to the renderer.
  mojo::Remote<media::mojom::SodaRecognizerClient> client_remote_;

#if BUILDFLAG(ENABLE_SODA)
  std::unique_ptr<SodaClient> soda_client_;
#endif  // BUILDFLAG(ENABLE_SODA)

  // The callback that is eventually executed on a speech recognition event
  // which passes the transcribed audio back to the caller via the SODA
  // recognition event client remote.
  OnRecognitionEventCallback recognition_event_callback_;

  base::WeakPtrFactory<SodaRecognizerImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SodaRecognizerImpl);
};

}  // namespace soda

#endif  // CHROME_SERVICES_SODA_SODA_RECOGNIZER_IMPL_H_
