// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/bio/enrollment.h"

#include "components/cbor/diagnostic_writer.h"
#include "components/cbor/writer.h"
#include "components/device_event_log/device_event_log.h"

namespace device {

// static
BioEnrollmentRequest BioEnrollmentRequest::ForGetModality() {
  BioEnrollmentRequest request;
  request.get_modality = true;
  return request;
}

// static
BioEnrollmentRequest BioEnrollmentRequest::ForGetSensorInfo() {
  BioEnrollmentRequest request;
  request.modality = BioEnrollmentModality::kFingerprint;
  request.subcommand = BioEnrollmentSubCommand::kGetFingerprintSensorInfo;
  return request;
}

// static
BioEnrollmentRequest BioEnrollmentRequest::ForEnrollBegin(
    const pin::TokenResponse& response) {
  BioEnrollmentRequest request;
  request.pin_protocol = 1;
  request.modality = BioEnrollmentModality::kFingerprint;
  request.subcommand = BioEnrollmentSubCommand::kEnrollBegin;
  request.pin_auth = response.PinAuth(
      std::vector<uint8_t>{static_cast<uint8_t>(*request.modality),
                           static_cast<uint8_t>(*request.subcommand)});
  return request;
}

// static
BioEnrollmentRequest BioEnrollmentRequest::ForEnrollNextSample(
    const pin::TokenResponse& response,
    std::vector<uint8_t> template_id) {
  BioEnrollmentRequest request;
  request.pin_protocol = 1;
  request.modality = BioEnrollmentModality::kFingerprint;
  request.subcommand = BioEnrollmentSubCommand::kEnrollCaptureNextSample;
  request.params = cbor::Value::MapValue();
  request.params->emplace(
      static_cast<int>(BioEnrollmentSubCommandParam::kTemplateId),
      cbor::Value(template_id));

  std::vector<uint8_t> pin_auth =
      *cbor::Writer::Write(cbor::Value(*request.params));
  pin_auth.insert(pin_auth.begin(), static_cast<int>(*request.subcommand));
  pin_auth.insert(pin_auth.begin(), static_cast<int>(*request.modality));

  request.pin_auth = response.PinAuth(std::move(pin_auth));

  return request;
}

// static
BioEnrollmentRequest BioEnrollmentRequest::ForCancel() {
  BioEnrollmentRequest request;
  request.modality = BioEnrollmentModality::kFingerprint;
  request.subcommand = BioEnrollmentSubCommand::kCancelCurrentEnrollment;
  return request;
}

// static
BioEnrollmentRequest BioEnrollmentRequest::ForEnumerate(
    const pin::TokenResponse& response) {
  BioEnrollmentRequest request;
  request.modality = BioEnrollmentModality::kFingerprint;
  request.subcommand = BioEnrollmentSubCommand::kEnumerateEnrollments;
  request.pin_protocol = 1;
  request.pin_auth = response.PinAuth(
      std::vector<uint8_t>{static_cast<int>(*request.modality),
                           static_cast<int>(*request.subcommand)});
  return request;
}

BioEnrollmentRequest::BioEnrollmentRequest(BioEnrollmentRequest&&) = default;
BioEnrollmentRequest& BioEnrollmentRequest::operator=(BioEnrollmentRequest&&) =
    default;
BioEnrollmentRequest::BioEnrollmentRequest() = default;
BioEnrollmentRequest::~BioEnrollmentRequest() = default;

template <typename T>
static base::Optional<T> ToBioEnrollmentEnum(uint8_t v) {
  // Check if enum-class is in range...
  if (v < static_cast<int>(T::kMin) || v > static_cast<int>(T::kMax)) {
    // ...to avoid UB.
    return base::nullopt;
  }
  return static_cast<T>(v);
}

// static
base::Optional<BioEnrollmentResponse> BioEnrollmentResponse::Parse(
    const base::Optional<cbor::Value>& cbor_response) {
  BioEnrollmentResponse response;

  if (!cbor_response || !cbor_response->is_map()) {
    return response;
  }

  const auto& response_map = cbor_response->GetMap();

  // modality
  auto it = response_map.find(
      cbor::Value(static_cast<int>(BioEnrollmentResponseKey::kModality)));
  if (it != response_map.end()) {
    if (!it->second.is_unsigned()) {
      return base::nullopt;
    }
    response.modality =
        ToBioEnrollmentEnum<BioEnrollmentModality>(it->second.GetUnsigned());
    if (!response.modality) {
      return base::nullopt;
    }
  }

  // fingerprint kind
  it = response_map.find(cbor::Value(
      static_cast<int>(BioEnrollmentResponseKey::kFingerprintKind)));
  if (it != response_map.end()) {
    if (!it->second.is_unsigned()) {
      return base::nullopt;
    }
    response.fingerprint_kind =
        ToBioEnrollmentEnum<BioEnrollmentFingerprintKind>(
            it->second.GetUnsigned());
    if (!response.fingerprint_kind) {
      return base::nullopt;
    }
  }

  // max captures required for enroll
  it = response_map.find(cbor::Value(static_cast<int>(
      BioEnrollmentResponseKey::kMaxCaptureSamplesRequiredForEnroll)));
  if (it != response_map.end()) {
    if (!it->second.is_unsigned()) {
      return base::nullopt;
    }
    response.max_samples_for_enroll = it->second.GetUnsigned();
  }

  // template id
  it = response_map.find(
      cbor::Value(static_cast<int>(BioEnrollmentResponseKey::kTemplateId)));
  if (it != response_map.end()) {
    if (!it->second.is_bytestring()) {
      return base::nullopt;
    }
    response.template_id = it->second.GetBytestring();
  }

  // last enroll sample status
  it = response_map.find(cbor::Value(
      static_cast<int>(BioEnrollmentResponseKey::kLastEnrollSampleStatus)));
  if (it != response_map.end()) {
    if (!it->second.is_unsigned()) {
      return base::nullopt;
    }
    response.last_status = ToBioEnrollmentEnum<BioEnrollmentSampleStatus>(
        it->second.GetUnsigned());
    if (!response.last_status) {
      return base::nullopt;
    }
  }

  // remaining samples
  it = response_map.find(cbor::Value(
      static_cast<int>(BioEnrollmentResponseKey::kRemainingSamples)));
  if (it != response_map.end()) {
    if (!it->second.is_unsigned()) {
      return base::nullopt;
    }
    response.remaining_samples = it->second.GetUnsigned();
  }

  // enumerated template infos
  it = response_map.find(
      cbor::Value(static_cast<int>(BioEnrollmentResponseKey::kTemplateInfos)));
  if (it != response_map.end()) {
    if (!it->second.is_array()) {
      return base::nullopt;
    }

    std::vector<std::pair<std::vector<uint8_t>, std::string>> enumerated_ids;
    for (const auto& bio_template : it->second.GetArray()) {
      if (!bio_template.is_map()) {
        return base::nullopt;
      }
      const cbor::Value::MapValue& template_map = bio_template.GetMap();

      // id (required)
      auto template_it = template_map.find(cbor::Value(
          static_cast<int>(BioEnrollmentTemplateInfoParam::kTemplateId)));
      if (template_it == template_map.end() ||
          !template_it->second.is_bytestring()) {
        return base::nullopt;
      }
      std::vector<uint8_t> id = template_it->second.GetBytestring();

      // name (optional)
      std::string name;
      template_it = template_map.find(cbor::Value(static_cast<int>(
          BioEnrollmentTemplateInfoParam::kTemplateFriendlyName)));
      if (template_it != template_map.end()) {
        if (!template_it->second.is_string()) {
          return base::nullopt;
        }
        name = template_it->second.GetString();
      }
      enumerated_ids.push_back(std::make_pair(std::move(id), std::move(name)));
    }
    response.enumerated_ids = std::move(enumerated_ids);
  }

  return response;
}

BioEnrollmentResponse::BioEnrollmentResponse() = default;
BioEnrollmentResponse::BioEnrollmentResponse(BioEnrollmentResponse&&) = default;
BioEnrollmentResponse::~BioEnrollmentResponse() = default;

bool BioEnrollmentResponse::operator==(const BioEnrollmentResponse& r) const {
  return modality == r.modality && fingerprint_kind == r.fingerprint_kind &&
         max_samples_for_enroll == r.max_samples_for_enroll &&
         template_id == r.template_id && last_status == r.last_status &&
         remaining_samples == r.remaining_samples &&
         enumerated_ids == r.enumerated_ids;
}

std::pair<CtapRequestCommand, base::Optional<cbor::Value>>
AsCTAPRequestValuePair(const BioEnrollmentRequest& request) {
  cbor::Value::MapValue map;

  using Key = BioEnrollmentRequestKey;

  if (request.modality) {
    map.emplace(static_cast<int>(Key::kModality),
                static_cast<int>(*request.modality));
  }

  if (request.subcommand) {
    map.emplace(static_cast<int>(Key::kSubCommand),
                static_cast<int>(*request.subcommand));
  }

  if (request.params) {
    map.emplace(static_cast<int>(Key::kSubCommandParams), *request.params);
  }

  if (request.pin_protocol) {
    map.emplace(static_cast<int>(Key::kPinProtocol), *request.pin_protocol);
  }

  if (request.pin_auth) {
    map.emplace(static_cast<int>(Key::kPinAuth), *request.pin_auth);
  }

  if (request.get_modality) {
    map.emplace(static_cast<int>(Key::kGetModality), *request.get_modality);
  }

  return {CtapRequestCommand::kAuthenticatorBioEnrollmentPreview,
          cbor::Value(std::move(map))};
}

}  // namespace device
