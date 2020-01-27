// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/first_run/steps/app_list_step.h"

#include "ash/public/cpp/first_run_helper.h"
#include "chrome/browser/chromeos/first_run/first_run_controller.h"
#include "chrome/browser/chromeos/first_run/step_names.h"
#include "chrome/browser/ui/webui/chromeos/first_run/first_run_actor.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

namespace {

const int kCircleRadius = 30;

}  // namespace

namespace chromeos {
namespace first_run {

AppListStep::AppListStep(FirstRunController* controller, FirstRunActor* actor)
    : Step(kAppListStep, controller, actor) {}

bool AppListStep::DoShow() {
  // FirstRunController owns this object, so use Unretained.
  gfx::Rect screen_bounds =
      first_run_controller()->first_run_helper()->GetAppListButtonBounds();
  if (screen_bounds.IsEmpty())
    return false;

  gfx::Point center = screen_bounds.CenterPoint();
  actor()->AddRoundHole(center.x(), center.y(), kCircleRadius);
  actor()->ShowStepPointingTo(name(), center.x(), center.y(), kCircleRadius);
  return true;
}

}  // namespace first_run
}  // namespace chromeos
