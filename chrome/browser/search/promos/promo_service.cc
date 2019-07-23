// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/promos/promo_service.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/values.h"
#include "chrome/common/webui_url_constants.h"
#include "components/google/core/browser/google_url_tracker.h"
#include "components/google/core/common/google_util.h"
#include "content/public/browser/system_connector.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/data_decoder/public/cpp/safe_json_parser.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace {

const char kNewTabPromosApiPath[] = "/async/newtab_promos";

const char kXSSIResponsePreamble[] = ")]}'";

// Parses an update proto from |value|. Will return false if |value|
// is not of the form: {"update":{"promos":{"middle": "", "log_url": ""}}}, and
// true otherwise. Additionally |data| will be base::nullopt if at least
// the top level dictionary with "update" and "promos" keys is not present.
// Resolves the log_url against the base_url to form a valid GURL.
bool JsonToPromoData(const base::Value& value,
                     const GURL& base_url,
                     base::Optional<PromoData>& data) {
  data = base::nullopt;

  const base::DictionaryValue* dict = nullptr;
  if (!value.GetAsDictionary(&dict)) {
    DVLOG(1) << "Parse error: top-level dictionary not found";
    return false;
  }

  const base::DictionaryValue* update = nullptr;
  if (!dict->GetDictionary("update", &update)) {
    DVLOG(1) << "Parse error: no update";
    return false;
  }

  const base::DictionaryValue* promos = nullptr;
  if (!update->GetDictionary("promos", &promos)) {
    DVLOG(1) << "Parse error: no promos";
    return false;
  }

  PromoData result;
  data = result;

  std::string middle = std::string();
  if (!promos->GetString("middle", &middle)) {
    DVLOG(1) << "No middle promo";
    return false;
  }

  std::string log_url = std::string();
  if (!promos->GetString("log_url", &log_url)) {
    DVLOG(1) << "No promo log_url";
    return false;
  }

  GURL promo_log_url = base_url.Resolve(log_url);
  result.promo_html = middle;
  result.promo_log_url = promo_log_url;

  data = result;

  return true;
}

}  // namespace

PromoService::PromoService(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    GoogleURLTracker* google_url_tracker)
    : url_loader_factory_(url_loader_factory),
      google_url_tracker_(google_url_tracker) {}

PromoService::~PromoService() = default;

GURL PromoService::GetGoogleBaseUrl() const {
  GURL google_base_url = google_util::CommandLineGoogleBaseURL();
  if (!google_base_url.is_valid()) {
    google_base_url = google_url_tracker_->google_url();
  }

  return google_base_url;
}

GURL PromoService::GetApiUrl() const {
  GURL api_url = GetGoogleBaseUrl().Resolve(kNewTabPromosApiPath);

  return api_url;
}

void PromoService::Refresh() {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("promo_service", R"(
        semantics {
          sender: "Promo Service"
          description: "Downloads promos."
          trigger:
            "Displaying the new tab page on Desktop, if Google is the "
            "configured search provider."
          data: "None."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can control this feature via selecting a non-Google default "
            "search engine in Chrome settings under 'Search Engine'."
          chrome_policy {
            DefaultSearchProviderEnabled {
              policy_options {mode: MANDATORY}
              DefaultSearchProviderEnabled: false
            }
          }
        })");

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GetApiUrl();
  resource_request->allow_credentials = false;
  resource_request->request_initiator =
      url::Origin::Create(GURL(chrome::kChromeUINewTabURL));

  simple_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                    traffic_annotation);
  simple_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&PromoService::OnLoadDone, base::Unretained(this)),
      1024 * 1024);
}

void PromoService::OnLoadDone(std::unique_ptr<std::string> response_body) {
  if (!response_body) {
    // This represents network errors (i.e. the server did not provide a
    // response).
    DVLOG(1) << "Request failed with error: " << simple_loader_->NetError();
    PromoDataLoaded(Status::TRANSIENT_ERROR, base::nullopt);
    return;
  }

  std::string response;
  response.swap(*response_body);

  // The response may start with )]}'. Ignore this.
  if (base::StartsWith(response, kXSSIResponsePreamble,
                       base::CompareCase::SENSITIVE)) {
    response = response.substr(strlen(kXSSIResponsePreamble));
  }

  data_decoder::SafeJsonParser::Parse(
      content::GetSystemConnector(), response,
      base::BindOnce(&PromoService::OnJsonParsed,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&PromoService::OnJsonParseFailed,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PromoService::OnJsonParsed(base::Value value) {
  const GURL google_base_url = GetGoogleBaseUrl();
  base::Optional<PromoData> result;
  if (JsonToPromoData(value, google_base_url, result)) {
    PromoDataLoaded(Status::OK_WITH_PROMO, result);
  } else {
    if (result.has_value()) {
      PromoDataLoaded(Status::OK_WITHOUT_PROMO, result);
    } else {
      PromoDataLoaded(Status::FATAL_ERROR, result);
    }
  }
}

void PromoService::OnJsonParseFailed(const std::string& message) {
  DVLOG(1) << "Parsing JSON failed: " << message;
  PromoDataLoaded(Status::FATAL_ERROR, base::nullopt);
}

void PromoService::Shutdown() {
  for (auto& observer : observers_) {
    observer.OnPromoServiceShuttingDown();
  }

  DCHECK(!observers_.might_have_observers());
}

void PromoService::AddObserver(PromoServiceObserver* observer) {
  observers_.AddObserver(observer);
}

void PromoService::RemoveObserver(PromoServiceObserver* observer) {
  observers_.RemoveObserver(observer);
}

void PromoService::PromoDataLoaded(Status status,
                                   const base::Optional<PromoData>& data) {
  // In case of transient errors, keep our cached data (if any), but still
  // notify observers of the finished load (attempt).
  if (status != Status::TRANSIENT_ERROR) {
    promo_data_ = data;
  }
  promo_status_ = status;
  NotifyObservers();
}

void PromoService::NotifyObservers() {
  for (auto& observer : observers_) {
    observer.OnPromoDataUpdated();
  }
}

GURL PromoService::GetLoadURLForTesting() const {
  return GetApiUrl();
}
