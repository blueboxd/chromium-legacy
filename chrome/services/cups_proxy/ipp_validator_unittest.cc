// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/cups_proxy/ipp_validator.h"

#include <cups/cups.h>

#include <set>
#include <string>
#include <utility>

#include "chrome/services/cups_proxy/fake_cups_proxy_service_delegate.h"
#include "chrome/services/cups_proxy/public/cpp/ipp_messages.h"
#include "printing/backend/cups_ipp_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cups_proxy {
namespace {

// Mojom using statements to remove chrome::mojom everywhere...
using cups_ipp_parser::mojom::IppAttributePtr;
using cups_ipp_parser::mojom::IppMessagePtr;
using cups_ipp_parser::mojom::IppRequestPtr;
using cups_ipp_parser::mojom::ValueType;

using Printer = chromeos::Printer;

std::string EncodeEndpointForPrinterId(std::string printer_id) {
  return "/printers/" + printer_id;
}

// Fake backend for CupsProxyServiceDelegate.
class FakeServiceDelegate
    : public chromeos::printing::FakeCupsProxyServiceDelegate {
 public:
  FakeServiceDelegate() = default;
  ~FakeServiceDelegate() override = default;

  void AddPrinter(const std::string& printer_id) {
    known_printers_.insert(printer_id);
  }

  base::Optional<Printer> GetPrinter(const std::string& id) override {
    if (!base::Contains(known_printers_, id)) {
      return base::nullopt;
    }

    return Printer(id);
  }

 private:
  std::set<std::string> known_printers_;
};

class IppValidatorTest : public testing::Test {
 public:
  IppValidatorTest() {
    delegate_ = std::make_unique<FakeServiceDelegate>();
    ipp_validator_ = std::make_unique<IppValidator>(delegate_->GetWeakPtr());
  }

  ~IppValidatorTest() override = default;

  base::Optional<IppRequest> RunValidateIppRequest(
      const IppRequestPtr& request) {
    return ipp_validator_->ValidateIppRequest(request.Clone());
  }

 protected:
  // Fake delegate driving the IppValidator.
  std::unique_ptr<FakeServiceDelegate> delegate_;

  // The manager being tested. This must be declared, and thus initialized,
  // after the fakes.
  std::unique_ptr<IppValidator> ipp_validator_;
};

IppAttributePtr BuildAttributePtr(std::string name,
                                  ipp_tag_t group_tag,
                                  ipp_tag_t value_tag) {
  auto ret = cups_ipp_parser::mojom::IppAttribute::New();
  ret->name = name;
  ret->group_tag = group_tag;
  ret->value_tag = value_tag;
  ret->value = cups_ipp_parser::mojom::IppAttributeValue::New();
  return ret;
}

// Returns a mojom representation of a standard IPP request.
IppRequestPtr GetBasicIppRequest() {
  IppRequestPtr ret = cups_ipp_parser::mojom::IppRequest::New();

  // Request line.
  ret->method = "POST";
  ret->endpoint = "/";
  ret->http_version = "HTTP/1.1";

  // Map of Http headers.
  ret->headers = std::vector<ipp_converter::HttpHeader>{
      {"Content-Length", "72"},
      {"Content-Type", "application/ipp"},
      {"Date", "Thu, 04 Oct 2018 20:25:59 GMT"},
      {"Host", "localhost:0"},
      {"User-Agent",
       "CUPS/2.3b1 (Linux 4.4.159-15303-g65f4b5a7b3d3; i686) IPP/2.0"}};

  // IppMessage.
  IppMessagePtr ipp_message = cups_ipp_parser::mojom::IppMessage::New();
  ipp_message->major_version = 2;
  ipp_message->minor_version = 0;
  ipp_message->operation_id = IPP_OP_CUPS_GET_DEFAULT;
  ipp_message->request_id = 1;

  // Setup each attribute.
  IppAttributePtr attr_charset = BuildAttributePtr(
      "attributes-charset", IPP_TAG_OPERATION, IPP_TAG_CHARSET);
  attr_charset->type = ValueType::STRING;
  attr_charset->value->set_strings({"utf-8"});

  IppAttributePtr attr_natlang = BuildAttributePtr(
      "attributes-natural-language", IPP_TAG_OPERATION, IPP_TAG_LANGUAGE);
  attr_natlang->type = ValueType::STRING;
  attr_natlang->value->set_strings({"en"});

  ipp_message->attributes.push_back(std::move(attr_charset));
  ipp_message->attributes.push_back(std::move(attr_natlang));
  ret->ipp = std::move(ipp_message);
  return ret;
}

// Ensure basic IPP request passes validation.
TEST_F(IppValidatorTest, SimpleSanityTest) {
  auto request = GetBasicIppRequest();
  auto validated_request = RunValidateIppRequest(request);
  EXPECT_TRUE(RunValidateIppRequest(request));
}

// Ensure printer endpoints are printers known to Chrome.
TEST_F(IppValidatorTest, EndpointIsKnownPrinter) {
  auto request = GetBasicIppRequest();
  std::string printer_id = "abc";

  request->endpoint = EncodeEndpointForPrinterId(printer_id);
  EXPECT_FALSE(RunValidateIppRequest(request));

  // Should succeed now that |delegate_| knows about the printer.
  delegate_->AddPrinter(printer_id);
  EXPECT_TRUE(RunValidateIppRequest(request));
}

TEST_F(IppValidatorTest, IncorrectHttpMethod) {
  auto request = GetBasicIppRequest();
  request->method = "GET";
  EXPECT_FALSE(RunValidateIppRequest(request));
}

TEST_F(IppValidatorTest, IncorrectHttpVersion) {
  auto request = GetBasicIppRequest();
  request->http_version = "HTTP/2.0";
  EXPECT_FALSE(RunValidateIppRequest(request));
}

TEST_F(IppValidatorTest, MissingHeaderName) {
  auto request = GetBasicIppRequest();

  // Adds new header with an empty name.
  request->headers[""] = "arbitrary valid header value";
  EXPECT_FALSE(RunValidateIppRequest(request));
}

TEST_F(IppValidatorTest, MissingHeaderValue) {
  auto request = GetBasicIppRequest();

  // Adds new header with an empty name.
  request->headers["arbitrary_valid_header_name"] = "";
  EXPECT_TRUE(RunValidateIppRequest(request));
}

// TODO(crbug.com/945409): Test IPP validation.

}  // namespace
}  // namespace cups_proxy
