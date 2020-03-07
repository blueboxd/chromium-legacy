// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SODA_SODA_SERVICE_H_
#define CHROME_BROWSER_SODA_SODA_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"
#include "media/mojo/mojom/soda_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

class Profile;

namespace soda {

// Provides a mojo endpoint in the browser that allows the renderer process to
// launch and initialize the sandboxed Speech On-Device API (SODA) service
// process.
class SodaService : public KeyedService {
 public:
  explicit SodaService();
  SodaService(const SodaService&) = delete;
  SodaService& operator=(const SodaService&) = delete;
  ~SodaService() override;

  void Create(mojo::PendingReceiver<media::mojom::SodaContext> receiver);

 private:
  // Launches the SODA service in a sandboxed utility process.
  void LaunchIfNotRunning();

  // The remote to the SODA service. The browser will not launch a new SODA
  // service process if this remote is already bound.
  mojo::Remote<media::mojom::SodaService> soda_service_;
};

}  // namespace soda

#endif  // CHROME_BROWSER_SODA_SODA_SERVICE_H_
