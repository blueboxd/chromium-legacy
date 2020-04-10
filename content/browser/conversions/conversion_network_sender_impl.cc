// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/conversions/conversion_network_sender_impl.h"

#include "base/bind.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "content/public/browser/storage_partition.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_canon.h"

namespace content {

namespace {

GURL GetReportUrl(const content::ConversionReport& report) {
  url::Replacements<char> replacements;
  const char kEndpointPath[] = "/.well-known/register-conversion";
  replacements.SetPath(kEndpointPath, url::Component(0, strlen(kEndpointPath)));
  std::string query = base::StrCat(
      {"impression-data=", report.impression.impression_data(),
       "&conversion-data=", report.conversion_data,
       "&credit=", base::NumberToString(report.attribution_credit)});
  replacements.SetQuery(query.c_str(), url::Component(0, query.length()));
  return report.impression.reporting_origin().GetURL().ReplaceComponents(
      replacements);
}

}  // namespace

ConversionNetworkSenderImpl::ConversionNetworkSenderImpl(
    StoragePartition* storage_partition)
    : storage_partition_(storage_partition) {}

ConversionNetworkSenderImpl::~ConversionNetworkSenderImpl() = default;

void ConversionNetworkSenderImpl::SendReport(ConversionReport* report,
                                             ReportSentCallback sent_callback) {
  // The browser process URLLoaderFactory is not created by default, so don't
  // create it until it is directly needed.
  if (!url_loader_factory_) {
    url_loader_factory_ =
        storage_partition_->GetURLLoaderFactoryForBrowserProcess();
  }

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GetReportUrl(*report);
  resource_request->referrer = report->impression.conversion_origin().GetURL();
  resource_request->method = "POST";
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  // TODO(https://crbug.com/1058018): Update the "policy" field in the traffic
  // annotation when a setting to disable the API is properly
  // surfaced/implemented.
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("conversion_measurement_report", R"(
        semantics {
          sender: "Event-level Conversion Measurement API"
          description:
            "The Conversion Measurement API allows sites to measure "
            "conversions (e.g. purchases) and attribute them to clicked ads, "
            "without using cross-site persistent identifiers like third party "
            "cookies."
          trigger:
            "When a registered conversion has become eligible for reporting."
          data:
            "A high-entropy identifier declared by the site in which the user "
            "clicked on an impression. A noisy low entropy data value declared "
            "on the conversion site. A browser generated value that denotes "
            "if this was the last impression clicked prior to conversion."
          destination:OTHER
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature cannot be disabled by settings."
          policy_exception_justification: "Not implemented."
        })");

  auto simple_url_loader = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  network::SimpleURLLoader* simple_url_loader_ptr = simple_url_loader.get();

  auto it = loaders_in_progress_.insert(loaders_in_progress_.begin(),
                                        std::move(simple_url_loader));
  simple_url_loader_ptr->SetTimeoutDuration(base::TimeDelta::FromSeconds(30));

  // Unretained is safe because the URLLoader is owned by |this| and will be
  // deleted before |this|.
  simple_url_loader_ptr->DownloadHeadersOnly(
      url_loader_factory_.get(),
      base::BindOnce(&ConversionNetworkSenderImpl::OnReportSent,
                     base::Unretained(this), std::move(it),
                     std::move(sent_callback)));
}

void ConversionNetworkSenderImpl::SetURLLoaderFactoryForTesting(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  url_loader_factory_ = url_loader_factory;
}

void ConversionNetworkSenderImpl::OnReportSent(
    UrlLoaderList::iterator it,
    ReportSentCallback sent_callback,
    scoped_refptr<net::HttpResponseHeaders> headers) {
  // TODO(https://crbug.com/1054127): Log metrics for success/failure of sending
  // reports. This should inspect the HTTP response code from the headers HTTP
  // failures and SimpleUrlLoader::NetError() for internal errors/timeouts.
  loaders_in_progress_.erase(it);
  std::move(sent_callback).Run();
}

}  // namespace content
