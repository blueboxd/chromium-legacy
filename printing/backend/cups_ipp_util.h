// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Methods for parsing IPP Printer attributes.

#ifndef PRINTING_BACKEND_CUPS_IPP_UTIL_H_
#define PRINTING_BACKEND_CUPS_IPP_UTIL_H_

#include <memory>
#include <vector>

#include "printing/backend/cups_printer.h"
#include "printing/backend/print_backend.h"
#include "printing/printing_export.h"

namespace printing {

extern const char kIppCollate[];
extern const char kIppCopies[];
extern const char kIppColor[];
extern const char kIppMedia[];
extern const char kIppDuplex[];
extern const char kIppRequestingUserName[];
extern const char kIppPin[];
extern const char kIppPinEncryption[];

extern const char kCollated[];
extern const char kUncollated[];

extern const char kPinEncryptionNone[];

// Smart ptr wrapper for CUPS ipp_t
using ScopedIppPtr = std::unique_ptr<ipp_t, void (*)(ipp_t*)>;

// Returns the default ColorModel for |printer|.
ColorModel DefaultColorModel(const CupsOptionProvider& printer);

// Returns the set of supported ColorModels for |printer|.
std::vector<ColorModel> SupportedColorModels(const CupsOptionProvider& printer);

// Returns the default paper setting for |printer|.
PrinterSemanticCapsAndDefaults::Paper DefaultPaper(
    const CupsOptionProvider& printer);

// Returns the list of papers supported by the |printer|.
PrinterSemanticCapsAndDefaults::Papers SupportedPapers(
    const CupsOptionProvider& printer);

// Retrieves the supported number of copies from |printer| and writes the
// extremities of the range into |lower_bound| and |upper_bound|.  Values are
// set to -1 if there is an error.
void CopiesRange(const CupsOptionProvider& printer,
                 int* lower_bound,
                 int* upper_bound);

// Returns true if |printer| can do collation.
bool CollateCapable(const CupsOptionProvider& printer);

// Returns true if |printer| has collation enabled by default.
bool CollateDefault(const CupsOptionProvider& printer);

// Populates the |printer_info| object with attributes retrived using IPP from
// |printer|.
PRINTING_EXPORT void CapsAndDefaultsFromPrinter(
    const CupsOptionProvider& printer,
    PrinterSemanticCapsAndDefaults* printer_info);

// Wraps |ipp| in unique_ptr with appropriate deleter
PRINTING_EXPORT ScopedIppPtr WrapIpp(ipp_t* ipp);

}  // namespace printing

#endif  // PRINTING_BACKEND_CUPS_IPP_UTIL_H_
