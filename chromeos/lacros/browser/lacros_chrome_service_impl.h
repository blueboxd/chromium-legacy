// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_LACROS_BROWSER_LACROS_CHROME_SERVICE_IMPL_H_
#define CHROMEOS_LACROS_BROWSER_LACROS_CHROME_SERVICE_IMPL_H_

#include "base/component_export.h"
#include "chromeos/lacros/mojom/lacros.mojom.h"
#include "chromeos/lacros/mojom/screen_manager.mojom.h"
#include "chromeos/lacros/mojom/select_file.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {

// Implements LacrosChromeService, which owns the mojo remote connection to
// ash-chrome.
// This class is not thread safe. It can only be used on the main thread.
class COMPONENT_EXPORT(CHROMEOS_LACROS) LacrosChromeServiceImpl
    : public lacros::mojom::LacrosChromeService {
 public:
  static LacrosChromeServiceImpl* Get();

  LacrosChromeServiceImpl();
  ~LacrosChromeServiceImpl() override;

  void BindReceiver(
      mojo::PendingReceiver<lacros::mojom::LacrosChromeService> receiver);

  mojo::Remote<lacros::mojom::SelectFile>& select_file_remote() {
    return select_file_remote_;
  }

  void BindScreenManagerReceiver(
      mojo::PendingReceiver<lacros::mojom::ScreenManager> pending_receiver);

  // lacros::mojom::LacrosChromeService:
  void RequestAshChromeServiceReceiver(
      RequestAshChromeServiceReceiverCallback callback) override;

 private:
  mojo::Receiver<lacros::mojom::LacrosChromeService> receiver_{this};

  // Proxy to AshChromeService in ash-chrome.
  mojo::Remote<lacros::mojom::AshChromeService> ash_chrome_service_;

  // Pending receiver of AshChromeService.
  // AshChromeService is bound to mojo::Remote on construction, then
  // when AshChromeService requests via RequestAshChromeServiceReceiver,
  // its PendingReceiver is returned.
  // This member holds the PendingReceiver between them. Note that even
  // during the period, calling a method on AshChromeService via Remote
  // should be available.
  mojo::PendingReceiver<lacros::mojom::AshChromeService>
      pending_ash_chrome_service_receiver_;

  // Proxy to SelectFile interface in ash-chrome.
  mojo::Remote<lacros::mojom::SelectFile> select_file_remote_;
};

}  // namespace chromeos

#endif  // CHROMEOS_LACROS_BROWSER_LACROS_CHROME_SERVICE_IMPL_H_
