// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_CONTENT_VERIFIER_DELEGATE_H_
#define EXTENSIONS_BROWSER_CONTENT_VERIFIER_DELEGATE_H_

#include <set>

#include "extensions/browser/content_verifier/content_verifier_key.h"
#include "extensions/browser/content_verify_job.h"
#include "url/gurl.h"

namespace base {
class FilePath;
class Version;
}

namespace extensions {

class Extension;

// This is an interface for clients that want to use a ContentVerifier.
class ContentVerifierDelegate {
 public:
  virtual ~ContentVerifierDelegate() {}

  // Returns true if resources from |extension| should be checked for some
  // content mismatch at all. Note that differs from ShouldBeVerified, and does
  // not consider whether |extension| has signed hashes (verified_contents.json)
  // or not.
  virtual bool ShouldBeChecked(const Extension& extension) = 0;

  // Returns whether or not resources from |extension| should be verified using
  // signed hashes data (verified_contents.json). If yes, methods GetPublicKey
  // and GetSignatureFetchUrl might be used.
  virtual bool ShouldBeVerified(const Extension& extension) = 0;

  // Returns the public key to use for validating signatures.
  virtual ContentVerifierKey GetPublicKey() = 0;

  // Returns a URL that can be used to fetch the verified_contents.json
  // containing signatures for the given extension id/version pair.
  virtual GURL GetSignatureFetchUrl(const std::string& extension_id,
                                    const base::Version& version) = 0;

  // Returns the set of file paths for images used within the browser process.
  // (These may get transcoded during the install process).
  virtual std::set<base::FilePath> GetBrowserImagePaths(
      const extensions::Extension* extension) = 0;

  // Called when the content verifier detects that a read of a file inside an
  // extension did not match its expected hash.
  virtual void VerifyFailed(const std::string& extension_id,
                            ContentVerifyJob::FailureReason reason) = 0;

  // Called when ExtensionSystem is shutting down.
  virtual void Shutdown() = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_CONTENT_VERIFIER_DELEGATE_H_
