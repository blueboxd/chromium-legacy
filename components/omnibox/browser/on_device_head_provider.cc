// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/on_device_head_provider.h"

#include <limits>

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/i18n/case_conversion.h"
#include "base/metrics/field_trial_params.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/base_search_provider.h"
#include "components/omnibox/browser/on_device_head_provider.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url_service.h"
#include "third_party/metrics_proto/omnibox_input_type.pb.h"

namespace {
const int kBaseRelevance = 99;
const size_t kMaxRequestId = std::numeric_limits<size_t>::max() - 1;
constexpr char kOnDeviceHeadComponentId[] = "feejiaigafnbpeeogmhmjfmkcjplcneb";

bool IsDefaultSearchProviderGoogle(
    const TemplateURLService* template_url_service) {
  if (!template_url_service)
    return false;

  const TemplateURL* default_provider =
      template_url_service->GetDefaultSearchProvider();
  return default_provider &&
         default_provider->GetEngineType(
             template_url_service->search_terms_data()) == SEARCH_ENGINE_GOOGLE;
}

}  // namespace

struct OnDeviceHeadProvider::OnDeviceHeadProviderParams {
  // The id assigned during request creation, which is used to trace this
  // request and determine whether it is current or obsolete.
  const size_t request_id;

  // AutocompleteInput provided by OnDeviceHeadProvider::Start.
  AutocompleteInput input;

  // The suggestions fetched from the on device model which matches the input.
  std::vector<std::string> suggestions;

  // Indicates whether this request failed or not.
  bool failed = false;

  OnDeviceHeadProviderParams(size_t request_id, const AutocompleteInput& input)
      : request_id(request_id), input(input) {}

  ~OnDeviceHeadProviderParams() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(OnDeviceHeadProviderParams);
};

// static
OnDeviceHeadProvider* OnDeviceHeadProvider::Create(
    AutocompleteProviderClient* client,
    AutocompleteProviderListener* listener) {
  return new OnDeviceHeadProvider(client, listener);
}

OnDeviceHeadProvider::OnDeviceHeadProvider(
    AutocompleteProviderClient* client,
    AutocompleteProviderListener* listener)
    : AutocompleteProvider(AutocompleteProvider::TYPE_ON_DEVICE_HEAD),
      client_(client),
      listener_(listener),
      serving_(nullptr),
      task_runner_(base::SequencedTaskRunnerHandle::Get()),
      on_device_search_request_id_(0),
      observer_(this) {
  if (client_ != nullptr) {
    auto* component_update_service = client_->GetComponentUpdateService();
    if (component_update_service != nullptr) {
      observer_.Add(component_update_service);
    }
  }
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&OnDeviceHeadProvider::LoadPreInstalledModel,
                                weak_ptr_factory_.GetWeakPtr()));
}

OnDeviceHeadProvider::~OnDeviceHeadProvider() {
  serving_.reset();
}

bool OnDeviceHeadProvider::IsOnDeviceHeadProviderAllowed(
    const AutocompleteInput& input) {
  // Only accept asynchronous request.
  if (!input.want_asynchronous_matches() ||
      input.type() == metrics::OmniboxInputType::EMPTY)
    return false;

  // Make sure search suggest is enabled and user is not in incognito.
  if (client()->IsOffTheRecord() || !client()->SearchSuggestEnabled())
    return false;

  // Do not proceed if default search provider is not Google.
  return IsDefaultSearchProviderGoogle(client()->GetTemplateURLService());
}

void OnDeviceHeadProvider::Start(const AutocompleteInput& input,
                                 bool minimal_changes) {
  TRACE_EVENT0("omnibox", "OnDeviceHeadProvider::Start");

  // Cancel any in-progress request.
  Stop(!minimal_changes, false);

  if (!IsOnDeviceHeadProviderAllowed(input)) {
    matches_.clear();
    return;
  }

  // If the input text has not changed, the result can be reused.
  if (minimal_changes)
    return;

  matches_.clear();
  if (!input.text().empty() && serving_) {
    done_ = false;
    // Note |on_device_search_request_id_| has already been changed in |Stop|
    // so we don't need to change it again here to get a new id for this
    // request.
    std::unique_ptr<OnDeviceHeadProviderParams> params = base::WrapUnique(
        new OnDeviceHeadProviderParams(on_device_search_request_id_, input));

    // Since the On Device provider usually runs much faster than online
    // providers, it will be very likely users will see on device suggestions
    // first and then the Omnibox UI gets refreshed to show suggestions fetched
    // from server, if we issue both requests simultaneously.
    // Therefore, we might want to delay the On Device suggest requests (and
    // also apply a timeout to search default loader) to mitigate this issue.
    int delay = base::GetFieldTrialParamByFeatureAsInt(
        omnibox::kOnDeviceHeadProvider, "DelayOnDeviceHeadSuggestRequestMs", 0);
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&OnDeviceHeadProvider::DoSearch,
                       weak_ptr_factory_.GetWeakPtr(), std::move(params)),
        delay > 0 ? base::TimeDelta::FromMilliseconds(delay)
                  : base::TimeDelta());
  }
}

void OnDeviceHeadProvider::Stop(bool clear_cached_results,
                                bool due_to_user_inactivity) {
  // Increase the request_id so that any in-progress requests will become
  // obsolete.
  on_device_search_request_id_ =
      (on_device_search_request_id_ + 1) % kMaxRequestId;

  if (clear_cached_results)
    matches_.clear();

  done_ = true;
}

std::string OnDeviceHeadProvider::GetModelFilenameFromInstalledDirectory()
    const {
  // The model file name always ends with "_index.bin"
  base::FileEnumerator model_enum(installed_directory_, false,
                                  base::FileEnumerator::FILES,
                                  FILE_PATH_LITERAL("*_index.bin"));

  base::FilePath model_file_path = model_enum.Next();
  std::string model_filename;

  if (!model_file_path.empty()) {
#if defined(OS_WIN)
    model_filename = base::WideToUTF8(model_file_path.value());
#else
    model_filename = model_file_path.value();
#endif  // defined(OS_WIN)
  }

  return model_filename;
}

// static
void OnDeviceHeadProvider::OverrideEnumDirOnDeviceHeadSuggestForTest(
    const base::FilePath file_path) {
  base::PathService::Override(component_updater::DIR_ON_DEVICE_HEAD_SUGGEST,
                              file_path);
}

// TODO(crbug/925072): Consider to add a listener here and in
// OnDeviceHeadSuggestInstallerPolicy::ComponentReady to load the on device
// model; see discussion at https://chromium-review.googlesource.com/1686677.
void OnDeviceHeadProvider::LoadPreInstalledModel() {
  // Retry the loading for at most 3 times or until the model is successfully
  // loaded.
  for (int i = 3;;) {
    CreateOnDeviceHeadServingInstance();
    --i;
    if (serving_ || i <= 0)
      return;
    base::PlatformThread::Sleep(base::TimeDelta::FromSeconds(15));
  }
}

void OnDeviceHeadProvider::DeleteInstalledDirectory() {
  // TODO(crbug/925072): Maybe log event if deletion fails.
  if (base::PathExists(installed_directory_))
    base::DeleteFile(installed_directory_, true);

  installed_directory_.clear();
}

void OnDeviceHeadProvider::CreateOnDeviceHeadServingInstance() {
  base::FilePath file_path;
  base::PathService::Get(component_updater::DIR_ON_DEVICE_HEAD_SUGGEST,
                         &file_path);

  if (!file_path.empty() && file_path != installed_directory_) {
    DeleteInstalledDirectory();
    installed_directory_ = file_path;
    serving_ = OnDeviceHeadServing::Create(
        GetModelFilenameFromInstalledDirectory(), provider_max_matches_);
  }
}

void OnDeviceHeadProvider::AddProviderInfo(ProvidersInfo* provider_info) const {
  provider_info->push_back(metrics::OmniboxEventProto_ProviderInfo());
  metrics::OmniboxEventProto_ProviderInfo& new_entry = provider_info->back();
  new_entry.set_provider(metrics::OmniboxEventProto::ON_DEVICE_HEAD);
  new_entry.set_provider_done(done_);
}

void OnDeviceHeadProvider::DoSearch(
    std::unique_ptr<OnDeviceHeadProviderParams> params) {
  if (serving_ && params &&
      params->request_id == on_device_search_request_id_) {
    // TODO(crbug.com/925072): Add model search time to UMA.
    base::string16 trimmed_input;
    base::TrimWhitespace(params->input.text(), base::TRIM_ALL, &trimmed_input);
    auto results = serving_->GetSuggestionsForPrefix(
        base::UTF16ToUTF8(base::i18n::ToLower(trimmed_input)));
    params->suggestions.clear();
    for (const auto& item : results) {
      // The second member is the score which is not useful for provider.
      params->suggestions.push_back(item.first);
    }
  } else {
    params->failed = true;
  }
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&OnDeviceHeadProvider::SearchDone,
                     weak_ptr_factory_.GetWeakPtr(), std::move(params)));
}

void OnDeviceHeadProvider::SearchDone(
    std::unique_ptr<OnDeviceHeadProviderParams> params) {
  TRACE_EVENT0("omnibox", "OnDeviceHeadProvider::SearchDone");
  // Ignore this request if it has been stopped or a new one has already been
  // created.
  if (!params || params->request_id != on_device_search_request_id_)
    return;

  const TemplateURLService* template_url_service =
      client()->GetTemplateURLService();

  if (IsDefaultSearchProviderGoogle(template_url_service) && !params->failed) {
    matches_.clear();
    int relevance =
        (params->input.type() != metrics::OmniboxInputType::URL)
            ? base::GetFieldTrialParamByFeatureAsInt(
                  omnibox::kOnDeviceHeadProvider,
                  "OnDeviceSuggestMaxScoreForNonUrlInput", kBaseRelevance)
            : kBaseRelevance;
    for (const auto& item : params->suggestions) {
      matches_.push_back(BaseSearchProvider::CreateOnDeviceSearchSuggestion(
          /*autocomplete_provider=*/this, /*input=*/params->input,
          /*suggestion=*/base::UTF8ToUTF16(item), /*relevance=*/relevance--,
          /*template_url=*/
          template_url_service->GetDefaultSearchProvider(),
          /*search_terms_data=*/
          template_url_service->search_terms_data(),
          /*accepted_suggestion=*/TemplateURLRef::NO_SUGGESTION_CHOSEN));
    }
  }

  done_ = true;
  listener_->OnProviderUpdate(true);
}

// We use component updater to fetch the on device model, which will fire
// Events::COMPONENT_UPDATED when the download is finished. In this case we will
// reload the new model and maybe clean up the old one.
void OnDeviceHeadProvider::OnEvent(Events event, const std::string& id) {
  if (event != Events::COMPONENT_UPDATED || id != kOnDeviceHeadComponentId)
    return;

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&OnDeviceHeadProvider::CreateOnDeviceHeadServingInstance,
                     weak_ptr_factory_.GetWeakPtr()));
}
