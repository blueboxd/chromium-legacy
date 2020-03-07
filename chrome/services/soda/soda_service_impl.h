// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SODA_SODA_SERVICE_IMPL_H_
#define CHROME_SERVICES_SODA_SODA_SERVICE_IMPL_H_

#include "media/mojo/mojom/soda_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace soda {

class SodaServiceImpl : public media::mojom::SodaService,
                        public media::mojom::SodaContext {
 public:
  explicit SodaServiceImpl(
      mojo::PendingReceiver<media::mojom::SodaService> receiver);
  ~SodaServiceImpl() override;

  // media::mojom::SodaService
  void BindContext(
      mojo::PendingReceiver<media::mojom::SodaContext> context) override;

  // media::mojom::SodaContext
  void BindRecognizer(
      mojo::PendingReceiver<media::mojom::SodaRecognizer> receiver,
      mojo::PendingRemote<media::mojom::SodaRecognizerClient> client) override;

 private:
  mojo::Receiver<media::mojom::SodaService> receiver_;

  // The set of receivers used to receive messages from the renderer clients.
  mojo::ReceiverSet<media::mojom::SodaContext> soda_contexts_;

  DISALLOW_COPY_AND_ASSIGN(SodaServiceImpl);
};

}  // namespace soda

#endif  // CHROME_SERVICES_SODA_SODA_SERVICE_IMPL_H_
