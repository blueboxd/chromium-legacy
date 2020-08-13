// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSAPI_ASH_CHROME_SERVICE_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_CROSAPI_ASH_CHROME_SERVICE_IMPL_H_

#include <memory>

#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class ScreenManagerCrosapi;

namespace crosapi {

class AttestationAsh;
class SelectFileAsh;

// Implementation of AshChromeService. It provides a set of APIs that
// lacros-chrome can call into.
class AshChromeServiceImpl : public mojom::AshChromeService {
 public:
  explicit AshChromeServiceImpl(
      mojo::PendingReceiver<mojom::AshChromeService> pending_receiver);
  ~AshChromeServiceImpl() override;

  // crosapi::mojom::AshChromeService:
  void BindAttestation(
      mojo::PendingReceiver<crosapi::mojom::Attestation> receiver) override;
  void BindScreenManager(
      mojo::PendingReceiver<mojom::ScreenManager> receiver) override;
  void BindSelectFile(
      mojo::PendingReceiver<mojom::SelectFile> receiver) override;

 private:
  mojo::Receiver<mojom::AshChromeService> receiver_;

  std::unique_ptr<crosapi::AttestationAsh> attestation_ash_;
  std::unique_ptr<ScreenManagerCrosapi> screen_manager_crosapi_;
  std::unique_ptr<SelectFileAsh> select_file_crosapi_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_CHROMEOS_CROSAPI_ASH_CHROME_SERVICE_IMPL_H_
