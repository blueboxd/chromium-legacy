// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_CUPS_IPP_PARSER_CUPS_IPP_PARSER_SERVICE_H_
#define CHROME_SERVICES_CUPS_IPP_PARSER_CUPS_IPP_PARSER_SERVICE_H_

#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/cpp/service_keepalive.h"
#include "services/service_manager/public/mojom/service.mojom.h"

namespace cups_ipp_parser {

// CupsIppParser Service Implementation.
//
// This service's sole purpose is parsing CUPS IPP printing requests. It accepts
// arbitrary byte buffers as input and returns a fully-parsed IPP request,
// mojom::IppRequest, as a result.
//
// Because this service doesn't know the origin of these requests, it
// treats each request as potentially malicious. As such, this service runs
// out-of-process, lessening the chance of exposing an exploit to the rest of
// Chrome.
//
// Note: In practice, this service is used to support printing requests incoming
// from ChromeOS.
class CupsIppParserService : public service_manager::Service {
 public:
  explicit CupsIppParserService(service_manager::mojom::ServiceRequest request);
  ~CupsIppParserService() override;

  // Lifecycle events that occur after the service has started to spinup.
  void OnStart() override;
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

 private:
  service_manager::ServiceBinding service_binding_;
  service_manager::ServiceKeepalive service_keepalive_;
  service_manager::BinderRegistry registry_;

  DISALLOW_COPY_AND_ASSIGN(CupsIppParserService);
};

}  // namespace cups_ipp_parser

#endif  // CHROME_SERVICES_CUPS_IPP_PARSER_CUPS_IPP_PARSER_SERVICE_H_
