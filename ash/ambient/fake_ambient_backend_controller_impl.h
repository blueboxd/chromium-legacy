// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_FAKE_AMBIENT_BACKEND_CONTROLLER_IMPL_H_
#define ASH_AMBIENT_FAKE_AMBIENT_BACKEND_CONTROLLER_IMPL_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/ambient/ambient_backend_controller.h"
#include "base/memory/weak_ptr.h"

namespace ash {

// A dummy implementation of AmbientBackendController.
class ASH_EXPORT FakeAmbientBackendControllerImpl
    : public AmbientBackendController {
 public:
  FakeAmbientBackendControllerImpl();
  ~FakeAmbientBackendControllerImpl() override;

  // AmbientBackendController:
  void FetchScreenUpdateInfo(
      OnScreenUpdateInfoFetchedCallback callback) override;
  void GetSettings(GetSettingsCallback callback) override;
  void UpdateSettings(AmbientModeTopicSource topic_source,
                      UpdateSettingsCallback callback) override;
  void SetPhotoRefreshInterval(base::TimeDelta interval) override;

 private:
  base::WeakPtrFactory<FakeAmbientBackendControllerImpl> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_AMBIENT_FAKE_AMBIENT_BACKEND_CONTROLLER_IMPL_H_
