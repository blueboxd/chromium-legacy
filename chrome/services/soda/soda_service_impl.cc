// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/soda/soda_service_impl.h"

#include "chrome/services/soda/soda_recognizer_impl.h"

namespace soda {

SodaServiceImpl::SodaServiceImpl(
    mojo::PendingReceiver<media::mojom::SodaService> receiver)
    : receiver_(this, std::move(receiver)) {}

SodaServiceImpl::~SodaServiceImpl() = default;

void SodaServiceImpl::BindContext(
    mojo::PendingReceiver<media::mojom::SodaContext> context) {
  soda_contexts_.Add(this, std::move(context));
}

void SodaServiceImpl::BindRecognizer(
    mojo::PendingReceiver<media::mojom::SodaRecognizer> receiver,
    mojo::PendingRemote<media::mojom::SodaRecognizerClient> client) {
  SodaRecognizerImpl::Create(std::move(receiver), std::move(client));
}

}  // namespace soda
