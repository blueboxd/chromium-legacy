// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/updater/parallel_unpacker.h"

#include "base/task/post_task.h"
#include "base/values.h"
#include "chrome/browser/extensions/pending_extension_info.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/content_verifier.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace extensions {

ParallelUnpacker::UnpackedExtension::UnpackedExtension() = default;
ParallelUnpacker::UnpackedExtension::UnpackedExtension(
    FetchedCRXFile fetch_info,
    const base::FilePath& temp_dir,
    const base::FilePath& extension_root,
    std::unique_ptr<base::DictionaryValue> original_manifest,
    scoped_refptr<const Extension> extension,
    const SkBitmap& install_icon,
    declarative_net_request::RulesetChecksums ruleset_checksums)
    : fetch_info(std::move(fetch_info)),
      temp_dir(temp_dir),
      extension_root(extension_root),
      original_manifest(std::move(original_manifest)),
      extension(extension),
      install_icon(install_icon),
      ruleset_checksums(std::move(ruleset_checksums)) {}

ParallelUnpacker::UnpackedExtension::UnpackedExtension(UnpackedExtension&&) =
    default;

ParallelUnpacker::UnpackedExtension&
ParallelUnpacker::UnpackedExtension::operator=(UnpackedExtension&&) = default;

ParallelUnpacker::UnpackedExtension::~UnpackedExtension() = default;

ParallelUnpacker::ParallelUnpacker(Delegate* delegate, Profile* profile)
    : delegate_(delegate), profile_(profile) {}

ParallelUnpacker::~ParallelUnpacker() = default;

void ParallelUnpacker::Unpack(
    FetchedCRXFile fetch_info,
    const PendingExtensionInfo* pending_extension_info,
    const Extension* extension,
    const base::FilePath& install_directory) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(pending_extension_info || extension);

  auto install_source = pending_extension_info
                            ? pending_extension_info->install_source()
                            : extension->location();
  auto creation_flags = pending_extension_info
                            ? pending_extension_info->creation_flags()
                            : Extension::NO_FLAGS;
  auto io_task_runner = GetExtensionFileTaskRunner();
  auto client = base::MakeRefCounted<Client>(
      weak_ptr_factory_.GetWeakPtr(), std::move(fetch_info), io_task_runner);

  auto unpacker = base::MakeRefCounted<SandboxedUnpacker>(
      install_source, creation_flags, install_directory, io_task_runner,
      client.get());
  if (!io_task_runner->PostTask(
          FROM_HERE, base::BindOnce(&SandboxedUnpacker::StartWithCrx, unpacker,
                                    client->fetch_info().info))) {
    NOTREACHED();
  }
}

ParallelUnpacker::Client::Client(
    base::WeakPtr<ParallelUnpacker> unpacker,
    FetchedCRXFile fetch_info,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner)
    : unpacker_(unpacker),
      fetch_info_(std::move(fetch_info)),
      io_task_runner_(io_task_runner) {}
ParallelUnpacker::Client::~Client() = default;

void ParallelUnpacker::Client::ShouldComputeHashesForOffWebstoreExtension(
    scoped_refptr<const Extension> extension,
    base::OnceCallback<void(bool)> callback) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  base::PostTask(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&ParallelUnpacker::Client::ShouldComputeHashesOnUIThread,
                     this, extension, std::move(callback)));
}

void ParallelUnpacker::Client::ShouldComputeHashesOnUIThread(
    scoped_refptr<const Extension> extension,
    base::OnceCallback<void(bool)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!unpacker_) {
    // |ExtensionUpdater| isn't running, e.g. Stop() was called. Drop the refs
    // in |callback|.
    return;
  }
  auto* content_verifier =
      ExtensionSystem::Get(unpacker_->profile_)->content_verifier();
  bool result = content_verifier &&
                content_verifier->ShouldComputeHashesOnInstall(*extension);
  io_task_runner_->PostTask(FROM_HERE,
                            base::BindOnce(std::move(callback), result));
}

void ParallelUnpacker::Client::OnUnpackSuccess(
    const base::FilePath& temp_dir,
    const base::FilePath& extension_root,
    std::unique_ptr<base::DictionaryValue> original_manifest,
    const Extension* extension,
    const SkBitmap& install_icon,
    declarative_net_request::RulesetChecksums ruleset_checksums) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  UnpackedExtension unpacked_extension(
      std::move(fetch_info_), temp_dir, extension_root,
      std::move(original_manifest), extension, install_icon,
      std::move(ruleset_checksums));
  base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                 base::BindOnce(&ParallelUnpacker::ReportSuccessOnUIThread,
                                unpacker_, std::move(unpacked_extension)));
}

void ParallelUnpacker::Client::OnUnpackFailure(const CrxInstallError& error) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                 base::BindOnce(&ParallelUnpacker::ReportFailureOnUIThread,
                                unpacker_, std::move(fetch_info_), error));
}

void ParallelUnpacker::ReportSuccessOnUIThread(
    UnpackedExtension unpacked_extension) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  delegate_->OnParallelUnpackSuccess(std::move(unpacked_extension));
}

void ParallelUnpacker::ReportFailureOnUIThread(FetchedCRXFile fetch_info,
                                               CrxInstallError error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  delegate_->OnParallelUnpackFailure(std::move(fetch_info), std::move(error));
}

}  // namespace extensions
