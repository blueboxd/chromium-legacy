// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LACROS_SCREEN_MANAGER_CROSAPI_H_
#define CHROME_BROWSER_CHROMEOS_LACROS_SCREEN_MANAGER_CROSAPI_H_

#include "base/memory/weak_ptr.h"
#include "chromeos/lacros/mojom/screen_manager.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "ui/gfx/image/image.h"

// This class is the ash-chrome implementation of the ScreenManager interface.
// This class must only be used from the main thread.
class ScreenManagerCrosapi : public lacros::mojom::ScreenManager {
 public:
  ScreenManagerCrosapi();
  ScreenManagerCrosapi(const ScreenManagerCrosapi&) = delete;
  ScreenManagerCrosapi& operator=(const ScreenManagerCrosapi&) = delete;
  ~ScreenManagerCrosapi() override;

  void BindReceiver(
      mojo::PendingReceiver<lacros::mojom::ScreenManager> receiver);

  // lacros::mojom::ScreenManager:
  void TakeScreenSnapshot(TakeScreenSnapshotCallback callback) override;

 private:
  void DidTakeScreenSnapshot(TakeScreenSnapshotCallback callback,
                             gfx::Image image);

  // This class supports any number of connections. This allows the client to
  // have multiple, potentially thread-affine, remotes. This is needed by
  // WebRTC.
  mojo::ReceiverSet<lacros::mojom::ScreenManager> receivers_;

  base::WeakPtrFactory<ScreenManagerCrosapi> weak_factory_{this};
};

#endif  // CHROME_BROWSER_CHROMEOS_LACROS_SCREEN_MANAGER_CROSAPI_H_
