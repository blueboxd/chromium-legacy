// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ppapi_migration/url_loader.h"

#include <stdint.h>

#include <utility>

#include "base/callback.h"
#include "pdf/ppapi_migration/callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/trusted/ppb_url_loader_trusted.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/url_loader.h"
#include "ppapi/cpp/url_request_info.h"
#include "ppapi/cpp/url_response_info.h"

namespace chrome_pdf {

UrlLoader::UrlLoader() = default;

UrlLoader::UrlLoader(pp::InstanceHandle plugin_instance)
    : pepper_loader_(plugin_instance) {}

UrlLoader::~UrlLoader() = default;

void UrlLoader::GrantUniversalAccess() {
  const PPB_URLLoaderTrusted* trusted_interface =
      static_cast<const PPB_URLLoaderTrusted*>(
          pp::Module::Get()->GetBrowserInterface(
              PPB_URLLOADERTRUSTED_INTERFACE));
  if (trusted_interface)
    trusted_interface->GrantUniversalAccess(pepper_loader_.pp_resource());
}

void UrlLoader::Open(const pp::URLRequestInfo& request_info,
                     ResultCallback callback) {
  pp::CompletionCallback pp_callback =
      PPCompletionCallbackFromResultCallback(std::move(callback));
  int32_t result = pepper_loader_.Open(request_info, pp_callback);
  if (result != PP_OK_COMPLETIONPENDING)
    pp_callback.Run(result);
}

bool UrlLoader::GetDownloadProgress(int64_t* bytes_received,
                                    int64_t* total_bytes_to_be_received) const {
  return pepper_loader_.GetDownloadProgress(bytes_received,
                                            total_bytes_to_be_received);
}

pp::URLResponseInfo UrlLoader::GetResponseInfo() const {
  return pepper_loader_.GetResponseInfo();
}

void UrlLoader::ReadResponseBody(base::span<char> buffer,
                                 ResultCallback callback) {
  pp::CompletionCallback pp_callback =
      PPCompletionCallbackFromResultCallback(std::move(callback));
  int32_t result = pepper_loader_.ReadResponseBody(buffer.data(), buffer.size(),
                                                   pp_callback);
  if (result != PP_OK_COMPLETIONPENDING)
    pp_callback.Run(result);
}

void UrlLoader::Close() {
  pepper_loader_.Close();
}

}  // namespace chrome_pdf
