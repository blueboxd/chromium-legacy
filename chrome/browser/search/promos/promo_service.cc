// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/promos/promo_service.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/values.h"
#include "chrome/browser/search/ntp_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "components/google/core/common/google_util.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/system_connector.h"
#include "net/base/load_flags.h"
#include "net/base/url_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/data_decoder/public/cpp/safe_json_parser.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace {

const char kNewTabPromosApiPath[] = "/async/newtab_promos";

const char kXSSIResponsePreamble[] = ")]}'";

bool CanBlockPromos() {
  return base::FeatureList::IsEnabled(features::kDismissNtpPromos);
}

GURL GetGoogleBaseUrl() {
  GURL google_base_url = google_util::CommandLineGoogleBaseURL();
  if (!google_base_url.is_valid()) {
    google_base_url = GURL(google_util::kGoogleHomepageURL);
  }
  return google_base_url;
}

GURL GetApiUrl() {
  return GetGoogleBaseUrl().Resolve(kNewTabPromosApiPath);
}

// Parses an update proto from |value|. Will return false if |value| is not of
// the form: {"update":{"promos":{"middle": ""}}}, and true otherwise.
// Additionally, there can be a "log_url" or "id" field in the promo. Those are
// populated if found. They're not set for emergency promos. |data| will never
// be base::nullopt if top level dictionary keys of "update" and "promos" are
// present. Note: the "log_url" (if found), is resolved against
// GetGoogleBaseUrl() to form a valid GURL.
bool JsonToPromoData(const base::Value& value,
                     base::Optional<PromoData>* data) {
  *data = base::nullopt;

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
  *data = result;

  std::string middle;
  if (!promos->GetString("middle", &middle)) {
    DVLOG(1) << "No middle promo";
    return false;
  }

  std::string log_url;
  // Emergency promos don't have these, so it's OK if this key is missing.
  promos->GetString("log_url", &log_url);

  GURL promo_log_url;
  if (!log_url.empty())
    promo_log_url = GetGoogleBaseUrl().Resolve(log_url);

  std::string promo_id;
  if (CanBlockPromos()) {
    if (!promos->GetString("id", &promo_id))
      net::GetValueForKeyInQuery(promo_log_url, "id", &promo_id);
  }
  // Emergency promos may not have IDs, which is OK. They also can't be
  // dismissed (because of this).

  result.promo_html = middle;
  result.promo_log_url = promo_log_url;
  result.promo_id = promo_id;

  *data = result;

  return true;
}

}  // namespace

PromoService::PromoService(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    PrefService* pref_service)
    : url_loader_factory_(url_loader_factory), pref_service_(pref_service) {}

PromoService::~PromoService() = default;

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
  base::Optional<PromoData> result;
  PromoService::Status status;

  if (JsonToPromoData(value, &result)) {
    bool is_blocked = IsBlocked(result->promo_id);
    if (is_blocked)
      result = PromoData();
    status = is_blocked ? Status::OK_BUT_BLOCKED : Status::OK_WITH_PROMO;
  } else {
    status = result ? Status::OK_WITHOUT_PROMO : Status::FATAL_ERROR;
  }

  PromoDataLoaded(status, result);
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

// static
void PromoService::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kNtpPromoBlocklist);
}

void PromoService::AddObserver(PromoServiceObserver* observer) {
  observers_.AddObserver(observer);
}

void PromoService::RemoveObserver(PromoServiceObserver* observer) {
  observers_.RemoveObserver(observer);
}

void PromoService::BlocklistPromo(const std::string& promo_id) {
  if (!CanBlockPromos() || promo_id.empty() || IsBlocked(promo_id))
    return;

  ListPrefUpdate update(pref_service_, prefs::kNtpPromoBlocklist);
  update->Append(promo_id);

  if (promo_data_ && promo_data_->promo_id == promo_id) {
    promo_data_ = PromoData();
    promo_status_ = Status::OK_BUT_BLOCKED;
    NotifyObservers();
    // TODO(crbug.com/1003508): hide promos on existing, already-opened NTPs.
  }
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

bool PromoService::IsBlocked(const std::string& promo_id) const {
  if (promo_id.empty() || !CanBlockPromos())
    return false;

  const auto* blocklist = pref_service_->GetList(prefs::kNtpPromoBlocklist);
  for (const auto& blocked : blocklist->GetList()) {
    if (blocked.GetString() == promo_id)
      return true;
  }
  return false;
}

GURL PromoService::GetLoadURLForTesting() const {
  return GetApiUrl();
}
