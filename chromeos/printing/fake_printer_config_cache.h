// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PRINTING_FAKE_PRINTER_CONFIG_CACHE_H_
#define CHROMEOS_PRINTING_FAKE_PRINTER_CONFIG_CACHE_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/strings/string_piece.h"
#include "base/time/clock.h"
#include "chromeos/printing/printer_config_cache.h"

namespace chromeos {

// A FakePrinterConfigCache provides canned responses like a real
// PrinterConfigCache would for testing purposes.
//
// This class doesn't meaningfully populate
// PrinterConfigCache::FetchResult::time_of_fetch.
class CHROMEOS_EXPORT FakePrinterConfigCache : public PrinterConfigCache {
 public:
  FakePrinterConfigCache();
  ~FakePrinterConfigCache() override;

  // Calls |cb| with a canned response for |key| previously provided by
  // SetFetchResponseForTesting().
  void Fetch(const std::string& key,
             base::TimeDelta unused_expiration,
             PrinterConfigCache::FetchCallback cb) override;

  // Causes subsequent Fetch() calls for |key| to fail (until a future
  // SetFetchResponseForTesting() provides a new canned response).
  void Drop(const std::string& key) override;

  // Sets internal state of |this| s.t. future Fetch() calls for
  // |key| get called back with |value|.
  // Subsequent calls to this method override the canned |value|.
  void SetFetchResponseForTesting(base::StringPiece key,
                                  base::StringPiece value);

 private:
  base::flat_map<std::string, std::string> contents_;
};

}  // namespace chromeos

#endif  // CHROMEOS_PRINTING_FAKE_PRINTER_CONFIG_CACHE_H_
