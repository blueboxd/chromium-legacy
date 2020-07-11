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
class SelectFileCrosapi;

// Implementation of AshChromeService. It provides a set of APIs that
// lacros-chrome can call into.
class AshChromeServiceImpl : public crosapi::mojom::AshChromeService {
 public:
  explicit AshChromeServiceImpl(
      mojo::PendingReceiver<crosapi::mojom::AshChromeService> pending_receiver);
  ~AshChromeServiceImpl() override;

  // crosapi::mojom::AshChromeService:
  void BindScreenManager(
      mojo::PendingReceiver<crosapi::mojom::ScreenManager> receiver) override;
  void BindSelectFile(
      mojo::PendingReceiver<crosapi::mojom::SelectFile> receiver) override;

 private:
  mojo::Receiver<crosapi::mojom::AshChromeService> receiver_;

  std::unique_ptr<ScreenManagerCrosapi> screen_manager_crosapi_;
  std::unique_ptr<SelectFileCrosapi> select_file_crosapi_;
};

#endif  // CHROME_BROWSER_CHROMEOS_CROSAPI_ASH_CHROME_SERVICE_IMPL_H_
