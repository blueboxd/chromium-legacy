/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/svg/animation/svg_smil_element.h"

#include <algorithm>

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_event_listener.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/dom/id_target_observer.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/svg/animation/smil_time_container.h"
#include "third_party/blink/renderer/core/svg/svg_svg_element.h"
#include "third_party/blink/renderer/core/svg/svg_uri_reference.h"
#include "third_party/blink/renderer/core/xlink_names.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

// Compute the next time an interval with a certain (non-zero) simple duration
// will repeat, relative to a certain presentation time.
SMILTime ComputeNextRepeatTime(SMILTime interval_begin,
                               SMILTime simple_duration,
                               SMILTime presentation_time) {
  DCHECK(simple_duration);
  DCHECK_LE(interval_begin, presentation_time);
  SMILTime time_in_interval = presentation_time - interval_begin;
  SMILTime time_until_next_repeat =
      simple_duration - (time_in_interval % simple_duration);
  return presentation_time + time_until_next_repeat;
}

}  // namespace

// This is used for duration type time values that can't be negative.
static constexpr SMILTime kInvalidCachedTime = SMILTime::Earliest();

class ConditionEventListener final : public NativeEventListener {
 public:
  ConditionEventListener(SVGSMILElement* animation,
                         SVGSMILElement::Condition* condition)
      : animation_(animation), condition_(condition) {}

  bool Matches(const EventListener& other) const override;

  void DisconnectAnimation() { animation_ = nullptr; }

  void Invoke(ExecutionContext*, Event*) override;

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(animation_);
    visitor->Trace(condition_);
    NativeEventListener::Trace(visitor);
  }

  bool IsConditionEventListener() const override { return true; }

 private:
  Member<SVGSMILElement> animation_;
  Member<SVGSMILElement::Condition> condition_;
};

template <>
struct DowncastTraits<ConditionEventListener> {
  static bool AllowFrom(const EventListener& event_listener) {
    const NativeEventListener* native_event_listener =
        DynamicTo<NativeEventListener>(event_listener);
    return native_event_listener &&
           native_event_listener->IsConditionEventListener();
  }
};

bool ConditionEventListener::Matches(const EventListener& listener) const {
  if (const ConditionEventListener* condition_event_listener =
          DynamicTo<ConditionEventListener>(listener)) {
    return animation_ == condition_event_listener->animation_ &&
           condition_ == condition_event_listener->condition_;
  }
  return false;
}

void ConditionEventListener::Invoke(ExecutionContext*, Event* event) {
  if (!animation_)
    return;
  animation_->IntervalIsDirty();
  animation_->AddInstanceTime(condition_->GetBeginOrEnd(),
                              animation_->Elapsed() + condition_->Offset());
}

SVGSMILElement::Condition::Condition(Type type,
                                     BeginOrEnd begin_or_end,
                                     const AtomicString& base_id,
                                     const AtomicString& name,
                                     SMILTime offset,
                                     unsigned repeat)
    : type_(type),
      begin_or_end_(begin_or_end),
      base_id_(base_id),
      name_(name),
      offset_(offset),
      repeat_(repeat) {}

SVGSMILElement::Condition::~Condition() = default;

void SVGSMILElement::Condition::Trace(blink::Visitor* visitor) {
  visitor->Trace(base_element_);
  visitor->Trace(base_id_observer_);
  visitor->Trace(event_listener_);
}

void SVGSMILElement::Condition::ConnectSyncBase(SVGSMILElement& timed_element) {
  DCHECK(!base_id_.IsEmpty());
  DCHECK_EQ(type_, kSyncBase);
  auto* svg_smil_element =
      DynamicTo<SVGSMILElement>(SVGURIReference::ObserveTarget(
          base_id_observer_, timed_element.GetTreeScope(), base_id_,
          WTF::BindRepeating(&SVGSMILElement::BuildPendingResource,
                             WrapWeakPersistent(&timed_element))));
  if (!svg_smil_element) {
    base_element_ = nullptr;
    return;
  }
  base_element_ = svg_smil_element;
  svg_smil_element->AddSyncBaseDependent(timed_element);
}

void SVGSMILElement::Condition::DisconnectSyncBase(
    SVGSMILElement& timed_element) {
  DCHECK_EQ(type_, kSyncBase);
  SVGURIReference::UnobserveTarget(base_id_observer_);
  if (!base_element_)
    return;
  To<SVGSMILElement>(*base_element_).RemoveSyncBaseDependent(timed_element);
  base_element_ = nullptr;
}

void SVGSMILElement::Condition::ConnectEventBase(
    SVGSMILElement& timed_element) {
  DCHECK_EQ(type_, kEventBase);
  DCHECK(!base_element_);
  Element* target;
  if (base_id_.IsEmpty()) {
    target = timed_element.targetElement();
  } else {
    target = SVGURIReference::ObserveTarget(
        base_id_observer_, timed_element.GetTreeScope(), base_id_,
        WTF::BindRepeating(&SVGSMILElement::BuildPendingResource,
                           WrapWeakPersistent(&timed_element)));
  }
  auto* svg_element = DynamicTo<SVGElement>(target);
  if (!svg_element)
    return;
  DCHECK(!event_listener_);
  event_listener_ =
      MakeGarbageCollected<ConditionEventListener>(&timed_element, this);
  base_element_ = svg_element;
  base_element_->addEventListener(name_, event_listener_, false);
  timed_element.AddReferenceTo(base_element_);
}

void SVGSMILElement::Condition::DisconnectEventBase(
    SVGSMILElement& timed_element) {
  DCHECK_EQ(type_, kEventBase);
  SVGURIReference::UnobserveTarget(base_id_observer_);
  if (!event_listener_)
    return;
  base_element_->removeEventListener(name_, event_listener_, false);
  base_element_ = nullptr;
  event_listener_->DisconnectAnimation();
  event_listener_ = nullptr;
}

SVGSMILElement::SVGSMILElement(const QualifiedName& tag_name, Document& doc)
    : SVGElement(tag_name, doc),
      SVGTests(this),
      attribute_name_(AnyQName()),
      target_element_(nullptr),
      sync_base_conditions_connected_(false),
      has_end_event_conditions_(false),
      is_waiting_for_first_interval_(true),
      is_scheduled_(false),
      interval_{SMILTime::Unresolved(), SMILTime::Unresolved()},
      previous_interval_{SMILTime::Unresolved(), SMILTime::Unresolved()},
      active_state_(kInactive),
      restart_(kRestartAlways),
      fill_(kFillRemove),
      last_percent_(0),
      last_repeat_(0),
      document_order_index_(0),
      cached_dur_(kInvalidCachedTime),
      cached_repeat_dur_(kInvalidCachedTime),
      cached_repeat_count_(SMILRepeatCount::Invalid()),
      cached_min_(kInvalidCachedTime),
      cached_max_(kInvalidCachedTime),
      interval_has_changed_(false) {
  ResolveFirstInterval();
}

SVGSMILElement::~SVGSMILElement() = default;

void SVGSMILElement::ClearResourceAndEventBaseReferences() {
  SVGURIReference::UnobserveTarget(target_id_observer_);
  RemoveAllOutgoingReferences();
}

void SVGSMILElement::ClearConditions() {
  DisconnectSyncBaseConditions();
  DisconnectEventBaseConditions();
  conditions_.clear();
}

void SVGSMILElement::BuildPendingResource() {
  ClearResourceAndEventBaseReferences();
  DisconnectEventBaseConditions();
  DisconnectSyncBaseConditions();

  if (!isConnected()) {
    // Reset the target element if we are no longer in the document.
    SetTargetElement(nullptr);
    return;
  }

  const AtomicString& href = SVGURIReference::LegacyHrefString(*this);
  Element* target;
  if (href.IsEmpty()) {
    target = parentElement();
  } else {
    target = SVGURIReference::ObserveTarget(target_id_observer_, *this, href);
  }
  auto* svg_target = DynamicTo<SVGElement>(target);

  if (svg_target && !svg_target->isConnected())
    svg_target = nullptr;

  SetTargetElement(svg_target);

  if (svg_target) {
    // Register us with the target in the dependencies map. Any change of
    // hrefElement that leads to relayout/repainting now informs us, so we can
    // react to it.
    AddReferenceTo(svg_target);
  }
  ConnectSyncBaseConditions();
  ConnectEventBaseConditions();
}

static inline void ClearTimesWithDynamicOrigins(
    Vector<SMILTimeWithOrigin>& time_list) {
  for (int i = time_list.size() - 1; i >= 0; --i) {
    if (time_list[i].OriginIsScript())
      time_list.EraseAt(i);
  }
}

void SVGSMILElement::Reset() {
  active_state_ = kInactive;
  is_waiting_for_first_interval_ = true;
  interval_ = {SMILTime::Unresolved(), SMILTime::Unresolved()};
  previous_interval_ = {SMILTime::Unresolved(), SMILTime::Unresolved()};
  last_percent_ = 0;
  last_repeat_ = 0;
  ResolveFirstInterval();
}

Node::InsertionNotificationRequest SVGSMILElement::InsertedInto(
    ContainerNode& root_parent) {
  SVGElement::InsertedInto(root_parent);

  if (!root_parent.isConnected())
    return kInsertionDone;

  UseCounter::Count(GetDocument(), WebFeature::kSVGSMILElementInDocument);
  if (GetDocument().IsLoadCompleted()) {
    UseCounter::Count(&GetDocument(),
                      WebFeature::kSVGSMILElementInsertedAfterLoad);
  }

  SVGSVGElement* owner = ownerSVGElement();
  if (!owner)
    return kInsertionDone;

  time_container_ = owner->TimeContainer();
  DCHECK(time_container_);
  time_container_->SetDocumentOrderIndexesDirty();

  // "If no attribute is present, the default begin value (an offset-value of 0)
  // must be evaluated."
  if (!FastHasAttribute(svg_names::kBeginAttr))
    begin_times_.push_back(SMILTimeWithOrigin());

  if (is_waiting_for_first_interval_)
    ResolveFirstInterval();

  if (time_container_)
    time_container_->NotifyIntervalsChanged();

  BuildPendingResource();

  return kInsertionDone;
}

void SVGSMILElement::RemovedFrom(ContainerNode& root_parent) {
  if (root_parent.isConnected()) {
    ClearResourceAndEventBaseReferences();
    ClearConditions();
    SetTargetElement(nullptr);
    AnimationAttributeChanged();
    time_container_ = nullptr;
  }

  SVGElement::RemovedFrom(root_parent);
}

SMILTime SVGSMILElement::ParseOffsetValue(const String& data) {
  bool ok;
  double result = 0;
  String parse = data.StripWhiteSpace();
  if (parse.EndsWith('h'))
    result = parse.Left(parse.length() - 1).ToDouble(&ok) * 60 * 60;
  else if (parse.EndsWith("min"))
    result = parse.Left(parse.length() - 3).ToDouble(&ok) * 60;
  else if (parse.EndsWith("ms"))
    result = parse.Left(parse.length() - 2).ToDouble(&ok) / 1000;
  else if (parse.EndsWith('s'))
    result = parse.Left(parse.length() - 1).ToDouble(&ok);
  else
    result = parse.ToDouble(&ok);
  if (!ok)
    return SMILTime::Unresolved();
  SMILTime offset_value = SMILTime::FromSecondsD(result);
  if (!offset_value.IsFinite())
    return SMILTime::Unresolved();
  return offset_value;
}

SMILTime SVGSMILElement::ParseClockValue(const String& data) {
  if (data.IsNull())
    return SMILTime::Unresolved();

  String parse = data.StripWhiteSpace();

  DEFINE_STATIC_LOCAL(const AtomicString, indefinite_value, ("indefinite"));
  if (parse == indefinite_value)
    return SMILTime::Indefinite();

  double result = 0;
  bool ok;
  wtf_size_t double_point_one = parse.find(':');
  wtf_size_t double_point_two = parse.find(':', double_point_one + 1);
  if (double_point_one == 2 && double_point_two == 5 && parse.length() >= 8) {
    result += parse.Substring(0, 2).ToUIntStrict(&ok) * 60 * 60;
    if (!ok)
      return SMILTime::Unresolved();
    result += parse.Substring(3, 2).ToUIntStrict(&ok) * 60;
    if (!ok)
      return SMILTime::Unresolved();
    result += parse.Substring(6).ToDouble(&ok);
  } else if (double_point_one == 2 && double_point_two == kNotFound &&
             parse.length() >= 5) {
    result += parse.Substring(0, 2).ToUIntStrict(&ok) * 60;
    if (!ok)
      return SMILTime::Unresolved();
    result += parse.Substring(3).ToDouble(&ok);
  } else {
    return ParseOffsetValue(parse);
  }

  if (!ok)
    return SMILTime::Unresolved();
  SMILTime clock_value = SMILTime::FromSecondsD(result);
  if (!clock_value.IsFinite())
    return SMILTime::Unresolved();
  return clock_value;
}

bool SVGSMILElement::ParseCondition(const String& value,
                                    BeginOrEnd begin_or_end) {
  String parse_string = value.StripWhiteSpace();

  bool is_negated = false;
  bool ok;
  wtf_size_t pos = parse_string.find('+');
  if (pos == kNotFound) {
    pos = parse_string.find('-');
    is_negated = pos != kNotFound;
  }
  String condition_string;
  SMILTime offset;
  if (pos == kNotFound)
    condition_string = parse_string;
  else {
    condition_string = parse_string.Left(pos).StripWhiteSpace();
    String offset_string = parse_string.Substring(pos + 1).StripWhiteSpace();
    offset = ParseOffsetValue(offset_string);
    if (offset.IsUnresolved())
      return false;
    if (is_negated)
      offset = -offset;
  }
  if (condition_string.IsEmpty())
    return false;
  pos = condition_string.find('.');

  String base_id;
  String name_string;
  if (pos == kNotFound)
    name_string = condition_string;
  else {
    base_id = condition_string.Left(pos);
    name_string = condition_string.Substring(pos + 1);
  }
  if (name_string.IsEmpty())
    return false;

  Condition::Type type;
  int repeat = -1;
  if (name_string.StartsWith("repeat(") && name_string.EndsWith(')')) {
    repeat =
        name_string.Substring(7, name_string.length() - 8).ToUIntStrict(&ok);
    if (!ok)
      return false;
    name_string = "repeat";
    type = Condition::kSyncBase;
  } else if (name_string == "begin" || name_string == "end") {
    if (base_id.IsEmpty())
      return false;
    UseCounter::Count(&GetDocument(),
                      WebFeature::kSVGSMILBeginOrEndSyncbaseValue);
    type = Condition::kSyncBase;
  } else if (name_string.StartsWith("accesskey(")) {
    // FIXME: accesskey() support.
    type = Condition::kAccessKey;
  } else {
    UseCounter::Count(&GetDocument(), WebFeature::kSVGSMILBeginOrEndEventValue);
    type = Condition::kEventBase;
  }

  conditions_.push_back(MakeGarbageCollected<Condition>(
      type, begin_or_end, AtomicString(base_id), AtomicString(name_string),
      offset, repeat));

  if (type == Condition::kEventBase && begin_or_end == kEnd)
    has_end_event_conditions_ = true;

  return true;
}

void SVGSMILElement::ParseBeginOrEnd(const String& parse_string,
                                     BeginOrEnd begin_or_end) {
  Vector<SMILTimeWithOrigin>& time_list =
      begin_or_end == kBegin ? begin_times_ : end_times_;
  if (begin_or_end == kEnd)
    has_end_event_conditions_ = false;
  HashSet<SMILTime> existing;
  for (const auto& instance_time : time_list) {
    if (!instance_time.Time().IsUnresolved())
      existing.insert(instance_time.Time());
  }
  Vector<String> split_string;
  parse_string.Split(';', split_string);
  for (const auto& item : split_string) {
    SMILTime value = ParseClockValue(item);
    if (value.IsUnresolved()) {
      ParseCondition(item, begin_or_end);
    } else if (!existing.Contains(value)) {
      time_list.push_back(
          SMILTimeWithOrigin(value, SMILTimeWithOrigin::kParserOrigin));
    }
  }
  std::sort(time_list.begin(), time_list.end());
}

void SVGSMILElement::ParseAttribute(const AttributeModificationParams& params) {
  const QualifiedName& name = params.name;
  const AtomicString& value = params.new_value;
  if (name == svg_names::kBeginAttr) {
    if (!conditions_.IsEmpty()) {
      ClearConditions();
      ParseBeginOrEnd(FastGetAttribute(svg_names::kEndAttr), kEnd);
    }
    ParseBeginOrEnd(value.GetString(), kBegin);
    if (isConnected()) {
      ConnectSyncBaseConditions();
      ConnectEventBaseConditions();
      BeginListChanged(Elapsed());
    }
    AnimationAttributeChanged();
  } else if (name == svg_names::kEndAttr) {
    if (!conditions_.IsEmpty()) {
      ClearConditions();
      ParseBeginOrEnd(FastGetAttribute(svg_names::kBeginAttr), kBegin);
    }
    ParseBeginOrEnd(value.GetString(), kEnd);
    if (isConnected()) {
      ConnectSyncBaseConditions();
      ConnectEventBaseConditions();
      EndListChanged(Elapsed());
    }
    AnimationAttributeChanged();
  } else if (name == svg_names::kOnbeginAttr) {
    SetAttributeEventListener(event_type_names::kBeginEvent,
                              CreateAttributeEventListener(this, name, value));
  } else if (name == svg_names::kOnendAttr) {
    SetAttributeEventListener(event_type_names::kEndEvent,
                              CreateAttributeEventListener(this, name, value));
  } else if (name == svg_names::kOnrepeatAttr) {
    SetAttributeEventListener(event_type_names::kRepeatEvent,
                              CreateAttributeEventListener(this, name, value));
  } else if (name == svg_names::kRestartAttr) {
    if (value == "never")
      restart_ = kRestartNever;
    else if (value == "whenNotActive")
      restart_ = kRestartWhenNotActive;
    else
      restart_ = kRestartAlways;
  } else if (name == svg_names::kFillAttr) {
    fill_ = value == "freeze" ? kFillFreeze : kFillRemove;
  } else {
    SVGElement::ParseAttribute(params);
  }
}

void SVGSMILElement::SvgAttributeChanged(const QualifiedName& attr_name) {
  if (attr_name == svg_names::kDurAttr) {
    cached_dur_ = kInvalidCachedTime;
  } else if (attr_name == svg_names::kRepeatDurAttr) {
    cached_repeat_dur_ = kInvalidCachedTime;
  } else if (attr_name == svg_names::kRepeatCountAttr) {
    cached_repeat_count_ = SMILRepeatCount::Invalid();
  } else if (attr_name == svg_names::kMinAttr) {
    cached_min_ = kInvalidCachedTime;
  } else if (attr_name == svg_names::kMaxAttr) {
    cached_max_ = kInvalidCachedTime;
  } else if (attr_name.Matches(svg_names::kHrefAttr) ||
             attr_name.Matches(xlink_names::kHrefAttr)) {
    // TODO(fs): Could be smarter here when 'href' is specified and 'xlink:href'
    // is changed.
    SVGElement::InvalidationGuard invalidation_guard(this);
    BuildPendingResource();
  } else {
    SVGElement::SvgAttributeChanged(attr_name);
    return;
  }

  AnimationAttributeChanged();
}

void SVGSMILElement::ConnectSyncBaseConditions() {
  if (sync_base_conditions_connected_)
    DisconnectSyncBaseConditions();
  sync_base_conditions_connected_ = true;
  for (Condition* condition : conditions_) {
    if (condition->GetType() == Condition::kSyncBase)
      condition->ConnectSyncBase(*this);
  }
}

void SVGSMILElement::DisconnectSyncBaseConditions() {
  if (!sync_base_conditions_connected_)
    return;
  sync_base_conditions_connected_ = false;
  for (Condition* condition : conditions_) {
    if (condition->GetType() == Condition::kSyncBase)
      condition->DisconnectSyncBase(*this);
  }
}

void SVGSMILElement::ConnectEventBaseConditions() {
  DisconnectEventBaseConditions();
  for (Condition* condition : conditions_) {
    if (condition->GetType() == Condition::kEventBase)
      condition->ConnectEventBase(*this);
  }
}

void SVGSMILElement::DisconnectEventBaseConditions() {
  for (Condition* condition : conditions_) {
    if (condition->GetType() == Condition::kEventBase)
      condition->DisconnectEventBase(*this);
  }
}

void SVGSMILElement::SetTargetElement(SVGElement* target) {
  if (target == target_element_)
    return;
  WillChangeAnimationTarget();

  // If the animation state is not Inactive, always reset to a clear state
  // before leaving the old target element.
  if (GetActiveState() != kInactive)
    EndedActiveInterval();

  target_element_ = target;
  DidChangeAnimationTarget();

  // If the animation is scheduled and there's an active interval, then
  // revalidate the animation value.
  if (GetActiveState() != kInactive && is_scheduled_)
    StartedActiveInterval();
}

SMILTime SVGSMILElement::Elapsed() const {
  return time_container_ ? time_container_->Elapsed() : SMILTime();
}

SMILTime SVGSMILElement::BeginTimeForPrioritization(
    SMILTime presentation_time) const {
  if (GetActiveState() == kFrozen) {
    if (interval_.BeginsAfter(presentation_time))
      return previous_interval_.begin;
  }
  return interval_.begin;
}

SMILTime SVGSMILElement::Dur() const {
  if (cached_dur_ != kInvalidCachedTime)
    return cached_dur_;
  const AtomicString& value = FastGetAttribute(svg_names::kDurAttr);
  SMILTime clock_value = ParseClockValue(value);
  return cached_dur_ =
             clock_value <= SMILTime() ? SMILTime::Unresolved() : clock_value;
}

SMILTime SVGSMILElement::RepeatDur() const {
  if (cached_repeat_dur_ != kInvalidCachedTime)
    return cached_repeat_dur_;
  const AtomicString& value = FastGetAttribute(svg_names::kRepeatDurAttr);
  SMILTime clock_value = ParseClockValue(value);
  cached_repeat_dur_ =
      clock_value <= SMILTime() ? SMILTime::Unresolved() : clock_value;
  return cached_repeat_dur_;
}

static SMILRepeatCount ParseRepeatCount(const AtomicString& value) {
  if (value.IsNull())
    return SMILRepeatCount::Unspecified();
  if (value == "indefinite")
    return SMILRepeatCount::Indefinite();
  bool ok;
  double result = value.ToDouble(&ok);
  if (ok && result > 0)
    return SMILRepeatCount::Numeric(result);
  return SMILRepeatCount::Unspecified();
}

SMILRepeatCount SVGSMILElement::RepeatCount() const {
  if (!cached_repeat_count_.IsValid()) {
    cached_repeat_count_ =
        ParseRepeatCount(FastGetAttribute(svg_names::kRepeatCountAttr));
  }
  DCHECK(cached_repeat_count_.IsValid());
  return cached_repeat_count_;
}

SMILTime SVGSMILElement::MaxValue() const {
  if (cached_max_ != kInvalidCachedTime)
    return cached_max_;
  const AtomicString& value = FastGetAttribute(svg_names::kMaxAttr);
  SMILTime result = ParseClockValue(value);
  return cached_max_ = (result.IsUnresolved() || result <= SMILTime())
                           ? SMILTime::Indefinite()
                           : result;
}

SMILTime SVGSMILElement::MinValue() const {
  if (cached_min_ != kInvalidCachedTime)
    return cached_min_;
  const AtomicString& value = FastGetAttribute(svg_names::kMinAttr);
  SMILTime result = ParseClockValue(value);
  return cached_min_ = (result.IsUnresolved() || result < SMILTime())
                           ? SMILTime()
                           : result;
}

SMILTime SVGSMILElement::SimpleDuration() const {
  return std::min(Dur(), SMILTime::Indefinite());
}

static void InsertSortedAndUnique(Vector<SMILTimeWithOrigin>& list,
                                  SMILTimeWithOrigin time) {
  auto* position = std::lower_bound(list.begin(), list.end(), time);
  // Don't add it if we already have one of those.
  for (auto* it = position; it < list.end(); ++it) {
    if (position->Time() != time.Time())
      break;
    // If they share both time and origin, we don't need to add it,
    // we just need to react.
    if (position->HasSameOrigin(time))
      return;
  }
  list.insert(position - list.begin(), time);
}

void SVGSMILElement::AddInstanceTime(BeginOrEnd begin_or_end,
                                     SMILTime time,
                                     SMILTimeWithOrigin::Origin origin) {
  SMILTime current_presentation_time =
      time_container_ ? time_container_->CurrentDocumentTime() : SMILTime();
  DCHECK(!current_presentation_time.IsUnresolved());
  SMILTimeWithOrigin time_with_origin(time, origin);
  // Ignore new instance times for 'end' if the element is not active
  // and the origin is script.
  if (begin_or_end == kEnd && GetActiveState() == kInactive &&
      time_with_origin.OriginIsScript())
    return;
  Vector<SMILTimeWithOrigin>& list =
      begin_or_end == kBegin ? begin_times_ : end_times_;
  InsertSortedAndUnique(list, time_with_origin);
  if (begin_or_end == kBegin)
    BeginListChanged(current_presentation_time);
  else
    EndListChanged(current_presentation_time);
}

SMILTime SVGSMILElement::FindInstanceTime(BeginOrEnd begin_or_end,
                                          SMILTime minimum_time,
                                          bool equals_minimum_ok) const {
  const Vector<SMILTimeWithOrigin>& list =
      begin_or_end == kBegin ? begin_times_ : end_times_;

  if (list.IsEmpty())
    return begin_or_end == kBegin ? SMILTime::Unresolved()
                                  : SMILTime::Indefinite();

  // If an equal value is not accepted, return the next bigger item in the list,
  // if any.
  auto predicate = [equals_minimum_ok](const SMILTimeWithOrigin& instance_time,
                                       const SMILTime& time) {
    return equals_minimum_ok ? instance_time.Time() < time
                             : instance_time.Time() <= time;
  };
  auto* item =
      std::lower_bound(list.begin(), list.end(), minimum_time, predicate);
  if (item == list.end())
    return SMILTime::Unresolved();

  // The special value "indefinite" does not yield an instance time in the begin
  // list.
  if (item->Time().IsIndefinite() && begin_or_end == kBegin)
    return SMILTime::Unresolved();

  return item->Time();
}

SMILTime SVGSMILElement::RepeatingDuration() const {
  // Computing the active duration
  // http://www.w3.org/TR/SMIL2/smil-timing.html#Timing-ComputingActiveDur
  SMILRepeatCount repeat_count = RepeatCount();
  SMILTime repeat_dur = RepeatDur();
  SMILTime simple_duration = SimpleDuration();
  if (!simple_duration ||
      (repeat_dur.IsUnresolved() && repeat_count.IsUnspecified()))
    return simple_duration;
  repeat_dur = std::min(repeat_dur, SMILTime::Indefinite());
  SMILTime repeat_count_duration = simple_duration.Repeat(repeat_count);
  if (!repeat_count_duration.IsUnresolved())
    return std::min(repeat_dur, repeat_count_duration);
  return repeat_dur;
}

SMILTime SVGSMILElement::ResolveActiveEnd(SMILTime resolved_begin,
                                          SMILTime resolved_end) const {
  // Computing the active duration
  // http://www.w3.org/TR/SMIL2/smil-timing.html#Timing-ComputingActiveDur
  SMILTime preliminary_active_duration;
  if (!resolved_end.IsUnresolved() && Dur().IsUnresolved() &&
      RepeatDur().IsUnresolved() && RepeatCount().IsUnspecified())
    preliminary_active_duration = resolved_end - resolved_begin;
  else if (!resolved_end.IsFinite())
    preliminary_active_duration = RepeatingDuration();
  else
    preliminary_active_duration =
        std::min(RepeatingDuration(), resolved_end - resolved_begin);

  SMILTime min_value = MinValue();
  SMILTime max_value = MaxValue();
  if (min_value > max_value) {
    // Ignore both.
    // http://www.w3.org/TR/2001/REC-smil-animation-20010904/#MinMax
    min_value = SMILTime();
    max_value = SMILTime::Indefinite();
  }
  return resolved_begin +
         std::min(max_value, std::max(min_value, preliminary_active_duration));
}

SMILInterval SVGSMILElement::ResolveInterval(
    IntervalSelector interval_selector) const {
  bool first = interval_selector == kFirstInterval;
  // See the pseudocode in http://www.w3.org/TR/SMIL3/smil-timing.html#q90.
  SMILTime begin_after = first ? SMILTime::Earliest() : interval_.end;
  SMILTime last_interval_temp_end = SMILTime::Indefinite();
  while (true) {
    bool equals_minimum_ok = !first || interval_.end > interval_.begin;
    SMILTime temp_begin =
        FindInstanceTime(kBegin, begin_after, equals_minimum_ok);
    if (temp_begin.IsUnresolved())
      break;
    SMILTime temp_end;
    if (end_times_.IsEmpty())
      temp_end = ResolveActiveEnd(temp_begin, SMILTime::Indefinite());
    else {
      temp_end = FindInstanceTime(kEnd, temp_begin, true);
      if ((first && temp_begin == temp_end &&
           temp_end == last_interval_temp_end) ||
          (!first && temp_end == interval_.end))
        temp_end = FindInstanceTime(kEnd, temp_begin, false);
      if (temp_end.IsUnresolved()) {
        if (!end_times_.IsEmpty() && !has_end_event_conditions_)
          break;
      }
      temp_end = ResolveActiveEnd(temp_begin, temp_end);
    }
    if (!first || (temp_end > SMILTime() || (!temp_begin && !temp_end)))
      return SMILInterval(temp_begin, temp_end);

    begin_after = temp_end;
    last_interval_temp_end = temp_end;
  }
  return SMILInterval(SMILTime::Unresolved(), SMILTime::Unresolved());
}

bool SVGSMILElement::ResolveFirstInterval() {
  SMILInterval first_interval = ResolveInterval(kFirstInterval);
  DCHECK(!first_interval.begin.IsIndefinite());
  if (!first_interval.IsResolved() || first_interval == interval_)
    return false;
  interval_ = first_interval;
  NotifyDependentsIntervalChanged(interval_);
  return true;
}

base::Optional<SMILInterval> SVGSMILElement::ResolveNextInterval() {
  SMILInterval next_interval = ResolveInterval(kNextInterval);
  DCHECK(!next_interval.begin.IsIndefinite());

  if (!next_interval.begin.IsUnresolved() &&
      next_interval.begin != interval_.begin) {
    return next_interval;
  }

  return base::nullopt;
}

SMILTime SVGSMILElement::NextInterestingTime(SMILTime presentation_time) const {
  DCHECK_GE(presentation_time, SMILTime());
  SMILTime next_interesting_interval_time = SMILTime::Indefinite();
  if (interval_.BeginsAfter(presentation_time)) {
    next_interesting_interval_time = interval_.begin;
  } else if (interval_.EndsAfter(presentation_time)) {
    if (SMILTime simple_duration = SimpleDuration()) {
      SMILTime next_repeat_time = ComputeNextRepeatTime(
          interval_.begin, simple_duration, presentation_time);
      DCHECK(next_repeat_time.IsFinite());
      next_interesting_interval_time =
          std::min(next_repeat_time, interval_.end);
    } else {
      next_interesting_interval_time = interval_.end;
    }
  }
  return std::min(next_interesting_interval_time,
                  FindInstanceTime(kBegin, presentation_time, false));
}

void SVGSMILElement::BeginListChanged(SMILTime event_time) {
  if (is_waiting_for_first_interval_) {
    ResolveFirstInterval();
  } else if (GetRestart() != kRestartNever) {
    SMILTime new_begin = FindInstanceTime(kBegin, event_time, true);
    if (new_begin.IsFinite() && (interval_.EndsBefore(event_time) ||
                                 interval_.BeginsAfter(new_begin))) {
      // Begin time changed, re-resolve the interval.
      SMILTime old_begin = interval_.begin;
      interval_.end = event_time;
      interval_ = ResolveInterval(kNextInterval);
      DCHECK(interval_.IsResolved());
      if (interval_.begin != old_begin) {
        if (GetActiveState() == kActive && interval_.BeginsAfter(event_time)) {
          active_state_ = DetermineActiveState(event_time);
          if (GetActiveState() != kActive)
            EndedActiveInterval();
        }
        NotifyDependentsIntervalChanged(interval_);
      }
    }
  }

  if (time_container_)
    time_container_->NotifyIntervalsChanged();
}

void SVGSMILElement::EndListChanged(SMILTime) {
  SMILTime elapsed = Elapsed();
  if (is_waiting_for_first_interval_) {
    ResolveFirstInterval();
  } else if (interval_.IsResolved() && interval_.EndsAfter(elapsed)) {
    SMILTime new_end = FindInstanceTime(kEnd, interval_.begin, false);
    if (interval_.EndsAfter(new_end)) {
      new_end = ResolveActiveEnd(interval_.begin, new_end);
      if (new_end != interval_.end) {
        interval_.end = new_end;
        NotifyDependentsIntervalChanged(interval_);
      }
    }
  }

  if (time_container_)
    time_container_->NotifyIntervalsChanged();
}

bool SVGSMILElement::CheckAndUpdateInterval(SMILTime elapsed) {
  DCHECK(!is_waiting_for_first_interval_);
  DCHECK(interval_.BeginsBefore(elapsed));

  Restart restart = GetRestart();
  if (restart == kRestartNever)
    return false;

  base::Optional<SMILInterval> new_interval;
  if (restart == kRestartAlways && interval_.EndsAfter(elapsed)) {
    SMILTime next_begin = FindInstanceTime(kBegin, interval_.begin, false);
    if (interval_.EndsAfter(next_begin)) {
      new_interval = interval_;
      new_interval->end = next_begin;
    }
  }

  if ((new_interval && new_interval->EndsBefore(elapsed)) ||
      (!new_interval && interval_.EndsBefore(elapsed))) {
    new_interval = ResolveNextInterval();
  }

  // Track "restarts" to handle active -> active transitions.
  bool interval_restart = (new_interval && *new_interval != interval_);

  if (new_interval) {
    previous_interval_ = interval_;
    interval_ = *new_interval;
    interval_has_changed_ = true;
  }
  return interval_restart;
}

const SMILInterval& SVGSMILElement::GetActiveInterval(SMILTime elapsed) const {
  // If there's no current interval, return the previous interval.
  if (!interval_.IsResolved())
    return previous_interval_;
  // If there's a previous interval and the current interval hasn't begun yet,
  // return the previous interval.
  if (previous_interval_.IsResolved() && interval_.BeginsAfter(elapsed))
    return previous_interval_;
  return interval_;
}

unsigned SVGSMILElement::CalculateAnimationRepeat(SMILTime elapsed) const {
  const SMILTime simple_duration = SimpleDuration();
  if (!simple_duration)
    return 0;
  DCHECK(simple_duration.IsFinite());
  const SMILInterval& active_interval = GetActiveInterval(elapsed);
  DCHECK(active_interval.begin.IsFinite());

  SMILTime active_time = elapsed - active_interval.begin;
  SMILTime repeating_duration = RepeatingDuration();
  int64_t repeat;
  if (elapsed >= active_interval.end || active_time > repeating_duration) {
    if (!repeating_duration.IsFinite())
      return 0;
    repeat = repeating_duration / simple_duration;
    if (!(repeating_duration % simple_duration))
      repeat--;
  } else {
    repeat = active_time / simple_duration;
  }
  return clampTo<unsigned>(repeat);
}

float SVGSMILElement::CalculateAnimationPercent(SMILTime elapsed) const {
  SMILTime simple_duration = SimpleDuration();
  if (simple_duration.IsIndefinite()) {
    return 0.f;
  }
  if (!simple_duration) {
    return 1.f;
  }
  DCHECK(simple_duration.IsFinite());
  const SMILInterval& active_interval = GetActiveInterval(elapsed);
  DCHECK(active_interval.IsResolved());

  SMILTime repeating_duration = RepeatingDuration();
  SMILTime active_time = elapsed - active_interval.begin;
  if (elapsed >= active_interval.end || active_time > repeating_duration) {
    // Use the interval to compute the interval position if we've passed the
    // interval end, otherwise use the "repeating duration". This prevents a
    // stale interval (with for instance an 'indefinite' end) from yielding an
    // invalid interval position.
    SMILTime last_active_duration = repeating_duration;
    if (elapsed >= active_interval.end)
      last_active_duration = active_interval.end - active_interval.begin;
    double percent = last_active_duration.InternalValueAsDouble() /
                     simple_duration.InternalValueAsDouble();
    percent = percent - floor(percent);
    float epsilon = std::numeric_limits<float>::epsilon();
    if (percent < epsilon || 1 - percent < epsilon)
      return 1.0f;
    return clampTo<float>(percent);
  }
  SMILTime simple_time = active_time % simple_duration;
  return clampTo<float>(simple_time.InternalValueAsDouble() /
                        simple_duration.InternalValueAsDouble());
}

SMILTime SVGSMILElement::NextProgressTime(SMILTime presentation_time) const {
  if (GetActiveState() == kActive) {
    // If duration is indefinite the value does not actually change over time.
    // Same is true for <set>.
    SMILTime simple_duration = SimpleDuration();
    if (simple_duration.IsIndefinite() || IsSVGSetElement(*this)) {
      SMILTime repeating_duration_end = interval_.begin + RepeatingDuration();
      // We are supposed to do freeze semantics when repeating ends, even if the
      // element is still active.
      // Take care that we get a timer callback at that point.
      if (presentation_time < repeating_duration_end &&
          interval_.EndsAfter(repeating_duration_end) &&
          repeating_duration_end.IsFinite())
        return repeating_duration_end;
      return interval_.end;
    }
    return presentation_time;
  }
  return interval_.begin >= presentation_time ? interval_.begin
                                              : SMILTime::Unresolved();
}

SVGSMILElement::ActiveState SVGSMILElement::DetermineActiveState(
    SMILTime elapsed) const {
  if (interval_.Contains(elapsed))
    return kActive;
  return Fill() == kFillFreeze ? kFrozen : kInactive;
}

bool SVGSMILElement::IsContributing(SMILTime elapsed) const {
  // Animation does not contribute during the active time if it is past its
  // repeating duration and has fill=remove.
  return (GetActiveState() == kActive &&
          (Fill() == kFillFreeze ||
           elapsed <= interval_.begin + RepeatingDuration())) ||
         GetActiveState() == kFrozen;
}

// The first part of the processing of the animation,
// this checks if there are any further calculations needed
// to continue and makes sure the intervals are correct.
bool SVGSMILElement::NeedsToProgress(SMILTime elapsed) {
  // Check we're connected to something and that our conditions have been
  // "connected".
  DCHECK(time_container_);
  DCHECK(sync_base_conditions_connected_);
  // Check that we have some form of start or are prepared to find it.
  DCHECK(is_waiting_for_first_interval_ || interval_.IsResolved());

  // Check if we need updating, otherwise just return.
  if (!interval_.IsResolved()) {
    DCHECK_EQ(GetActiveState(), kInactive);
    return false;
  }

  if (interval_.BeginsAfter(elapsed)) {
    DCHECK_NE(GetActiveState(), kActive);
    return false;
  }

  if (is_waiting_for_first_interval_) {
    is_waiting_for_first_interval_ = false;
    if (ResolveFirstInterval())
      time_container_->NotifyIntervalsChanged();
  }
  return true;
}

void SVGSMILElement::UpdateSyncBases() {
  if (!interval_has_changed_)
    return;
  interval_has_changed_ = false;
  NotifyDependentsIntervalChanged(interval_);
}

void SVGSMILElement::UpdateActiveState(SMILTime elapsed,
                                       bool interval_restart) {
  ActiveState old_active_state = GetActiveState();
  active_state_ = DetermineActiveState(elapsed);

  if (IsContributing(elapsed)) {
    if (old_active_state == kInactive || interval_restart) {
      ScheduleEvent(event_type_names::kBeginEvent);
      StartedActiveInterval();
    }

    unsigned repeat = CalculateAnimationRepeat(elapsed);
    if (repeat && repeat != last_repeat_) {
      last_repeat_ = repeat;
      NotifyDependentsIntervalChanged(interval_);
      ScheduleRepeatEvents();
    }

    last_percent_ = CalculateAnimationPercent(elapsed);
  }

  if ((old_active_state == kActive && GetActiveState() != kActive) ||
      interval_restart) {
    ScheduleEvent(event_type_names::kEndEvent);
    EndedActiveInterval();
  }
}

void SVGSMILElement::NotifyDependentsIntervalChanged(
    const SMILInterval& interval) {
  DCHECK(interval.IsResolved());
  // |loopBreaker| is used to avoid infinite recursions which may be caused by:
  // |notifyDependentsIntervalChanged| -> |createInstanceTimesFromSyncBase| ->
  // |add{Begin,End}Time| -> |{begin,end}TimeChanged| ->
  // |notifyDependentsIntervalChanged|
  //
  // As the set here records SVGSMILElements on the stack, it is acceptable to
  // use a HashSet of untraced heap references -- any conservative GC which
  // strikes before unwinding will find these elements on the stack.
  DEFINE_STATIC_LOCAL(HashSet<UntracedMember<SVGSMILElement>>, loop_breaker,
                      ());
  if (!loop_breaker.insert(this).is_new_entry)
    return;

  for (SVGSMILElement* element : sync_base_dependents_)
    element->CreateInstanceTimesFromSyncBase(this, interval);

  loop_breaker.erase(this);
}

void SVGSMILElement::CreateInstanceTimesFromSyncBase(
    SVGSMILElement* timed_element,
    const SMILInterval& new_interval) {
  // FIXME: To be really correct, this should handle updating exising interval
  // by changing the associated times instead of creating new ones.
  for (Condition* condition : conditions_) {
    if (condition->IsSyncBaseFor(timed_element)) {
      // TODO(edvardt): This is a lot of string compares, which is slow, it
      // might be a good idea to change it for an enum and maybe make Condition
      // into a union?
      DCHECK(condition->GetName() == "begin" || condition->GetName() == "end" ||
             condition->GetName() == "repeat");

      // No nested time containers in SVG, no need for crazy time space
      // conversions. Phew!
      SMILTime time = SMILTime::Unresolved();
      if (condition->GetName() == "begin") {
        time = new_interval.begin + condition->Offset();
      } else if (condition->GetName() == "end") {
        time = new_interval.end + condition->Offset();
      } else if (condition->Repeat()) {
        if (timed_element->last_repeat_ == condition->Repeat()) {
          // If the STAPIT algorithm works, the current document
          // time will be accurate. So this event should be sent
          // correctly.
          SMILTime base_time = time_container_
                                   ? time_container_->CurrentDocumentTime()
                                   : SMILTime();
          time = base_time + condition->Offset();
        }
      }
      if (!time.IsFinite())
        continue;
      AddInstanceTime(condition->GetBeginOrEnd(), time);
    }
  }
}

void SVGSMILElement::AddSyncBaseDependent(SVGSMILElement& animation) {
  sync_base_dependents_.insert(&animation);
  if (interval_.IsResolved())
    animation.CreateInstanceTimesFromSyncBase(this, interval_);
}

void SVGSMILElement::RemoveSyncBaseDependent(SVGSMILElement& animation) {
  sync_base_dependents_.erase(&animation);
}

void SVGSMILElement::BeginByLinkActivation() {
  AddInstanceTime(kBegin, Elapsed());
}

void SVGSMILElement::EndedActiveInterval() {
  ClearTimesWithDynamicOrigins(begin_times_);
  ClearTimesWithDynamicOrigins(end_times_);
}

void SVGSMILElement::ScheduleRepeatEvents() {
  ScheduleEvent(event_type_names::kRepeatEvent);
  ScheduleEvent(AtomicString("repeatn"));
}

void SVGSMILElement::ScheduleEvent(const AtomicString& event_type) {
  GetDocument()
      .GetTaskRunner(TaskType::kDOMManipulation)
      ->PostTask(FROM_HERE, WTF::Bind(&SVGSMILElement::DispatchPendingEvent,
                                      WrapPersistent(this), event_type));
}

void SVGSMILElement::DispatchPendingEvent(const AtomicString& event_type) {
  DCHECK(event_type == event_type_names::kEndEvent ||
         event_type == event_type_names::kBeginEvent ||
         event_type == event_type_names::kRepeatEvent ||
         event_type == "repeatn");
  DispatchEvent(*Event::Create(event_type));
}

bool SVGSMILElement::HasValidTarget() {
  return targetElement() && targetElement()->InActiveDocument();
}

void SVGSMILElement::WillChangeAnimationTarget() {
  if (!is_scheduled_)
    return;
  DCHECK(time_container_);
  DCHECK(target_element_);
  time_container_->Unschedule(this, target_element_, attribute_name_);
  is_scheduled_ = false;
}

void SVGSMILElement::DidChangeAnimationTarget() {
  DCHECK(!is_scheduled_);
  if (!time_container_ || !HasValidTarget())
    return;
  time_container_->Schedule(this, target_element_, attribute_name_);
  is_scheduled_ = true;
}

const AttrNameToTrustedType& SVGSMILElement::GetCheckedAttributeTypes() const {
  return SVGURIReference::GetCheckedAttributeTypes();
}

void SVGSMILElement::Trace(blink::Visitor* visitor) {
  visitor->Trace(target_element_);
  visitor->Trace(target_id_observer_);
  visitor->Trace(time_container_);
  visitor->Trace(conditions_);
  visitor->Trace(sync_base_dependents_);
  SVGElement::Trace(visitor);
  SVGTests::Trace(visitor);
}

}  // namespace blink
