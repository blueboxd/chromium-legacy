// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PPAPI_MIGRATION_URL_LOADER_H_
#define PDF_PPAPI_MIGRATION_URL_LOADER_H_

#include <stdint.h>

#include "base/containers/span.h"
#include "base/memory/ref_counted.h"
#include "pdf/ppapi_migration/callback.h"
#include "ppapi/cpp/url_loader.h"

namespace pp {
class InstanceHandle;
class URLRequestInfo;
class URLResponseInfo;
}  // namespace pp

namespace chrome_pdf {

// Thin wrapper around a `pp::URLLoader`. Unlike a `pp::URLLoader`, this class
// does not perform its own reference counting, but relies on `scoped_refptr`.
//
// TODO(crbug.com/1099022): Make this abstract, and add a Blink implementation.
class UrlLoader : public base::RefCounted<UrlLoader> {
 public:
  UrlLoader();
  explicit UrlLoader(pp::InstanceHandle plugin_instance);
  UrlLoader(const UrlLoader&) = delete;
  UrlLoader& operator=(const UrlLoader&) = delete;

  // Tries to grant the loader the capability to make unrestricted cross-origin
  // requests ("universal access," in `blink::SecurityOrigin` terms).
  void GrantUniversalAccess();

  // Mimic `pp::URLLoader`:
  void Open(const pp::URLRequestInfo& request_info, ResultCallback callback);
  bool GetDownloadProgress(int64_t* bytes_received,
                           int64_t* total_bytes_to_be_received) const;
  pp::URLResponseInfo GetResponseInfo() const;
  void ReadResponseBody(base::span<char> buffer, ResultCallback callback);
  void Close();

 private:
  friend class base::RefCounted<UrlLoader>;

  ~UrlLoader();

  pp::URLLoader pepper_loader_;
};

}  // namespace chrome_pdf

#endif  // PDF_PPAPI_MIGRATION_URL_LOADER_H_
