// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/blink/blink_optimization_guide_web_contents_observer.h"

#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/optimization_guide/proto/delay_async_script_execution_metadata.pb.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/loader/previews_resource_loading_hints.mojom.h"

namespace optimization_guide {

namespace {

bool IsDelayAsyncScriptExecutionEnabled() {
  static const bool is_feature_enabled =
      base::FeatureList::IsEnabled(
          blink::features::kDelayAsyncScriptExecution) &&
      blink::features::kDelayAsyncScriptExecutionDelayParam.Get() ==
          blink::features::DelayAsyncScriptDelayType::kUseOptimizationGuide;
  return is_feature_enabled;
}

// Used for storing results of CanApplyOptimizationAsync().
struct QueryResult {
  bool is_ready = false;
  OptimizationGuideDecision decision;
  OptimizationMetadata metadata;
};

blink::mojom::DelayAsyncScriptExecutionHintsPtr
CreateDelayAsyncScriptExecutionHints(
    content::NavigationHandle* navigation_handle,
    OptimizationGuideDecider* decider) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(IsDelayAsyncScriptExecutionEnabled());

  // CanApplyOptimizationAsync() synchronously runs the callback when the hints
  // are already available. The following code assumes the case.
  // TODO(https://crbug.com/1113980): Support the case where the hints get
  // available after this point.

  // Creates an object to share the result between the current calling function
  // and the callback for CanApplyOptimizationAsync(). This should be refcounted
  // because the callback may outlive the current calling function and vice
  // versa.
  auto result = base::MakeRefCounted<base::RefCountedData<QueryResult>>();

  decider->CanApplyOptimizationAsync(
      navigation_handle, proto::OptimizationType::DELAY_ASYNC_SCRIPT_EXECUTION,
      base::BindOnce(
          [](scoped_refptr<base::RefCountedData<QueryResult>> result,
             OptimizationGuideDecision decision,
             const OptimizationMetadata& metadata) {
            result->data = {true, decision, metadata};
          },
          result));

  // TODO(https://crbug.com/1113980): Add UMAs to record if the hints are
  // available on navigation commit ready.

  // Didn't get the hints synchronously.
  if (!result->data.is_ready)
    return nullptr;

  switch (result->data.decision) {
    case OptimizationGuideDecision::kTrue:
      break;
    case OptimizationGuideDecision::kUnknown:
    case OptimizationGuideDecision::kFalse:
      // The optimization guide service decided not to provide the hints.
      return nullptr;
  }

  // Give up providing the hints when the metadata is not available.
  base::Optional<proto::DelayAsyncScriptExecutionMetadata> metadata =
      result->data.metadata.delay_async_script_execution_metadata();
  if (!metadata || !metadata->delay_type())
    return nullptr;

  // Populate the decision into the hints.
  using blink::mojom::DelayAsyncScriptExecutionDelayType;
  auto hints = blink::mojom::DelayAsyncScriptExecutionHints::New();
  switch (metadata->delay_type()) {
    case proto::DelayType::DELAY_TYPE_UNKNOWN:
      hints->delay_type = DelayAsyncScriptExecutionDelayType::kUnknown;
      break;
    case proto::DelayType::DELAY_TYPE_FINISHED_PARSING:
      hints->delay_type = DelayAsyncScriptExecutionDelayType::kFinishedParsing;
      break;
    case proto::DelayType::DELAY_TYPE_FIRST_PAINT_OR_FINISHED_PARSING:
      hints->delay_type =
          DelayAsyncScriptExecutionDelayType::kFirstPaintOrFinishedParsing;
      break;
  }
  return hints;
}

}  // namespace

BlinkOptimizationGuideWebContentsObserver::
    ~BlinkOptimizationGuideWebContentsObserver() = default;

void BlinkOptimizationGuideWebContentsObserver::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Currently the optimization guide supports only the main frame navigation.
  if (!navigation_handle->IsInMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  // Don't support non-HTTP(S) navigation.
  if (!navigation_handle->GetURL().SchemeIsHTTPOrHTTPS())
    return;

  OptimizationGuideDecider* decider =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile_);
  if (!decider)
    return;

  auto hints = blink::mojom::BlinkOptimizationGuideHints::New();
  if (IsDelayAsyncScriptExecutionEnabled()) {
    hints->delay_async_script_execution_hints =
        CreateDelayAsyncScriptExecutionHints(navigation_handle, decider);
  }

  // Tentatively use the Previews interface to talk with the renderer.
  // TODO(https://crbug.com/1113980): Implement our own interface.
  mojo::AssociatedRemote<blink::mojom::PreviewsResourceLoadingHintsReceiver>
      hints_receiver_associated;
  if (navigation_handle->GetRenderFrameHost()
          ->GetRemoteAssociatedInterfaces()) {
    navigation_handle->GetRenderFrameHost()
        ->GetRemoteAssociatedInterfaces()
        ->GetInterface(&hints_receiver_associated);
  }

  // Keeping the hints to be sent for testing.
  // TODO(https://crbug.com/1113980): Stop doing this, and replace this with a
  // less intrusive way.
  sent_hints_for_testing_ = hints.Clone();

  // Sends the hints to the renderer.
  hints_receiver_associated->SetBlinkOptimizationGuideHints(std::move(hints));
}

BlinkOptimizationGuideWebContentsObserver::
    BlinkOptimizationGuideWebContentsObserver(
        content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      profile_(Profile::FromBrowserContext(web_contents->GetBrowserContext())) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  OptimizationGuideDecider* decider =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile_);
  if (!decider)
    return;

  // Register the optimization types which we want to subscribe to.
  std::vector<proto::OptimizationType> opts;
  if (IsDelayAsyncScriptExecutionEnabled())
    opts.push_back(proto::OptimizationType::DELAY_ASYNC_SCRIPT_EXECUTION);
  if (!opts.empty())
    decider->RegisterOptimizationTypes(opts);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(BlinkOptimizationGuideWebContentsObserver)

}  // namespace optimization_guide
