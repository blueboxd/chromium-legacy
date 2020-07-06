// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chromeos/printing/ppd_cache.h"
#include "chromeos/printing/ppd_metadata_manager.h"
#include "chromeos/printing/ppd_provider_v3.h"
#include "chromeos/printing/printer_config_cache.h"

namespace chromeos {
namespace {

// This class implements the PpdProvider interface for the v3 metadata
// (https://crbug.com/888189).

class PpdProviderImpl : public PpdProvider {
 public:
  PpdProviderImpl(base::StringPiece browser_locale,
                  const base::Version& current_version,
                  scoped_refptr<PpdCache> cache,
                  std::unique_ptr<PpdMetadataManager> metadata_manager,
                  std::unique_ptr<PrinterConfigCache> config_cache)
      : browser_locale_(std::string(browser_locale)),
        version_(current_version),
        cache_(cache),
        metadata_manager_(std::move(metadata_manager)),
        config_cache_(std::move(config_cache)),
        file_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
            {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
             base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {
    TryToGetMetadataManagerLocale();
  }

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
  // Called when |this| is totally ready to go; dequeues
  // locale-sensitive method calls, including
  // *  ResolveManufacturers(),
  // *  ResolvePpdReference(), and
  // *  ReverseLookup().
  void FlushQueuedMethodCalls() {
    // TODO(crbug.com/888189): implement this method.
  }

  // Readies |metadata_manager_| to call methods which require a
  // successful callback from PpdMetadataManager::GetLocale().
  //
  // |this| is largely useless if its |metadata_manager_| is not ready
  // to traffick in locale-sensitive PPD metadata, so we want this
  // method to eventually succeed.
  void TryToGetMetadataManagerLocale() {
    auto callback =
        base::BindOnce(&PpdProviderImpl::OnMetadataManagerLocaleGotten,
                       weak_factory_.GetWeakPtr());
    metadata_manager_->GetLocale(std::move(callback));
  }

  // Callback fed to PpdMetadataManager::GetLocale().
  void OnMetadataManagerLocaleGotten(bool succeeded) {
    if (!succeeded) {
      TryToGetMetadataManagerLocale();
      return;
    }
    metadata_manager_has_gotten_locale_ = true;
    FlushQueuedMethodCalls();
  }

  // Locale of the browser, as returned by
  // BrowserContext::GetApplicationLocale();
  const std::string browser_locale_;

  // Current version used to filter restricted ppds
  const base::Version version_;

  // Provides PPD storage on-device.
  scoped_refptr<PpdCache> cache_;

  // Interacts with and controls PPD metadata.
  std::unique_ptr<PpdMetadataManager> metadata_manager_;

  // Denotes whether the |metadata_manager_| has successfully completed
  // a call to its GetLocale() method.
  bool metadata_manager_has_gotten_locale_ = false;

  // Fetches PPDs from the Chrome OS Printing team's serving root.
  std::unique_ptr<PrinterConfigCache> config_cache_;

  // Where to run disk operations.
  const scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  base::WeakPtrFactory<PpdProviderImpl> weak_factory_{this};
};

// Copied directly from v2 PpdProvider
// TODO(crbug.com/888189): figure out where this fits in the big picture
bool PpdReferenceIsWellFormed(const Printer::PpdReference& reference) {
  int filled_fields = 0;
  if (!reference.user_supplied_ppd_url.empty()) {
    ++filled_fields;
    GURL tmp_url(reference.user_supplied_ppd_url);
    if (!tmp_url.is_valid() || !tmp_url.SchemeIs("file")) {
      LOG(ERROR) << "Invalid url for a user-supplied ppd: "
                 << reference.user_supplied_ppd_url
                 << " (must be a file:// URL)";
      return false;
    }
  }
  if (!reference.effective_make_and_model.empty()) {
    ++filled_fields;
  }

  // All effective-make-and-model strings should be lowercased, since v2.
  // Since make-and-model strings could include non-Latin chars, only checking
  // that it excludes all upper-case chars A-Z.
  if (!std::all_of(reference.effective_make_and_model.begin(),
                   reference.effective_make_and_model.end(),
                   [](char c) -> bool { return !base::IsAsciiUpper(c); })) {
    return false;
  }
  // Should have exactly one non-empty field.
  return filled_fields == 1;
}

}  // namespace

PrinterSearchData::PrinterSearchData() = default;
PrinterSearchData::PrinterSearchData(const PrinterSearchData& other) = default;
PrinterSearchData::~PrinterSearchData() = default;

// static; copied directly from v2 PpdProvider
// TODO(crbug.com/888189): figure out where this fits in the big picture
std::string PpdProvider::PpdReferenceToCacheKey(
    const Printer::PpdReference& reference) {
  DCHECK(PpdReferenceIsWellFormed(reference));
  // The key prefixes here are arbitrary, but ensure we can't have an (unhashed)
  // collision between keys generated from different PpdReference fields.
  if (!reference.effective_make_and_model.empty()) {
    return std::string("em:") + reference.effective_make_and_model;
  } else {
    return std::string("up:") + reference.user_supplied_ppd_url;
  }
}

// static
scoped_refptr<PpdProvider> PpdProvider::Create(
    const std::string& browser_locale,
    network::mojom::URLLoaderFactory* loader_factory,
    scoped_refptr<PpdCache> ppd_cache,
    const base::Version& current_version,
    const PpdProvider::Options& options) {
  NOTREACHED();  // TODO(crbug.com/888189): deprecate this Create().
  return nullptr;
}

// free function; _not_ static
scoped_refptr<PpdProvider> CreateV3Provider(
    base::StringPiece browser_locale,
    const base::Version& current_version,
    scoped_refptr<PpdCache> cache,
    std::unique_ptr<PpdMetadataManager> metadata_manager,
    std::unique_ptr<PrinterConfigCache> config_cache) {
  return base::MakeRefCounted<PpdProviderImpl>(
      browser_locale, current_version, cache, std::move(metadata_manager),
      std::move(config_cache));
}

}  // namespace chromeos
