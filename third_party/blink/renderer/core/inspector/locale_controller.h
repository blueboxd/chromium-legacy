// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_LOCALE_CONTROLLER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_LOCALE_CONTROLLER_H_

#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class LocaleController {
 public:
  static LocaleController& instance();

  String SetLocaleOverride(const String& locale);
  bool has_locale_override() const;

 private:
  LocaleController();
  ~LocaleController() = default;

  String embedder_locale_;
  String locale_override_;
  DISALLOW_COPY_AND_ASSIGN(LocaleController);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_LOCALE_CONTROLLER_H_
