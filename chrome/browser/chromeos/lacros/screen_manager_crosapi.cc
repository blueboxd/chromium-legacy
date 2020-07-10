// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/lacros/screen_manager_crosapi.h"

#include <utility>
#include <vector>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "base/files/file_path.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/lacros/cpp/window_snapshot.h"
#include "ui/snapshot/snapshot.h"

ScreenManagerCrosapi::ScreenManagerCrosapi() = default;

ScreenManagerCrosapi::~ScreenManagerCrosapi() {
  for (auto pair : window_to_id_) {
    pair.first->RemoveObserver(this);
  }
}

void ScreenManagerCrosapi::BindReceiver(
    mojo::PendingReceiver<lacros::mojom::ScreenManager> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void ScreenManagerCrosapi::TakeScreenSnapshot(
    TakeScreenSnapshotCallback callback) {
  // TODO(https://crbug.com/1094460): Handle display selection and multiple
  // displays.
  aura::Window* primary_window = ash::Shell::GetPrimaryRootWindow();

  ui::GrabWindowSnapshotAsync(
      primary_window, primary_window->bounds(),
      base::BindOnce(&ScreenManagerCrosapi::DidTakeSnapshot,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ScreenManagerCrosapi::ListWindows(ListWindowsCallback callback) {
  // TODO(https://crbug.com/1094460): Handle window selection and multiple
  // virtual desktops.
  aura::Window* container =
      ash::Shell::GetContainer(ash::Shell::GetRootWindowForNewWindows(),
                               ash::kShellWindowId_DefaultContainerDeprecated);

  // We need to create a vector that contains window_id and title.
  std::vector<lacros::mojom::WindowDetailsPtr> windows;

  // The |container| has all the top-level windows in reverse order, e.g. the
  // most top-level window is at the end. So iterate children reversely to make
  // sure |sources| is in the expected order.
  for (auto it = container->children().rbegin();
       it != container->children().rend(); ++it) {
    aura::Window* window = *it;
    // TODO(https://crbug.com/1094460): The window is currently not visible or
    // focusable. If the window later becomes invisible or unfocusable, we don't
    // bother removing the window from the list. We should handle this more
    // robustly.
    if (!window->IsVisible() || !window->CanFocus())
      continue;

    lacros::mojom::WindowDetailsPtr details =
        lacros::mojom::WindowDetails::New();

    // We are already tracking the window.
    auto existing_window_it = window_to_id_.find(window);
    if (existing_window_it != window_to_id_.end()) {
      details->id = existing_window_it->second;
      details->title = base::UTF16ToUTF8(existing_window_it->first->GetTitle());
      windows.push_back(std::move(details));
      continue;
    }

    id_to_window_[++next_window_id_] = window;
    window_to_id_[window] = next_window_id_;
    window->AddObserver(this);

    details->id = next_window_id_;
    details->title = base::UTF16ToUTF8(window->GetTitle());
    windows.push_back(std::move(details));
  }

  std::move(callback).Run(std::move(windows));
}

void ScreenManagerCrosapi::TakeWindowSnapshot(
    uint64_t id,
    TakeWindowSnapshotCallback callback) {
  auto it = id_to_window_.find(id);
  if (it == id_to_window_.end()) {
    std::move(callback).Run(/*success=*/false, lacros::WindowSnapshot());
    return;
  }

  SnapshotCallback snapshot_callback =
      base::BindOnce(std::move(callback), /*success=*/true);
  aura::Window* window = it->second;
  gfx::Rect bounds = window->bounds();
  bounds.set_x(0);
  bounds.set_y(0);
  ui::GrabWindowSnapshotAsync(
      window, bounds,
      base::BindOnce(&ScreenManagerCrosapi::DidTakeSnapshot,
                     weak_factory_.GetWeakPtr(), std::move(snapshot_callback)));
}

void ScreenManagerCrosapi::OnWindowDestroying(aura::Window* window) {
  auto it = window_to_id_.find(window);
  if (it == window_to_id_.end())
    return;
  uint64_t id = it->second;
  window_to_id_.erase(it);
  id_to_window_.erase(id);
}

void ScreenManagerCrosapi::DidTakeSnapshot(SnapshotCallback callback,
                                           gfx::Image image) {
  SkBitmap bitmap = image.AsBitmap();

  // This code currently relies on the assumption that the bitmap is unpadded,
  // and uses 4 bytes per pixel.
  int size;
  size = base::CheckMul(bitmap.width(), bitmap.height()).ValueOrDie();
  size = base::CheckMul(size, 4).ValueOrDie();
  CHECK_EQ(bitmap.computeByteSize(), base::checked_cast<size_t>(size));

  uint8_t* base = static_cast<uint8_t*>(bitmap.getPixels());
  std::vector<uint8_t> bytes(base, base + bitmap.computeByteSize());

  lacros::WindowSnapshot snapshot;
  snapshot.width = bitmap.width();
  snapshot.height = bitmap.height();
  snapshot.bitmap.swap(bytes);
  std::move(callback).Run(std::move(snapshot));
}
