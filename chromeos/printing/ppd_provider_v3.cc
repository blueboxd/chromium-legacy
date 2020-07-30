// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/queue.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chromeos/printing/ppd_cache.h"
#include "chromeos/printing/ppd_metadata_manager.h"
#include "chromeos/printing/ppd_provider_v3.h"
#include "chromeos/printing/printer_config_cache.h"
#include "chromeos/printing/printer_configuration.h"
#include "net/base/backoff_entry.h"

namespace chromeos {
namespace {

// The exact queue length at which PpdProvider will begin to post
// failure callbacks in response to its queue-able public methods.
// Arbitrarily chosen.
// See also: struct MethodDeferralContext
constexpr size_t kMethodDeferralLimit = 20;

// Backoff policy for retrying
// PpdProviderImpl::TryToGetMetadataManagerLocale(). Specifies that we
// *  perform the first retry with a 1s delay,
// *  double the retry delay thereafter, and
// *  cap the retry delay at 32s.
//
// We perform backoff to prevent the PpdProviderImpl from running at
// full sequence speed if it continuously fails to obtain a metadata
// locale.
constexpr net::BackoffEntry::Policy kBackoffPolicy{
    /*num_errors_to_ignore=*/0,
    /*initial_delay_ms=*/1000,
    /*multiply_factor=*/2.0,
    /*jitter_factor=*/0.0,
    /*maximum_backoff_ms=*/32000LL,
    /*entry_lifetime_ms=*/-1LL,
    /*always_use_initial_delay=*/true};

// Age limit for time-sensitive API calls. Typically denotes "Please
// respond with data no older than kMaxDataAge." Arbitrarily chosen.
constexpr base::TimeDelta kMaxDataAge = base::TimeDelta::FromMinutes(30LL);

// Helper struct for PpdProviderImpl. Allows PpdProviderImpl to defer
// its public method calls, which PpdProviderImpl will do when the
// PpdMetadataManager is not ready to deal with locale-sensitive PPD
// metadata.
//
// Note that the semantics of this struct demand two things of the
// deferable public methods of PpdProviderImpl:
// 1. that they check for its presence and
// 2. that they check its |current_method_is_being_failed| member to
//    prevent infinite re-enqueueing of public methods once the queue
//    is full.
struct MethodDeferralContext {
  MethodDeferralContext() : backoff_entry(&kBackoffPolicy) {}
  ~MethodDeferralContext() = default;

  // This struct is not copyable.
  MethodDeferralContext(const MethodDeferralContext&) = delete;
  MethodDeferralContext& operator=(const MethodDeferralContext&) = delete;

  // Pops the first entry from |deferred_methods| and synchronously runs
  // it with the intent to fail it.
  void FailOneEnqueuedMethod() {
    DCHECK(!current_method_is_being_failed);

    // Explicitly activates the failure codepath for whatever public
    // method of PpdProviderImpl that we'll now Run().
    current_method_is_being_failed = true;

    std::move(deferred_methods.front()).Run();
    deferred_methods.pop();
    current_method_is_being_failed = false;
  }

  // Fails all |deferred_methods| synchronously.
  void FailAllEnqueuedMethods() {
    while (!deferred_methods.empty()) {
      FailOneEnqueuedMethod();
    }
  }

  // Dequeues and posts all |deferred_methods| onto our sequence.
  void FlushAndPostAll() {
    while (!deferred_methods.empty()) {
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, std::move(deferred_methods.front()));
      deferred_methods.pop();
    }
  }

  bool IsFull() { return deferred_methods.size() >= kMethodDeferralLimit; }

  // This bool is checked during execution of a queue-able public method
  // of PpdProviderImpl. If it is true, then
  // 1. the current queue-able public method was previously enqueued,
  // 2. the deferral queue is full, and so
  // 3. the current queue-able public method was posted for the sole
  //    purpose of being _failed_, and should not be re-enqueued.
  bool current_method_is_being_failed = false;

  base::queue<base::OnceCallback<void()>> deferred_methods;

  // Implements retry backoff for
  // PpdProviderImpl::TryToGetMetadataManagerLocale().
  net::BackoffEntry backoff_entry;
};

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
        deferral_context_(std::make_unique<MethodDeferralContext>()),
        metadata_manager_(std::move(metadata_manager)),
        config_cache_(std::move(config_cache)),
        file_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
            {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
             base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {
    // Immediately attempts to obtain a metadata locale.
    TryToGetMetadataManagerLocale();
  }

  void ResolveManufacturers(ResolveManufacturersCallback cb) override {
    // Do we need
    // 1. to defer this method?
    // 2. to fail this method (which was already previously deferred)?
    if (deferral_context_) {
      if (deferral_context_->current_method_is_being_failed) {
        auto failure_cb = base::BindOnce(
            std::move(cb), PpdProvider::CallbackResultCode::SERVER_ERROR,
            std::vector<std::string>());
        base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                         std::move(failure_cb));
        return;
      }

      if (deferral_context_->IsFull()) {
        deferral_context_->FailOneEnqueuedMethod();
        DCHECK(!deferral_context_->IsFull());
      }
      base::OnceCallback<void()> this_method =
          base::BindOnce(&PpdProviderImpl::ResolveManufacturers,
                         weak_factory_.GetWeakPtr(), std::move(cb));
      deferral_context_->deferred_methods.push(std::move(this_method));
      return;
    }

    metadata_manager_->GetManufacturers(kMaxDataAge, std::move(cb));
  }

  void ResolvePrinters(const std::string& manufacturer,
                       ResolvePrintersCallback cb) override {
    // Caller must not call ResolvePrinters() before a successful reply
    // from ResolveManufacturers(). ResolveManufacturers() cannot have
    // been successful if the |deferral_context_| still exists.
    if (deferral_context_) {
      auto failure_cb = base::BindOnce(
          std::move(cb), PpdProvider::CallbackResultCode::INTERNAL_ERROR,
          ResolvedPrintersList());
      base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                       std::move(failure_cb));
      return;
    }

    PpdMetadataManager::GetPrintersCallback manager_callback =
        base::BindOnce(&PpdProviderImpl::OnPrintersGotten,
                       weak_factory_.GetWeakPtr(), std::move(cb));
    metadata_manager_->GetPrinters(manufacturer, kMaxDataAge,
                                   std::move(manager_callback));
  }

  // This method depends on
  // 1. forward indices and
  // 2. USB indices,
  // neither of which are locale-sensitive.
  void ResolvePpdReference(const PrinterSearchData& search_data,
                           ResolvePpdReferenceCallback cb) override {
    // TODO(crbug.com/888189): implement this.
  }

  // This method depends on a successful prior call to
  // ResolvePpdReference().
  void ResolvePpd(const Printer::PpdReference& reference,
                  ResolvePpdCallback cb) override {
    // TODO(crbug.com/888189): implement this.
  }

  void ReverseLookup(const std::string& effective_make_and_model,
                     ReverseLookupCallback cb) override {
    // Do we need
    // 1. to defer this method?
    // 2. to fail this method (which was already previously deferred)?
    if (deferral_context_) {
      if (deferral_context_->current_method_is_being_failed) {
        auto failure_cb = base::BindOnce(
            std::move(cb), PpdProvider::CallbackResultCode::SERVER_ERROR, "",
            "");
        base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                         std::move(failure_cb));
        return;
      }

      if (deferral_context_->IsFull()) {
        deferral_context_->FailOneEnqueuedMethod();
        DCHECK(!deferral_context_->IsFull());
      }
      base::OnceCallback<void()> this_method = base::BindOnce(
          &PpdProviderImpl::ReverseLookup, weak_factory_.GetWeakPtr(),
          effective_make_and_model, std::move(cb));
      deferral_context_->deferred_methods.push(std::move(this_method));
      return;
    }

    // TODO(crbug.com/888189): implement this.
  }

  // This method depends on forward indices, which are not
  // locale-sensitive.
  void ResolvePpdLicense(base::StringPiece effective_make_and_model,
                         ResolvePpdLicenseCallback cb) override {
    // TODO(crbug.com/888189): implement this.
  }

 protected:
  ~PpdProviderImpl() override = default;

 private:
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

  // Evaluates true if our |version_| falls within the bounds set by
  // |restrictions|.
  bool CurrentVersionSatisfiesRestrictions(const Restrictions& restrictions) {
    if ((restrictions.min_milestone.IsValid() &&
         version_ < restrictions.min_milestone) ||
        (restrictions.max_milestone.IsValid() &&
         version_ > restrictions.max_milestone)) {
      return false;
    }
    return true;
  }

  // Callback fed to PpdMetadataManager::GetLocale().
  void OnMetadataManagerLocaleGotten(bool succeeded) {
    if (!succeeded) {
      // Uh-oh, we concretely failed to get a metadata locale. We should
      // fail all outstanding deferred methods and let callers retry as
      // they see fit.
      deferral_context_->FailAllEnqueuedMethods();

      // Inform the BackoffEntry of our failure; let it adjust the
      // retry delay.
      deferral_context_->backoff_entry.InformOfRequest(false);

      base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&PpdProviderImpl::TryToGetMetadataManagerLocale,
                         weak_factory_.GetWeakPtr()),
          deferral_context_->backoff_entry.GetTimeUntilRelease());
      return;
    }
    deferral_context_->FlushAndPostAll();

    // It is no longer necessary to defer public method calls.
    deferral_context_.reset();
  }

  // Callback fed to PpdMetadataManager::GetPrinters().
  void OnPrintersGotten(ResolvePrintersCallback cb,
                        bool succeeded,
                        const ParsedPrinters& printers) {
    if (!succeeded) {
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(cb), CallbackResultCode::SERVER_ERROR,
                         ResolvedPrintersList()));
      return;
    }

    ResolvedPrintersList printers_available_to_our_version;
    for (const ParsedPrinter& printer : printers) {
      if (!printer.restrictions.has_value() ||
          CurrentVersionSatisfiesRestrictions(printer.restrictions.value())) {
        Printer::PpdReference ppd_reference;
        ppd_reference.effective_make_and_model =
            printer.effective_make_and_model;
        printers_available_to_our_version.push_back(ResolvedPpdReference{
            printer.user_visible_printer_name, ppd_reference});
      }
    }
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(cb), CallbackResultCode::SUCCESS,
                                  printers_available_to_our_version));
  }

  // Locale of the browser, as returned by
  // BrowserContext::GetApplicationLocale();
  const std::string browser_locale_;

  // Current version used to filter restricted ppds
  const base::Version version_;

  // Provides PPD storage on-device.
  scoped_refptr<PpdCache> cache_;

  // Used to
  // 1. to determine if |this| should defer locale-sensitive public
  //    method calls and
  // 2. to defer those method calls, if necessary.
  // These deferrals are only necessary before the |metadata_manager_|
  // is ready to deal with locale-sensitive PPD metadata. This member is
  // reset once deferrals are unnecessary.
  std::unique_ptr<MethodDeferralContext> deferral_context_;

  // Interacts with and controls PPD metadata.
  std::unique_ptr<PpdMetadataManager> metadata_manager_;

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
