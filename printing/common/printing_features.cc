// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/common/printing_features.h"

namespace printing {
namespace features {

#if defined(OS_WIN)
// Use XPS for printing instead of GDI.
const base::Feature kUseXpsForPrinting{"UseXpsForPrinting",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// Use XPS for printing instead of GDI for printing PDF documents.   This is
// independent of |kUseXpsForPrinting|; can use XPS for PDFs even if still using
// GDI for modifiable content.
const base::Feature kUseXpsForPrintingFromPdf{
    "UseXpsForPrintingFromPdf", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

}  // namespace features
}  // namespace printing
