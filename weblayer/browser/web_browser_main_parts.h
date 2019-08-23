// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_WEB_BROWSER_MAIN_PARTS_H_
#define WEBLAYER_BROWSER_WEB_BROWSER_MAIN_PARTS_H_

#include <memory>

#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "build/build_config.h"
#include "content/public/browser/browser_main_parts.h"
#include "content/public/common/main_function_params.h"

namespace weblayer {
struct WebMainParams;

class WebBrowserMainParts : public content::BrowserMainParts {
 public:
  WebBrowserMainParts(WebMainParams* weblayer_params,
                      const content::MainFunctionParams& main_function_params);
  ~WebBrowserMainParts() override;

  // BrowserMainParts overrides.
  int PreEarlyInitialization() override;
  void PreMainMessageLoopStart() override;
  void PreMainMessageLoopRun() override;
  void PreDefaultMainMessageLoopRun(base::OnceClosure quit_closure) override;

 private:
  WebMainParams* weblayer_params_;

  DISALLOW_COPY_AND_ASSIGN(WebBrowserMainParts);
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_WEB_BROWSER_MAIN_PARTS_H_
