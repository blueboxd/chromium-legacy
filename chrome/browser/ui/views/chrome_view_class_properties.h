// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CHROME_VIEW_CLASS_PROPERTIES_H_
#define CHROME_BROWSER_UI_VIEWS_CHROME_VIEW_CLASS_PROPERTIES_H_

#include "ui/base/class_property.h"

// Set to true for a View if an in-product help promo is showing for it.
// The View can respond however is appropriate, i.e. with a highlight or
// a color change.
extern const ui::ClassProperty<bool>* const kHasInProductHelpPromoKey;

#endif  // CHROME_BROWSER_UI_VIEWS_CHROME_VIEW_CLASS_PROPERTIES_H_
