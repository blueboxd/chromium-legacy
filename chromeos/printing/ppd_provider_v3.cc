// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chromeos/printing/ppd_provider.h"

namespace chromeos {
namespace {

// This class implements the PpdProvider interface for the v3 metadata
// (https://crbug.com/888189).

class PpdProviderImpl : public PpdProvider {
 public:
  PpdProviderImpl(const std::string& browser_locale,
                  const base::Version& current_version,
                  const PpdProvider::Options& options)
      : browser_locale_(browser_locale),
        file_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
            {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
             base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
        version_(current_version),
        options_(options) {}

  void ResolveManufacturers(ResolveManufacturersCallback cb) override {
    // TODO(crbug.com/888189): implement this.
  }

  void ResolvePrinters(const std::string& manufacturer,
                       ResolvePrintersCallback cb) override {
    // TODO(crbug.com/888189): implement this.
  }

  void ResolvePpdReference(const PrinterSearchData& search_data,
                           ResolvePpdReferenceCallback cb) override {
    // TODO(crbug.com/888189): implement this.
  }

  void ResolvePpd(const Printer::PpdReference& reference,
                  ResolvePpdCallback cb) override {
    // TODO(crbug.com/888189): implement this.
  }

  void ReverseLookup(const std::string& effective_make_and_model,
                     ReverseLookupCallback cb) override {
    // TODO(crbug.com/888189): implement this.
  }

  void ResolvePpdLicense(base::StringPiece effective_make_and_model,
                         ResolvePpdLicenseCallback cb) override {
    // TODO(crbug.com/888189): implement this.
  }

 protected:
  ~PpdProviderImpl() override = default;

 private:
  // Locale of the browser, as returned by
  // BrowserContext::GetApplicationLocale();
  const std::string browser_locale_;

  // Where to run disk operations.
  const scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  // Current version used to filter restricted ppds
  const base::Version version_;

  // Construction-time options, immutable.
  const PpdProvider::Options options_;
};

}  // namespace

// TODO(crbug.com/888189): inline this into the header file.
PrinterSearchData::PrinterSearchData() = default;
PrinterSearchData::PrinterSearchData(const PrinterSearchData& other) = default;
PrinterSearchData::~PrinterSearchData() = default;

// static
scoped_refptr<PpdProvider> PpdProvider::Create(
    const std::string& browser_locale,
    network::mojom::URLLoaderFactory* loader_factory,
    scoped_refptr<PpdCache> ppd_cache,
    const base::Version& current_version,
    const PpdProvider::Options& options) {
  // TODO(crbug.com/888189): use |loader_factory| and do away with
  // |ppd_cache|.
  return base::MakeRefCounted<PpdProviderImpl>(browser_locale, current_version,
                                               options);
}

}  // namespace chromeos
