// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/cups_ipp_parser/cups_ipp_parser_service.h"

#include "base/bind.h"
#include "base/logging.h"
#include "chrome/services/cups_ipp_parser/ipp_parser.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace cups_ipp_parser {
namespace {

void OnIppParserRequest(service_manager::ServiceKeepalive* keepalive,
                        mojom::IppParserRequest request) {
  mojo::MakeStrongBinding(std::make_unique<IppParser>(keepalive->CreateRef()),
                          std::move(request));
}

}  // namespace

CupsIppParserService::CupsIppParserService(
    service_manager::mojom::ServiceRequest request)
    : service_binding_(this, std::move(request)),
      service_keepalive_(&service_binding_, base::TimeDelta()) {}

CupsIppParserService::~CupsIppParserService() = default;

void CupsIppParserService::OnStart() {
  registry_.AddInterface(
      base::BindRepeating(&OnIppParserRequest, &service_keepalive_));

  DVLOG(1) << "CupsIppParserService started.";
}

void CupsIppParserService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe));
}

}  // namespace cups_ipp_parser
