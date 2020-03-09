// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHILD_PDF_CHILD_INIT_H_
#define CHROME_CHILD_PDF_CHILD_INIT_H_

#include "pdf/buildflags.h"

#if !BUILDFLAG(ENABLE_PDF)
#error "PDF must be enabled"
#endif

// Initializes child-process specific code for the PDF module.
void MaybeInitializeGDI();

#endif  // CHROME_CHILD_PDF_CHILD_INIT_H_
