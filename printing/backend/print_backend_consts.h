// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_BACKEND_PRINT_BACKEND_CONSTS_H_
#define PRINTING_BACKEND_PRINT_BACKEND_CONSTS_H_

#include "printing/printing_export.h"

PRINTING_EXPORT extern const char kCUPSBlocking[];
PRINTING_EXPORT extern const char kCUPSEncryption[];
PRINTING_EXPORT extern const char kCUPSEnterprisePrinter[];
PRINTING_EXPORT extern const char kCUPSPrintServerURL[];
PRINTING_EXPORT extern const char kDriverInfoTagName[];
PRINTING_EXPORT extern const char kDriverNameTagName[];
PRINTING_EXPORT extern const char kLocationTagName[];
PRINTING_EXPORT extern const char kValueFalse[];
PRINTING_EXPORT extern const char kValueTrue[];

// CUPS destination option names.
PRINTING_EXPORT extern const char kCUPSOptDeviceUri[];
PRINTING_EXPORT extern const char kCUPSOptPrinterInfo[];
PRINTING_EXPORT extern const char kCUPSOptPrinterLocation[];
PRINTING_EXPORT extern const char kCUPSOptPrinterMakeAndModel[];
PRINTING_EXPORT extern const char kCUPSOptPrinterState[];
PRINTING_EXPORT extern const char kCUPSOptPrinterType[];

#endif  // PRINTING_BACKEND_PRINT_BACKEND_CONSTS_H_
