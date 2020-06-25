// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "chromeos/printing/fake_printer_config_cache.h"

#include "base/containers/flat_map.h"
#include "base/strings/string_piece.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "chromeos/printing/printer_config_cache.h"

namespace chromeos {

FakePrinterConfigCache::FakePrinterConfigCache() = default;
FakePrinterConfigCache::~FakePrinterConfigCache() = default;

void FakePrinterConfigCache::SetFetchResponseForTesting(
    base::StringPiece key,
    base::StringPiece value) {
  contents_.insert_or_assign(std::string(key), std::string(value));
}

void FakePrinterConfigCache::Fetch(const std::string& key,
                                   base::TimeDelta unused_expiration,
                                   PrinterConfigCache::FetchCallback cb) {
  if (contents_.contains(key)) {
    std::move(cb).Run(PrinterConfigCache::FetchResult::Success(
        key, contents_.at(key), base::Time()));
    return;
  }
  std::move(cb).Run(PrinterConfigCache::FetchResult::Failure(key));
}

void FakePrinterConfigCache::Drop(const std::string& key) {
  contents_.erase(key);
}

}  // namespace chromeos
