// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service.h"

#include "base/compiler_specific.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

NavigationPredictorKeyedService::Prediction::Prediction(
    const content::WebContents* web_contents,
    const GURL& source_document_url,
    const std::vector<GURL>& sorted_predicted_urls)
    : web_contents_(web_contents),
      source_document_url_(source_document_url),
      sorted_predicted_urls_(sorted_predicted_urls) {}

NavigationPredictorKeyedService::Prediction::Prediction(
    const NavigationPredictorKeyedService::Prediction& other) = default;

NavigationPredictorKeyedService::Prediction&
NavigationPredictorKeyedService::Prediction::operator=(
    const NavigationPredictorKeyedService::Prediction& other) = default;

NavigationPredictorKeyedService::Prediction::~Prediction() = default;

GURL NavigationPredictorKeyedService::Prediction::source_document_url() const {
  return source_document_url_;
}

std::vector<GURL>
NavigationPredictorKeyedService::Prediction::sorted_predicted_urls() const {
  return sorted_predicted_urls_;
}

const content::WebContents*
NavigationPredictorKeyedService::Prediction::web_contents() const {
  return web_contents_;
}

NavigationPredictorKeyedService::NavigationPredictorKeyedService(
    content::BrowserContext* browser_context)
    : search_engine_preconnector_(browser_context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!browser_context->IsOffTheRecord());

  // Start preconnecting to the search engine.
  search_engine_preconnector_.StartPreconnecting(/*with_startup_delay=*/true);
}

NavigationPredictorKeyedService::~NavigationPredictorKeyedService() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void NavigationPredictorKeyedService::OnPredictionUpdated(
    const content::WebContents* web_contents,
    const GURL& document_url,
    const std::vector<GURL>& sorted_predicted_urls) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  last_prediction_ =
      Prediction(web_contents, document_url, sorted_predicted_urls);
  for (auto& observer : observer_list_) {
    observer.OnPredictionUpdated(last_prediction_);
  }
}

void NavigationPredictorKeyedService::AddObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  observer_list_.AddObserver(observer);
  if (last_prediction_.has_value()) {
    observer->OnPredictionUpdated(last_prediction_);
  }
}

void NavigationPredictorKeyedService::RemoveObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  observer_list_.RemoveObserver(observer);
}

SearchEnginePreconnector*
NavigationPredictorKeyedService::SearchEnginePreconnectorForTesting() {
  return &search_engine_preconnector_;
}
