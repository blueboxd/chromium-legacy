// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/lazy_load_image_observer.h"

#include <limits>

#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "third_party/blink/public/platform/web_effective_connection_type.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html_element_type_helpers.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_entry.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/network/network_state_notifier.h"

namespace blink {

namespace {

int GetLazyImageLoadingViewportDistanceThresholdPx(const Document& document) {
  const Settings* settings = document.GetSettings();
  if (!settings)
    return 0;

  switch (GetNetworkStateNotifier().EffectiveType()) {
    case WebEffectiveConnectionType::kTypeUnknown:
      return settings->GetLazyImageLoadingDistanceThresholdPxUnknown();
    case WebEffectiveConnectionType::kTypeOffline:
      return settings->GetLazyImageLoadingDistanceThresholdPxOffline();
    case WebEffectiveConnectionType::kTypeSlow2G:
      return settings->GetLazyImageLoadingDistanceThresholdPxSlow2G();
    case WebEffectiveConnectionType::kType2G:
      return settings->GetLazyImageLoadingDistanceThresholdPx2G();
    case WebEffectiveConnectionType::kType3G:
      return settings->GetLazyImageLoadingDistanceThresholdPx3G();
    case WebEffectiveConnectionType::kType4G:
      return settings->GetLazyImageLoadingDistanceThresholdPx4G();
  }
  NOTREACHED();
  return 0;
}

Document* GetRootDocumentOrNull(Element* element) {
  if (LocalFrame* frame = element->GetDocument().GetFrame())
    return frame->LocalFrameRoot().GetDocument();
  return nullptr;
}

// Returns if the element or its ancestors are invisible, due to their style or
// attribute or due to themselves not connected to the main document tree.
bool IsElementInInvisibleSubTree(const Element& element) {
  if (!element.isConnected())
    return true;
  for (Node& ancestor : FlatTreeTraversal::InclusiveAncestorsOf(element)) {
    auto* ancestor_element = DynamicTo<Element>(ancestor);
    if (!ancestor_element)
      continue;
    const ComputedStyle* style = ancestor_element->EnsureComputedStyle();
    if (style && (style->Visibility() != EVisibility::kVisible ||
                  style->Display() == EDisplay::kNone)) {
      return true;
    }
  }
  return false;
}

}  // namespace

void LazyLoadImageObserver::StartMonitoring(Element* element,
                                            DeferralMessage deferral_message) {
  if (Document* document = GetRootDocumentOrNull(element)) {
    document->EnsureLazyLoadImageObserver().StartMonitoringNearViewport(
        document, element, deferral_message);
  }
}

void LazyLoadImageObserver::StopMonitoring(Element* element) {
  if (Document* document = GetRootDocumentOrNull(element)) {
    document->EnsureLazyLoadImageObserver()
        .lazy_load_intersection_observer_->unobserve(element);
  }
}

void LazyLoadImageObserver::StartTrackingVisibilityMetrics(
    HTMLImageElement* image_element) {
  if (!RuntimeEnabledFeatures::LazyImageVisibleLoadTimeMetricsEnabled())
    return;
  if (Document* document = GetRootDocumentOrNull(image_element)) {
    document->EnsureLazyLoadImageObserver().StartMonitoringVisibility(
        document, image_element);
  }
}

void LazyLoadImageObserver::RecordMetricsOnLoadFinished(
    HTMLImageElement* image_element) {
  if (!RuntimeEnabledFeatures::LazyImageVisibleLoadTimeMetricsEnabled())
    return;
  if (Document* document = GetRootDocumentOrNull(image_element)) {
    document->EnsureLazyLoadImageObserver().OnLoadFinished(image_element);
  }
}

LazyLoadImageObserver::LazyLoadImageObserver() = default;

void LazyLoadImageObserver::StartMonitoringNearViewport(
    Document* root_document,
    Element* element,
    DeferralMessage deferral_message) {
  DCHECK(RuntimeEnabledFeatures::LazyImageLoadingEnabled());

  if (!lazy_load_intersection_observer_) {
    lazy_load_intersection_observer_ = IntersectionObserver::Create(
        {Length::Fixed(
            GetLazyImageLoadingViewportDistanceThresholdPx(*root_document))},
        {std::numeric_limits<float>::min()}, root_document,
        WTF::BindRepeating(&LazyLoadImageObserver::LoadIfNearViewport,
                           WrapWeakPersistent(this)));
  }
  lazy_load_intersection_observer_->observe(element);

  if (deferral_message == DeferralMessage::kLoadEventsDeferred &&
      !is_load_event_deferred_intervention_shown_) {
    is_load_event_deferred_intervention_shown_ = true;
    root_document->AddConsoleMessage(ConsoleMessage::Create(
        mojom::ConsoleMessageSource::kIntervention,
        mojom::ConsoleMessageLevel::kInfo,
        "Images loaded lazily and replaced with placeholders. Load events are "
        "deferred. See https://crbug.com/954323"));
  }
  if (deferral_message == DeferralMessage::kMissingDimensionForLazy &&
      !is_missing_dimension_intervention_shown_) {
    is_missing_dimension_intervention_shown_ = true;
    root_document->AddConsoleMessage(ConsoleMessage::Create(
        mojom::ConsoleMessageSource::kIntervention,
        mojom::ConsoleMessageLevel::kInfo,
        "An <img> element was lazyloaded with loading=lazy, but had no "
        "dimensions specified. Specifying dimensions improves performance. See "
        "https://crbug.com/954323"));
    UseCounter::Count(root_document,
                      WebFeature::kLazyLoadImageMissingDimensionsForLazy);
  }
}

void LazyLoadImageObserver::LoadIfNearViewport(
    const HeapVector<Member<IntersectionObserverEntry>>& entries) {
  DCHECK(!entries.IsEmpty());

  for (auto entry : entries) {
    Element* element = entry->target();
    HTMLImageElement* image_element = ToHTMLImageElementOrNull(element);
    if (!entry->isIntersecting() && image_element) {
      // Fully load the invisible image elements. The elements can be invisible
      // by style such as display:none, visibility: hidden, or hidden via
      // attribute, etc. Style might also not be calculated if the ancestors
      // were invisible.
      const ComputedStyle* style = entry->target()->GetComputedStyle();
      if (!style || style->Visibility() != EVisibility::kVisible ||
          style->Display() == EDisplay::kNone) {
        // Check that style was null because it was not computed since the
        // element was in an invisible subtree.
        DCHECK(style || IsElementInInvisibleSubTree(*element));
        image_element->LoadDeferredImage();
        lazy_load_intersection_observer_->unobserve(element);
      }
    }
    if (!entry->isIntersecting())
      continue;
    if (image_element)
      image_element->LoadDeferredImage();

    // Load the background image if the element has one deferred.
    if (const ComputedStyle* style = element->GetComputedStyle())
      style->LoadDeferredImages(element->GetDocument());

    lazy_load_intersection_observer_->unobserve(element);
  }
}

void LazyLoadImageObserver::StartMonitoringVisibility(
    Document* root_document,
    HTMLImageElement* image_element) {
  DCHECK(RuntimeEnabledFeatures::LazyImageVisibleLoadTimeMetricsEnabled());

  VisibleLoadTimeMetrics& visible_load_time_metrics =
      image_element->EnsureVisibleLoadTimeMetrics();
  if (visible_load_time_metrics.has_initial_intersection_been_set) {
    // The element has already been monitored.
    return;
  }
  if (!visibility_metrics_observer_) {
    visibility_metrics_observer_ = IntersectionObserver::Create(
        {}, {std::numeric_limits<float>::min()}, root_document,
        WTF::BindRepeating(&LazyLoadImageObserver::OnVisibilityChanged,
                           WrapWeakPersistent(this)));
  }
  visible_load_time_metrics.record_visibility_metrics = true;
  visibility_metrics_observer_->observe(image_element);
}

void LazyLoadImageObserver::OnLoadFinished(HTMLImageElement* image_element) {
  DCHECK(RuntimeEnabledFeatures::LazyImageVisibleLoadTimeMetricsEnabled());

  VisibleLoadTimeMetrics& visible_load_time_metrics =
      image_element->EnsureVisibleLoadTimeMetrics();
  if (!visible_load_time_metrics.record_visibility_metrics)
    return;

  visible_load_time_metrics.record_visibility_metrics = false;
  visibility_metrics_observer_->unobserve(image_element);

  base::TimeDelta visible_load_delay;
  if (!visible_load_time_metrics.time_when_first_visible.is_null()) {
    visible_load_delay = base::TimeTicks::Now() -
                         visible_load_time_metrics.time_when_first_visible;
  }

  switch (GetNetworkStateNotifier().EffectiveType()) {
    case WebEffectiveConnectionType::kTypeSlow2G:
      if (visible_load_time_metrics.is_initially_intersecting) {
        UMA_HISTOGRAM_MEDIUM_TIMES(
            "Blink.VisibleLoadTime.LazyLoadImages.AboveTheFold.Slow2G",
            visible_load_delay);
      } else {
        UMA_HISTOGRAM_MEDIUM_TIMES(
            "Blink.VisibleLoadTime.LazyLoadImages.BelowTheFold.Slow2G",
            visible_load_delay);
      }
      break;

    case WebEffectiveConnectionType::kType2G:
      if (visible_load_time_metrics.is_initially_intersecting) {
        UMA_HISTOGRAM_MEDIUM_TIMES(
            "Blink.VisibleLoadTime.LazyLoadImages.AboveTheFold.2G",
            visible_load_delay);
      } else {
        UMA_HISTOGRAM_MEDIUM_TIMES(
            "Blink.VisibleLoadTime.LazyLoadImages.BelowTheFold.2G",
            visible_load_delay);
      }
      break;

    case WebEffectiveConnectionType::kType3G:
      if (visible_load_time_metrics.is_initially_intersecting) {
        UMA_HISTOGRAM_MEDIUM_TIMES(
            "Blink.VisibleLoadTime.LazyLoadImages.AboveTheFold.3G",
            visible_load_delay);
      } else {
        UMA_HISTOGRAM_MEDIUM_TIMES(
            "Blink.VisibleLoadTime.LazyLoadImages.BelowTheFold.3G",
            visible_load_delay);
      }
      break;

    case WebEffectiveConnectionType::kType4G:
      if (visible_load_time_metrics.is_initially_intersecting) {
        UMA_HISTOGRAM_MEDIUM_TIMES(
            "Blink.VisibleLoadTime.LazyLoadImages.AboveTheFold.4G",
            visible_load_delay);
      } else {
        UMA_HISTOGRAM_MEDIUM_TIMES(
            "Blink.VisibleLoadTime.LazyLoadImages.BelowTheFold.4G",
            visible_load_delay);
      }
      break;

    case WebEffectiveConnectionType::kTypeUnknown:
    case WebEffectiveConnectionType::kTypeOffline:
      // No VisibleLoadTime histograms are recorded for these effective
      // connection types.
      break;
  }
}

void LazyLoadImageObserver::OnVisibilityChanged(
    const HeapVector<Member<IntersectionObserverEntry>>& entries) {
  DCHECK(!entries.IsEmpty());

  for (auto entry : entries) {
    if (auto* image_element = ToHTMLImageElementOrNull(entry->target())) {
      VisibleLoadTimeMetrics& visible_load_time_metrics =
          image_element->EnsureVisibleLoadTimeMetrics();
      if (!visible_load_time_metrics.has_initial_intersection_been_set) {
        visible_load_time_metrics.is_initially_intersecting =
            entry->isIntersecting();
        visible_load_time_metrics.has_initial_intersection_been_set = true;
      }
      if (entry->isIntersecting()) {
        DCHECK(visible_load_time_metrics.time_when_first_visible.is_null());
        visible_load_time_metrics.time_when_first_visible =
            base::TimeTicks::Now();

        if (visible_load_time_metrics.record_visibility_metrics &&
            image_element->GetDocument().GetFrame()) {
          // Since the visibility metrics are recorded when the image finishes
          // loading, this means that the image became visible before it
          // finished loading.

          // Note: If the WebEffectiveConnectionType enum ever gets out of sync
          // with net::EffectiveConnectionType, then both the AboveTheFold and
          // BelowTheFold histograms here will have to be updated to record the
          // sample in terms of net::EffectiveConnectionType instead of
          // WebEffectiveConnectionType.
          if (visible_load_time_metrics.is_initially_intersecting) {
            UMA_HISTOGRAM_ENUMERATION(
                "Blink.VisibleBeforeLoaded.LazyLoadImages.AboveTheFold",
                GetNetworkStateNotifier().EffectiveType());
          } else {
            UMA_HISTOGRAM_ENUMERATION(
                "Blink.VisibleBeforeLoaded.LazyLoadImages.BelowTheFold",
                GetNetworkStateNotifier().EffectiveType());
          }
        }

        visibility_metrics_observer_->unobserve(image_element);
      }
    }
  }
}

void LazyLoadImageObserver::Trace(Visitor* visitor) {
  visitor->Trace(lazy_load_intersection_observer_);
  visitor->Trace(visibility_metrics_observer_);
}

}  // namespace blink
