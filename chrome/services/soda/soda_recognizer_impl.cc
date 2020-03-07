// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/soda/soda_recognizer_impl.h"

#include "base/bind.h"
#include "media/base/bind_to_current_loop.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace soda {

SodaRecognizerImpl::~SodaRecognizerImpl() = default;

void SodaRecognizerImpl::Create(
    mojo::PendingReceiver<media::mojom::SodaRecognizer> receiver,
    mojo::PendingRemote<media::mojom::SodaRecognizerClient> remote) {
  mojo::MakeSelfOwnedReceiver(
      base::WrapUnique(new SodaRecognizerImpl(std::move(remote))),
      std::move(receiver));
}

void SodaRecognizerImpl::OnRecognitionEvent(const std::string& result) {
  if (client_remote_.is_bound()) {
    client_remote_->OnSodaRecognitionEvent(result);
  } else {
    NOTREACHED();
  }
}

SodaRecognizerImpl::SodaRecognizerImpl(
    mojo::PendingRemote<media::mojom::SodaRecognizerClient> remote)
    : client_remote_(std::move(remote)) {
  recognition_event_callback_ = media::BindToCurrentLoop(base::Bind(
      &SodaRecognizerImpl::OnRecognitionEvent, weak_factory_.GetWeakPtr()));
}

}  // namespace soda
