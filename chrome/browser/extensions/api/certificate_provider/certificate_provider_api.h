// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_CERTIFICATE_PROVIDER_CERTIFICATE_PROVIDER_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_CERTIFICATE_PROVIDER_CERTIFICATE_PROVIDER_API_H_

#include <string>

#include "extensions/browser/extension_function.h"

namespace chromeos {
namespace certificate_provider {
struct CertificateInfo;
}
}

namespace extensions {

namespace api {
namespace certificate_provider {
// The maximum number of times per 10 minutes, extension is allowed to show PIN
// dialog again after user closed the previous one.
extern const int kMaxClosedDialogsPer10Mins;

struct CertificateInfo;
}
}

class CertificateProviderInternalReportCertificatesFunction
    : public UIThreadExtensionFunction {
 private:
  // UIThreadExtensionFunction:
  ~CertificateProviderInternalReportCertificatesFunction() override;
  ResponseAction Run() override;

  bool ParseCertificateInfo(
      const api::certificate_provider::CertificateInfo& info,
      chromeos::certificate_provider::CertificateInfo* out_info);

  DECLARE_EXTENSION_FUNCTION("certificateProviderInternal.reportCertificates",
                             CERTIFICATEPROVIDERINTERNAL_REPORTCERTIFICATES)
};

class CertificateProviderInternalReportSignatureFunction
    : public UIThreadExtensionFunction {
 private:
  // UIThreadExtensionFunction:
  ~CertificateProviderInternalReportSignatureFunction() override;
  ResponseAction Run() override;

  DECLARE_EXTENSION_FUNCTION("certificateProviderInternal.reportSignature",
                             CERTIFICATEPROVIDERINTERNAL_REPORTSIGNATURE)
};

class CertificateProviderRequestPinFunction : public UIThreadExtensionFunction {
 private:
  // UIThreadExtensionFunction:
  ~CertificateProviderRequestPinFunction() override;
  ResponseAction Run() override;
  bool ShouldSkipQuotaLimiting() const override;
  void GetQuotaLimitHeuristics(
      extensions::QuotaLimitHeuristics* heuristics) const override;

  void OnInputReceived(const std::string& value);

  DECLARE_EXTENSION_FUNCTION("certificateProvider.requestPin",
                             CERTIFICATEPROVIDER_REQUESTPIN)
};

class CertificateProviderStopPinRequestFunction
    : public UIThreadExtensionFunction {
 private:
  // UIThreadExtensionFunction:
  ~CertificateProviderStopPinRequestFunction() override;
  ResponseAction Run() override;

  void OnPinRequestStopped();

  DECLARE_EXTENSION_FUNCTION("certificateProvider.stopPinRequest",
                             CERTIFICATEPROVIDER_STOPPINREQUEST)
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_CERTIFICATE_PROVIDER_CERTIFICATE_PROVIDER_API_H_
