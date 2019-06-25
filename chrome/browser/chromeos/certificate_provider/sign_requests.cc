// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/certificate_provider/sign_requests.h"

#include <utility>

namespace chromeos {
namespace certificate_provider {

SignRequests::Request::Request(
    const scoped_refptr<net::X509Certificate>& certificate,
    net::SSLPrivateKey::SignCallback callback)
    : certificate(certificate), callback(std::move(callback)) {}

SignRequests::Request::Request(Request&& other) = default;

SignRequests::Request::~Request() = default;

SignRequests::Request& SignRequests::Request::operator=(Request&&) = default;

SignRequests::RequestsState::RequestsState() {}

SignRequests::RequestsState::RequestsState(RequestsState&& other) = default;

SignRequests::RequestsState::~RequestsState() {}

SignRequests::SignRequests() {}

SignRequests::~SignRequests() {}

int SignRequests::AddRequest(
    const std::string& extension_id,
    const scoped_refptr<net::X509Certificate>& certificate,
    net::SSLPrivateKey::SignCallback callback) {
  RequestsState& state = extension_to_requests_[extension_id];
  const int request_id = state.next_free_id++;
  state.pending_requests.emplace(request_id,
                                 Request(certificate, std::move(callback)));
  return request_id;
}

bool SignRequests::RemoveRequest(
    const std::string& extension_id,
    int request_id,
    scoped_refptr<net::X509Certificate>* certificate,
    net::SSLPrivateKey::SignCallback* callback) {
  RequestsState& state = extension_to_requests_[extension_id];
  std::map<int, Request>& pending = state.pending_requests;
  const auto it = pending.find(request_id);
  if (it == pending.end())
    return false;
  Request& request = it->second;

  *certificate = request.certificate;
  *callback = std::move(request.callback);
  pending.erase(it);
  return true;
}

std::vector<net::SSLPrivateKey::SignCallback> SignRequests::RemoveAllRequests(
    const std::string& extension_id) {
  std::vector<net::SSLPrivateKey::SignCallback> callbacks;
  for (auto& entry : extension_to_requests_[extension_id].pending_requests) {
    callbacks.push_back(std::move(entry.second.callback));
  }
  extension_to_requests_.erase(extension_id);
  return callbacks;
}

}  // namespace certificate_provider
}  // namespace chromeos
