// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_init.h"

namespace chrome_pdf {

namespace {

bool g_sdk_initialized_via_pepper = false;

}  // namespace

bool IsSDKInitializedViaPepper() {
  return g_sdk_initialized_via_pepper;
}

void SetIsSDKInitializedViaPepper(bool initialized_via_pepper) {
  g_sdk_initialized_via_pepper = initialized_via_pepper;
}

}  // namespace chrome_pdf
