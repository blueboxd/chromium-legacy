// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/certificate_provider/test_certificate_provider_extension.h"

#include <cstdint>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "chrome/common/extensions/api/certificate_provider.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "crypto/rsa_private_key.h"
#include "extensions/browser/api/test/test_api.h"
#include "extensions/browser/notification_types.h"
#include "net/cert/asn1_util.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/test/cert_test_util.h"
#include "net/test/key_util.h"
#include "net/test/test_data_directory.h"
#include "third_party/boringssl/src/include/openssl/nid.h"
#include "third_party/boringssl/src/include/openssl/rsa.h"

namespace {

// List of algorithms that the extension claims to support for the returned
// certificates.
constexpr extensions::api::certificate_provider::Hash kSupportedHashes[] = {
    extensions::api::certificate_provider::Hash::HASH_SHA256,
    extensions::api::certificate_provider::Hash::HASH_SHA1};

base::Value ConvertBytesToValue(base::span<const uint8_t> bytes) {
  base::Value value(base::Value::Type::LIST);
  for (auto byte : bytes)
    value.Append(byte);
  return value;
}

std::vector<uint8_t> ExtractBytesFromValue(const base::Value& value) {
  std::vector<uint8_t> bytes;
  for (const base::Value& item_value : value.GetList())
    bytes.push_back(base::checked_cast<uint8_t>(item_value.GetInt()));
  return bytes;
}

base::span<const uint8_t> GetCertDer(const net::X509Certificate& certificate) {
  return base::as_bytes(base::make_span(
      net::x509_util::CryptoBufferAsStringPiece(certificate.cert_buffer())));
}

base::Value MakeCertInfoValue(const net::X509Certificate& certificate) {
  base::Value cert_info_value(base::Value::Type::DICTIONARY);
  cert_info_value.SetKey("certificate",
                         ConvertBytesToValue(GetCertDer(certificate)));
  base::Value supported_hashes_value(base::Value::Type::LIST);
  for (auto supported_hash : kSupportedHashes) {
    supported_hashes_value.Append(base::Value(
        extensions::api::certificate_provider::ToString(supported_hash)));
  }
  cert_info_value.SetKey("supportedHashes", std::move(supported_hashes_value));
  return cert_info_value;
}

std::string ConvertValueToJson(const base::Value& value) {
  std::string json;
  CHECK(base::JSONWriter::Write(value, &json));
  return json;
}

base::Value ParseJsonToValue(const std::string& json) {
  base::Optional<base::Value> value = base::JSONReader::Read(json);
  CHECK(value);
  return std::move(*value);
}

bool RsaSignPrehashed(const EVP_PKEY& key,
                      int openssl_digest_type,
                      const std::vector<uint8_t>& digest,
                      std::vector<uint8_t>* signature) {
  RSA* const rsa_key = EVP_PKEY_get0_RSA(&key);
  if (!rsa_key)
    return false;
  unsigned signature_size = 0;
  signature->resize(RSA_size(rsa_key));
  if (!RSA_sign(openssl_digest_type, digest.data(), digest.size(),
                signature->data(), &signature_size, rsa_key)) {
    signature->clear();
    return false;
  }
  signature->resize(signature_size);
  return true;
}

void SendReplyToJs(extensions::TestSendMessageFunction* function,
                   const base::Value& response) {
  function->Reply(ConvertValueToJson(response));
}

}  // namespace

// static
scoped_refptr<net::X509Certificate>
TestCertificateProviderExtension::GetCertificate() {
  return net::ImportCertFromFile(net::GetTestCertsDirectory(), "client_1.pem");
}

// static
std::string TestCertificateProviderExtension::GetCertificateSpki() {
  const scoped_refptr<net::X509Certificate> certificate = GetCertificate();
  base::StringPiece spki_bytes;
  if (!net::asn1::ExtractSPKIFromDERCert(
          net::x509_util::CryptoBufferAsStringPiece(certificate->cert_buffer()),
          &spki_bytes)) {
    return {};
  }
  return spki_bytes.as_string();
}

TestCertificateProviderExtension::TestCertificateProviderExtension(
    content::BrowserContext* browser_context,
    const std::string& extension_id)
    : browser_context_(browser_context),
      extension_id_(extension_id),
      certificate_(GetCertificate()),
      private_key_(net::key_util::LoadEVP_PKEYFromPEM(
          net::GetTestCertsDirectory().Append(
              FILE_PATH_LITERAL("client_1.key")))) {
  DCHECK(browser_context_);
  DCHECK(!extension_id_.empty());
  CHECK(certificate_);
  CHECK(private_key_);
  notification_registrar_.Add(this,
                              extensions::NOTIFICATION_EXTENSION_TEST_MESSAGE,
                              content::NotificationService::AllSources());
}

TestCertificateProviderExtension::~TestCertificateProviderExtension() = default;

void TestCertificateProviderExtension::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(extensions::NOTIFICATION_EXTENSION_TEST_MESSAGE, type);

  extensions::TestSendMessageFunction* function =
      content::Source<extensions::TestSendMessageFunction>(source).ptr();
  if (!function->extension() || function->extension_id() != extension_id_ ||
      function->browser_context() != browser_context_) {
    // Ignore messages targeted to other extensions.
    return;
  }

  const auto typed_details =
      content::Details<std::pair<std::string, bool*>>(details);
  const std::string& message = typed_details->first;
  bool* const listener_will_respond = typed_details->second;

  // Handle the request and reply to it (possibly, asynchronously).
  base::Value message_value = ParseJsonToValue(message);
  CHECK(message_value.is_list());
  CHECK(message_value.GetList().size());
  CHECK(message_value.GetList()[0].is_string());
  const std::string& request_type = message_value.GetList()[0].GetString();
  ReplyToJsCallback send_reply_to_js_callback =
      base::BindOnce(&SendReplyToJs, base::Unretained(function));
  *listener_will_respond = true;
  if (request_type == "onCertificatesRequested") {
    CHECK_EQ(message_value.GetList().size(), 1U);
    HandleCertificatesRequest(std::move(send_reply_to_js_callback));
  } else if (request_type == "onSignatureRequested") {
    CHECK_EQ(message_value.GetList().size(), 4U);
    HandleSignatureRequest(
        /*sign_request=*/message_value.GetList()[1],
        /*pin_status=*/message_value.GetList()[2],
        /*pin=*/message_value.GetList()[3],
        std::move(send_reply_to_js_callback));
  } else {
    LOG(FATAL) << "Unexpected JS message type: " << request_type;
  }
}

void TestCertificateProviderExtension::HandleCertificatesRequest(
    ReplyToJsCallback callback) {
  ++certificate_request_count_;
  base::Value cert_info_values(base::Value::Type::LIST);
  if (!should_fail_certificate_requests_)
    cert_info_values.Append(MakeCertInfoValue(*certificate_));
  std::move(callback).Run(cert_info_values);
}

void TestCertificateProviderExtension::HandleSignatureRequest(
    const base::Value& sign_request,
    const base::Value& pin_status,
    const base::Value& pin,
    ReplyToJsCallback callback) {
  CHECK_EQ(*sign_request.FindKey("certificate"),
           ConvertBytesToValue(GetCertDer(*certificate_)));
  const std::string pin_status_string = pin_status.GetString();
  const std::string pin_string = pin.GetString();

  const int sign_request_id = sign_request.FindKey("signRequestId")->GetInt();
  const std::vector<uint8_t> digest =
      ExtractBytesFromValue(*sign_request.FindKey("digest"));

  const extensions::api::certificate_provider::Hash hash =
      extensions::api::certificate_provider::ParseHash(
          sign_request.FindKey("hash")->GetString());
  int openssl_digest_type = 0;
  if (hash == extensions::api::certificate_provider::Hash::HASH_SHA256)
    openssl_digest_type = NID_sha256;
  else if (hash == extensions::api::certificate_provider::Hash::HASH_SHA1)
    openssl_digest_type = NID_sha1;
  else
    LOG(FATAL) << "Unexpected signature request hash: " << hash;

  if (should_fail_sign_digest_requests_) {
    // Simulate a failure.
    std::move(callback).Run(/*response=*/base::Value());
    return;
  }

  base::Value response(base::Value::Type::DICTIONARY);
  if (required_pin_.has_value()) {
    if (pin_status_string == "not_requested") {
      // The PIN is required but not specified yet, so request it via the JS
      // side before generating the signature.
      base::Value pin_request_parameters(base::Value::Type::DICTIONARY);
      pin_request_parameters.SetIntKey("signRequestId", sign_request_id);
      response.SetKey("requestPin", std::move(pin_request_parameters));
      std::move(callback).Run(response);
      return;
    }
    if (pin_status_string == "canceled" ||
        base::StartsWith(pin_status_string,
                         "failed:", base::CompareCase::SENSITIVE)) {
      // The PIN request failed.
      LOG(WARNING) << "PIN request failed: " << pin_status_string;
      // Respond with a failure.
      std::move(callback).Run(/*response=*/base::Value());
      return;
    }
    DCHECK_EQ(pin_status_string, "ok");
    if (pin_string != *required_pin_) {
      // The PIN is wrong, so retry the PIN request with displaying an error.
      base::Value pin_request_parameters(base::Value::Type::DICTIONARY);
      pin_request_parameters.SetIntKey("signRequestId", sign_request_id);
      pin_request_parameters.SetStringKey("errorType", "INVALID_PIN");
      response.SetKey("requestPin", std::move(pin_request_parameters));
      std::move(callback).Run(response);
      return;
    }
    // The entered PIN is correct. Stop the PIN request and proceed to
    // generating the signature.
    base::Value stop_pin_request_parameters(base::Value::Type::DICTIONARY);
    stop_pin_request_parameters.SetIntKey("signRequestId", sign_request_id);
    response.SetKey("stopPinRequest", std::move(stop_pin_request_parameters));
  }
  // Generate and return a valid signature.
  std::vector<uint8_t> signature;
  CHECK(
      RsaSignPrehashed(*private_key_, openssl_digest_type, digest, &signature));
  response.SetKey("signature", ConvertBytesToValue(signature));
  std::move(callback).Run(response);
}
