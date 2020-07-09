// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/lacros/screen_manager_crosapi.h"

#include <utility>
#include <vector>

#include "ash/shell.h"
#include "base/files/file_path.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "chromeos/lacros/cpp/window_snapshot.h"
#include "ui/snapshot/snapshot.h"

ScreenManagerCrosapi::ScreenManagerCrosapi() = default;

ScreenManagerCrosapi::~ScreenManagerCrosapi() = default;

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
      base::BindOnce(&ScreenManagerCrosapi::DidTakeScreenSnapshot,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ScreenManagerCrosapi::DidTakeScreenSnapshot(
    TakeScreenSnapshotCallback callback,
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
