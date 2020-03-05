// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINTING_FEATURES_H_
#define PRINTING_PRINTING_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "printing/printing_export.h"

namespace printing {
namespace features {

#if defined(OS_CHROMEOS)
// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.
PRINTING_EXPORT extern const base::Feature kAdvancedPpdAttributes;
#endif

#if defined(OS_WIN)
PRINTING_EXPORT extern const base::Feature kUseXpsForPrinting;
PRINTING_EXPORT extern const base::Feature kUseXpsForPrintingFromPdf;

// Helper function to determine if there is any print path which could require
// the use of XPS print capabilities.
PRINTING_EXPORT bool IsXpsPrintCapabilityRequired();

// Helper function to determine if printing of a document from a particular
// source should be done using XPS printing API instead of with GDI.
PRINTING_EXPORT bool ShouldPrintUsingXps(bool source_is_pdf);
#endif

PRINTING_EXPORT extern const base::Feature kUseFrameAssociatedLoaderFactory;

}  // namespace features
}  // namespace printing

#endif  // PRINTING_PRINTING_FEATURES_H_
