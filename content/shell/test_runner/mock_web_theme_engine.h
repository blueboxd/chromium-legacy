// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_TEST_RUNNER_MOCK_WEB_THEME_ENGINE_H_
#define CONTENT_SHELL_TEST_RUNNER_MOCK_WEB_THEME_ENGINE_H_

#include "build/build_config.h"
#include "third_party/blink/public/platform/web_theme_engine.h"

namespace test_runner {

class MockWebThemeEngine : public blink::WebThemeEngine {
 public:
  ~MockWebThemeEngine() override {}

#if !defined(OS_MACOSX)
  // blink::WebThemeEngine:
  blink::WebSize GetSize(blink::WebThemeEngine::Part) override;
  void Paint(cc::PaintCanvas*,
             blink::WebThemeEngine::Part,
             blink::WebThemeEngine::State,
             const blink::WebRect&,
             const blink::WebThemeEngine::ExtraParams*,
             blink::WebColorScheme) override;
#endif  // !defined(OS_MACOSX)

  blink::ForcedColors ForcedColors() const override;
  void SetForcedColors(const blink::ForcedColors forced_colors) override;

 private:
  blink::ForcedColors forced_colors_ = blink::ForcedColors::kNone;
};

}  // namespace test_runner

#endif  // CONTENT_SHELL_TEST_RUNNER_MOCK_WEB_THEME_ENGINE_H_
