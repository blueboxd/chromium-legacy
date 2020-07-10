// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LACROS_SCREEN_MANAGER_CROSAPI_H_
#define CHROME_BROWSER_CHROMEOS_LACROS_SCREEN_MANAGER_CROSAPI_H_

#include <stdint.h>

#include <map>

#include "base/memory/weak_ptr.h"
#include "chromeos/lacros/mojom/screen_manager.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/image/image.h"

// This class is the ash-chrome implementation of the ScreenManager interface.
// This class must only be used from the main thread.
class ScreenManagerCrosapi : public lacros::mojom::ScreenManager,
                             aura::WindowObserver {
 public:
  ScreenManagerCrosapi();
  ScreenManagerCrosapi(const ScreenManagerCrosapi&) = delete;
  ScreenManagerCrosapi& operator=(const ScreenManagerCrosapi&) = delete;
  ~ScreenManagerCrosapi() override;

  void BindReceiver(
      mojo::PendingReceiver<lacros::mojom::ScreenManager> receiver);

  // lacros::mojom::ScreenManager:
  void TakeScreenSnapshot(TakeScreenSnapshotCallback callback) override;
  void ListWindows(ListWindowsCallback callback) override;
  void TakeWindowSnapshot(uint64_t id,
                          TakeWindowSnapshotCallback callback) override;

  // aura::WindowObserver
  // This method is overridden purely to remove dead windows from
  // |id_to_window_| and |window_to_id_|. This ensures that if the pointer is
  // reused for a new window, it does not get confused with a previous window.
  void OnWindowDestroying(aura::Window* window) final;

 private:
  using SnapshotCallback =
      base::OnceCallback<void(const lacros::WindowSnapshot&)>;
  void DidTakeSnapshot(SnapshotCallback callback, gfx::Image image);

  // This class generates unique, non-reused IDs for windows on demand. The IDs
  // are monotonically increasing 64-bit integers. Once an ID is assigned to a
  // window, this class listens for the destruction of the window in order to
  // remove dead windows from the map.
  //
  // The members |id_to_window_| and |window_to_id_| must be kept in sync. The
  // members exist to allow fast lookup in both directions.
  std::map<uint64_t, aura::Window*> id_to_window_;
  std::map<aura::Window*, uint64_t> window_to_id_;
  uint64_t next_window_id_ = 0;

  // This class supports any number of connections. This allows the client to
  // have multiple, potentially thread-affine, remotes. This is needed by
  // WebRTC.
  mojo::ReceiverSet<lacros::mojom::ScreenManager> receivers_;

  base::WeakPtrFactory<ScreenManagerCrosapi> weak_factory_{this};
};

#endif  // CHROME_BROWSER_CHROMEOS_LACROS_SCREEN_MANAGER_CROSAPI_H_
