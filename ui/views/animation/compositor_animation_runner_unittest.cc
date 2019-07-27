// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/compositor_animation_runner.h"

#include "base/test/bind_test_util.h"
#include "base/timer/timer.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/test/widget_test.h"

namespace views {
namespace test {

using CompositorAnimationRunnerTest = WidgetTest;

// TODO(crbug.com/969788): Re-enable CompositorAnimationRunner with better
// ui::Compositor switching support.
#if defined(OS_CHROMEOS)
#define MAYBE_BasicCoverageTest DISABLED_BasicCoverageTest
#else
#define MAYBE_BasicCoverageTest BasicCoverageTest
#endif
TEST_F(CompositorAnimationRunnerTest, MAYBE_BasicCoverageTest) {
  WidgetAutoclosePtr widget(CreateTopLevelPlatformWidget());
  widget->Show();

  constexpr base::TimeDelta kDuration = base::TimeDelta::FromMilliseconds(100);

  AnimationDelegateViews delegate(widget->GetContentsView());
  gfx::LinearAnimation animation(
      kDuration, gfx::LinearAnimation::kDefaultFrameRate, &delegate);

  base::RepeatingTimer interval_timer;
  base::RunLoop run_loop;

  animation.Start();
  EXPECT_TRUE(animation.is_animating());
  EXPECT_TRUE(delegate.container()->has_custom_animation_runner());

  interval_timer.Start(FROM_HERE, kDuration, base::BindLambdaForTesting([&]() {
                         if (animation.is_animating())
                           return;

                         interval_timer.Stop();
                         run_loop.Quit();
                       }));

  run_loop.Run();
}

}  // namespace test
}  // namespace views
