// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_UPDATER_PARALLEL_UNPACKER_H_
#define CHROME_BROWSER_EXTENSIONS_UPDATER_PARALLEL_UNPACKER_H_

#include <memory>
#include <set>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/extensions/updater/fetched_crx_file.h"
#include "extensions/browser/api/declarative_net_request/ruleset_checksum.h"
#include "extensions/browser/crx_file_info.h"
#include "extensions/browser/install/crx_install_error.h"
#include "extensions/browser/sandboxed_unpacker.h"
#include "extensions/browser/updater/extension_downloader_delegate.h"

namespace base {
class DictionaryValue;
}  // namespace base

class Profile;

namespace extensions {

class PendingExtensionInfo;

// Unpacks multiple extensions in parallel, and notifies |updater| when an
// extension has finished unpacking.
class ParallelUnpacker {
 public:
  // UnpackedExtension holds information about a CRX file we fetched and
  // unpacked.
  struct UnpackedExtension {
    UnpackedExtension();
    UnpackedExtension(
        FetchedCRXFile fetch_info,
        const base::FilePath& temp_dir,
        const base::FilePath& extension_root,
        std::unique_ptr<base::DictionaryValue> original_manifest,
        scoped_refptr<const Extension> extension,
        const SkBitmap& install_icon,
        declarative_net_request::RulesetChecksums ruleset_checksums);
    UnpackedExtension(UnpackedExtension&& other);
    UnpackedExtension& operator=(UnpackedExtension&&);
    ~UnpackedExtension();

    UnpackedExtension(const UnpackedExtension&) = delete;
    UnpackedExtension& operator=(const UnpackedExtension&) = delete;

    // Information about the fetched CRX file, including CRXFileInfo and a
    // callback.
    FetchedCRXFile fetch_info;

    // The fields below are the result of
    // SandboxedUnpackerClient::OnUnpackSuccess().

    // Temporary directory with results of unpacking. It should be deleted once
    // we don't need it anymore.
    base::FilePath temp_dir;
    // The path to the extension root inside of temp_dir.
    base::FilePath extension_root;
    // The parsed but unmodified version of the manifest, with no modifications
    // such as localization, etc.
    std::unique_ptr<base::DictionaryValue> original_manifest;
    // The extension that was unpacked.
    scoped_refptr<const Extension> extension;
    // The icon we will display in the installation UI, if any.
    SkBitmap install_icon;
    // Checksums for the indexed rulesets corresponding to the Declarative Net
    // Request API.
    declarative_net_request::RulesetChecksums ruleset_checksums;
  };

  class Delegate {
   public:
    virtual void OnParallelUnpackSuccess(
        UnpackedExtension unpacked_extension) = 0;
    virtual void OnParallelUnpackFailure(FetchedCRXFile fetch_info,
                                         CrxInstallError error) = 0;
  };

  // |delegate| must outlive this object.
  ParallelUnpacker(Delegate* delegate, Profile* profile);
  ~ParallelUnpacker();

  ParallelUnpacker(const ParallelUnpacker&) = delete;
  ParallelUnpacker& operator=(const ParallelUnpacker&) = delete;

  // Starts unpacking |crx_file|. Either |pending_extension_info| or |extension|
  // must be non-null. When done unpacking, calls
  // OnParallelUnpackSuccess/Failure() on this object's delegate.
  //
  // May be called multiple times in a row to unpack multiple extensions in
  // parallel.
  void Unpack(FetchedCRXFile crx_file,
              const PendingExtensionInfo* pending_extension_info,
              const Extension* extension,
              const base::FilePath& install_directory);

 private:
  // Listens for a single SandboxedUnpacker's events. Routes
  // OnUnpackSuccess/Failure() back to the ParallelUnpacker via
  // ReportSuccess/FailureOnUIThread().
  class Client : public SandboxedUnpackerClient {
   public:
    Client(base::WeakPtr<ParallelUnpacker> unpacker,
           FetchedCRXFile fetch_info,
           scoped_refptr<base::SequencedTaskRunner> io_task_runner);

    // SandboxedUnpackerClient:
    void ShouldComputeHashesForOffWebstoreExtension(
        scoped_refptr<const Extension> extension,
        base::OnceCallback<void(bool)> callback) override;
    void OnUnpackSuccess(
        const base::FilePath& temp_dir,
        const base::FilePath& extension_root,
        std::unique_ptr<base::DictionaryValue> original_manifest,
        const Extension* extension,
        const SkBitmap& install_icon,
        declarative_net_request::RulesetChecksums ruleset_checksums) override;
    void OnUnpackFailure(const CrxInstallError& error) override;

    FetchedCRXFile& fetch_info() { return fetch_info_; }

   private:
    ~Client() override;

    // To check whether we need to compute hashes or not, we have to make a
    // query to ContentVerifier, and that should be done on the UI thread.
    void ShouldComputeHashesOnUIThread(scoped_refptr<const Extension> extension,
                                       base::OnceCallback<void(bool)> callback);

    base::WeakPtr<ParallelUnpacker> unpacker_;
    FetchedCRXFile fetch_info_;
    scoped_refptr<base::SequencedTaskRunner> io_task_runner_;
  };

  void ReportSuccessOnUIThread(UnpackedExtension unpacked_extension);
  void ReportFailureOnUIThread(FetchedCRXFile fetch_info,
                               CrxInstallError error);

  Delegate* const delegate_;
  Profile* const profile_;

  base::WeakPtrFactory<ParallelUnpacker> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_UPDATER_PARALLEL_UNPACKER_H_
