// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/native_io/native_io_context.h"

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "content/browser/native_io/native_io_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "storage/common/database/database_identifier.h"
#include "third_party/blink/public/mojom/native_io/native_io.mojom.h"
#include "url/origin.h"

namespace content {

namespace {

constexpr base::FilePath::CharType kNativeIODirectoryName[] =
    FILE_PATH_LITERAL("NativeIO");

base::FilePath GetNativeIORootPath(const base::FilePath& profile_root) {
  if (profile_root.empty())
    return base::FilePath();

  return profile_root.Append(kNativeIODirectoryName);
}

}  // namespace

NativeIOContext::NativeIOContext(const base::FilePath& profile_root)
    : root_path_(GetNativeIORootPath(profile_root)) {}

NativeIOContext::~NativeIOContext() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void NativeIOContext::BindReceiver(
    const url::Origin& origin,
    mojo::PendingReceiver<blink::mojom::NativeIOHost> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = hosts_.find(origin);
  if (it == hosts_.end()) {
    base::FilePath origin_root_path = RootPathForOrigin(origin);
    if (origin_root_path.empty()) {
      // NativeIO is not supported for the origin.
      return;
    }

    DCHECK(root_path_.IsParent(origin_root_path))
        << "Per-origin data should be in a sub-directory of NativeIO/";

    bool insert_succeeded;
    std::tie(it, insert_succeeded) =
        hosts_.insert({origin, std::make_unique<NativeIOHost>(
                                   this, origin, std::move(origin_root_path))});
  }

  it->second->BindReceiver(std::move(receiver));
}

void NativeIOContext::OnHostReceiverDisconnect(NativeIOHost* host) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(host != nullptr);
  DCHECK(hosts_.count(host->origin()) > 0);
  DCHECK_EQ(hosts_[host->origin()].get(), host);

  if (!host->has_empty_receiver_set())
    return;

  hosts_.erase(host->origin());
}

base::FilePath NativeIOContext::RootPathForOrigin(const url::Origin& origin) {
  // TODO(pwnall): Implement in-memory files instead of bouncing in incognito.
  if (root_path_.empty())
    return root_path_;

  // This feature is only exposed to secure origins. This typically means https.
  // The most notable exception is http://localhost. Command-line flags may
  // cause other http origins to be considered secure.
  //
  // TODO(pwnall): Get consensus on the schemes we want to support. For example,
  //               maybe chrome-extension:// should get access as well?
  if (!origin.GetURL().SchemeIsHTTPOrHTTPS())
    return base::FilePath();

  std::string origin_identifier = storage::GetIdentifierFromOrigin(origin);
  base::FilePath origin_path = root_path_.AppendASCII(origin_identifier);
  DCHECK(root_path_.IsParent(origin_path));
  return origin_path;
}

}  // namespace content
