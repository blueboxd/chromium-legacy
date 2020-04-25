// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/mac/scoped_current_nsappearance.h"

#import <Cocoa/Cocoa.h>

namespace ui {
ScopedCurrentNSAppearance::ScopedCurrentNSAppearance(bool dark) {
  if (@available(macOS 10.14, *)) {
    NSAppearanceName appearance =
        dark ? NSAppearanceNameDarkAqua : NSAppearanceNameAqua;
    [NSAppearance
        setCurrentAppearance:[NSAppearance appearanceNamed:appearance]];
  }
}

ScopedCurrentNSAppearance::~ScopedCurrentNSAppearance() {
  if (@available(macOS 10.14, *))
    [NSAppearance setCurrentAppearance:nil];
}
}