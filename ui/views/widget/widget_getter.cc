// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/widget_getter.h"

namespace views {

Widget* WidgetGetter::GetWidget() {
  return const_cast<Widget*>(GetWidgetImpl());
}

const Widget* WidgetGetter::GetWidget() const {
  return GetWidgetImpl();
}

}  // namespace views
