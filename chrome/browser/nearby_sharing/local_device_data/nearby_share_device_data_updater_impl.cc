// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/local_device_data/nearby_share_device_data_updater_impl.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/nearby_sharing/client/nearby_share_client.h"
#include "chrome/browser/nearby_sharing/proto/rpc_resources.pb.h"

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class UpdaterResultCode {
  kSuccess = 0,
  kTimeout = 1,
  kHttpErrorOffline = 2,
  kHttpErrorEndpointNotFound = 3,
  kHttpErrorAuthenticationError = 4,
  kHttpErrorBadRequest = 5,
  kHttpErrorResponseMalformed = 6,
  kHttpErrorInternalServerError = 7,
  kHttpErrorUnknown = 8,
  kMaxValue = kHttpErrorUnknown
};

const char kDeviceIdPrefix[] = "users/me/devices/";
const char kDeviceNameFieldMaskPath[] = "device.display_name";
const char kContactsFieldMaskPath[] = "device.contacts";
const char kCertificatesFieldMaskPath[] = "device.public_certificates";

UpdaterResultCode RequestErrorToUpdaterResultCode(
    NearbyShareRequestError error) {
  switch (error) {
    case NearbyShareRequestError::kOffline:
      return UpdaterResultCode::kHttpErrorOffline;
    case NearbyShareRequestError::kEndpointNotFound:
      return UpdaterResultCode::kHttpErrorEndpointNotFound;
    case NearbyShareRequestError::kAuthenticationError:
      return UpdaterResultCode::kHttpErrorAuthenticationError;
    case NearbyShareRequestError::kBadRequest:
      return UpdaterResultCode::kHttpErrorBadRequest;
    case NearbyShareRequestError::kResponseMalformed:
      return UpdaterResultCode::kHttpErrorResponseMalformed;
    case NearbyShareRequestError::kInternalServerError:
      return UpdaterResultCode::kHttpErrorInternalServerError;
    case NearbyShareRequestError::kUnknown:
      return UpdaterResultCode::kHttpErrorUnknown;
  }
}

void RecordResultMetrics(UpdaterResultCode code) {
  // TODO(crbug.com/1105579): Record a histogram value for each result.
}

}  // namespace

// static
NearbyShareDeviceDataUpdaterImpl::Factory*
    NearbyShareDeviceDataUpdaterImpl::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<NearbyShareDeviceDataUpdater>
NearbyShareDeviceDataUpdaterImpl::Factory::Create(
    const std::string& device_id,
    base::TimeDelta timeout,
    NearbyShareClientFactory* client_factory) {
  if (test_factory_)
    return test_factory_->CreateInstance(device_id, timeout, client_factory);

  return base::WrapUnique(
      new NearbyShareDeviceDataUpdaterImpl(device_id, timeout, client_factory));
}

// static
void NearbyShareDeviceDataUpdaterImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

NearbyShareDeviceDataUpdaterImpl::Factory::~Factory() = default;

NearbyShareDeviceDataUpdaterImpl::NearbyShareDeviceDataUpdaterImpl(
    const std::string& device_id,
    base::TimeDelta timeout,
    NearbyShareClientFactory* client_factory)
    : NearbyShareDeviceDataUpdater(device_id),
      timeout_(timeout),
      client_factory_(client_factory) {}

NearbyShareDeviceDataUpdaterImpl::~NearbyShareDeviceDataUpdaterImpl() = default;

void NearbyShareDeviceDataUpdaterImpl::HandleNextRequest() {
  timer_.Start(FROM_HERE, timeout_,
               base::BindOnce(&NearbyShareDeviceDataUpdaterImpl::OnTimeout,
                              base::Unretained(this)));

  nearbyshare::proto::UpdateDeviceRequest request;
  request.mutable_device()->set_name(kDeviceIdPrefix + device_id_);
  if (pending_requests_.front().device_name) {
    request.mutable_device()->set_display_name(
        *pending_requests_.front().device_name);
    request.mutable_update_mask()->add_paths(kDeviceNameFieldMaskPath);
  }
  if (pending_requests_.front().contacts) {
    *request.mutable_device()->mutable_contacts() = {
        pending_requests_.front().contacts->begin(),
        pending_requests_.front().contacts->end()};
    request.mutable_update_mask()->add_paths(kContactsFieldMaskPath);
  }
  if (pending_requests_.front().certificates) {
    *request.mutable_device()->mutable_public_certificates() = {
        pending_requests_.front().certificates->begin(),
        pending_requests_.front().certificates->end()};
    request.mutable_update_mask()->add_paths(kCertificatesFieldMaskPath);
  }

  client_ = client_factory_->CreateInstance();
  client_->UpdateDevice(
      request,
      base::BindOnce(&NearbyShareDeviceDataUpdaterImpl::OnRpcSuccess,
                     base::Unretained(this)),
      base::BindOnce(&NearbyShareDeviceDataUpdaterImpl::OnRpcFailure,
                     base::Unretained(this)));
}

void NearbyShareDeviceDataUpdaterImpl::OnRpcSuccess(
    const nearbyshare::proto::UpdateDeviceResponse& response) {
  timer_.Stop();
  nearbyshare::proto::UpdateDeviceResponse response_copy(response);
  client_.reset();
  RecordResultMetrics(UpdaterResultCode::kSuccess);
  FinishAttempt(/*success=*/true, response_copy);
}

void NearbyShareDeviceDataUpdaterImpl::OnRpcFailure(
    NearbyShareRequestError error) {
  timer_.Stop();
  client_.reset();
  RecordResultMetrics(RequestErrorToUpdaterResultCode(error));
  FinishAttempt(/*success=*/false, /*response=*/base::nullopt);
}

void NearbyShareDeviceDataUpdaterImpl::OnTimeout() {
  client_.reset();
  RecordResultMetrics(UpdaterResultCode::kTimeout);
  FinishAttempt(/*success=*/false, /*response=*/base::nullopt);
}
