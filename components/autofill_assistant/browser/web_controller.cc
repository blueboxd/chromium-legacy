// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/web_controller.h"

#include <math.h>
#include <algorithm>
#include <ctime>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill_assistant/browser/client_settings.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/rectf.h"
#include "components/autofill_assistant/browser/string_conversions_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

namespace autofill_assistant {
using autofill::ContentAutofillDriver;

namespace {
const char* const kGetBoundingClientRectAsList =
    R"(function(node) {
      const r = node.getBoundingClientRect();
      return [window.scrollX + r.left,
              window.scrollY + r.top,
              window.scrollX + r.right,
              window.scrollY + r.bottom];
    })";

const char* const kGetVisualViewport =
    R"({ const v = window.visualViewport;
         [v.pageLeft,
          v.pageTop,
          v.width,
          v.height] })";

// Scrolls to the specified node with top padding. The top padding can
// be specified through pixels or ratio. Pixels take precedence.
const char* const kScrollIntoViewWithPaddingScript =
    R"(function(node, topPaddingPixels, topPaddingRatio) {
    node.scrollIntoViewIfNeeded();
    const rect = node.getBoundingClientRect();
    let topPadding = topPaddingPixels;
    if (!topPadding){
      topPadding = window.innerHeight * topPaddingRatio;
    }
    window.scrollBy({top: rect.top - topPadding});
  })";

// Scroll the window or any scrollable container as needed for the element to
// appear centered. This is in preparation of a click, to improve the chances
// for the element to click to be visible.
const char* const kScrollIntoViewCenterScript =
    R"(function(node) {
    node.scrollIntoView({block: "center", inline: "center"});
  })";

const char* const kScrollIntoViewIfNeededScript =
    R"(function(node) {
    node.scrollIntoViewIfNeeded();
  })";

// Javascript to select a value from a select box. Also fires a "change" event
// to trigger any listeners. Changing the index directly does not trigger this.
const char* const kSelectOptionScript =
    R"(function(value) {
      const uppercaseValue = value.toUpperCase();
      var found = false;
      for (var i = 0; i < this.options.length; ++i) {
        const label = this.options[i].label.toUpperCase();
        if (label.length > 0 && label.startsWith(uppercaseValue)) {
          this.options.selectedIndex = i;
          found = true;
          break;
        }
      }
      if (!found) {
        return false;
      }
      const e = document.createEvent('HTMLEvents');
      e.initEvent('change', true, true);
      this.dispatchEvent(e);
      return true;
    })";

// Javascript to highlight an element.
const char* const kHighlightElementScript =
    R"(function() {
      this.style.boxShadow = '0px 0px 0px 3px white, ' +
          '0px 0px 0px 6px rgb(66, 133, 244)';
      return true;
    })";

// Javascript code to retrieve the 'value' attribute of a node.
const char* const kGetValueAttributeScript =
    "function () { return this.value; }";

// Javascript code to set the 'value' attribute of a node and then fire a
// "change" event to trigger any listeners.
const char* const kSetValueAttributeScript =
    R"(function (value) {
         this.value = value;
         const e = document.createEvent('HTMLEvents');
         e.initEvent('change', true, true);
         this.dispatchEvent(e);
       })";

// Javascript code to set an attribute of a node to a given value.
const char* const kSetAttributeScript =
    R"(function (attribute, value) {
         let receiver = this;
         for (let i = 0; i < attribute.length - 1; i++) {
           receiver = receiver[attribute[i]];
         }
         receiver[attribute[attribute.length - 1]] = value;
       })";

// Javascript code to get the outerHTML of a node.
// TODO(crbug.com/806868): Investigate if using DOM.GetOuterHtml would be a
// better solution than injecting Javascript code.
const char* const kGetOuterHtmlScript =
    "function () { return this.outerHTML; }";

// Javascript code to get document root element.
const char* const kGetDocumentElement =
    R"(
    (function() {
      return document.documentElement;
    }())
    )";

// Javascript code to query an elements for a selector, either the first
// (non-strict) or a single (strict) element.
//
// Returns undefined if no elements are found, TOO_MANY_ELEMENTS (18) if too
// many elements were found and strict mode is enabled.
const char* const kQuerySelector =
    R"(function (selector, strictMode) {
      var found = this.querySelectorAll(selector);
      if(found.length == 0) return undefined;
      if(found.length > 1 && strictMode) return 18;
      return found[0];
    })";

// Javascript code to query a visible elements for a selector, either the first
// (non-strict) or a single (strict) visible element.q
//
// Returns undefined if no elements are found, TOO_MANY_ELEMENTS (18) if too
// many elements were found and strict mode is enabled.
const char* const kQuerySelectorWithConditions =
    R"(function (selector, strict, visible, inner_text_str, value_str) {
        var found = this.querySelectorAll(selector);
        var found_index = -1;
        var inner_text_re = inner_text_str ? RegExp(inner_text_str) : undefined;
        var value_re = value_str ? RegExp(value_str) : undefined;
        var match = function(e) {
          if (visible && e.getClientRects().length == 0) return false;
          if (inner_text_re && !inner_text_re.test(e.innerText)) return false;
          if (value_re && !value_re.test(e.value)) return false;
          return true;
        };
        for (let i = 0; i < found.length; i++) {
          if (match(found[i])) {
            if (found_index != -1) return 18;
            found_index = i;
            if (!strict) break;
          }
        }
        return found_index == -1 ? undefined : found[found_index];
    })";

// Javascript code to query whether the document is ready for interact.
const char* const kIsDocumentReadyForInteract =
    R"(function () {
      return document.readyState == 'interactive'
          || document.readyState == 'complete';
    })";

// Javascript code to click on an element.
const char* const kClickElement =
    R"(function (selector) {
      selector.click();
    })";

// Javascript code that returns a promise that will succeed once the main
// document window has changed height.
//
// This ignores width changes, to filter out resizes caused by changes to the
// screen orientation.
const char* const kWaitForWindowHeightChange = R"(
new Promise((fulfill, reject) => {
  var lastWidth = window.innerWidth;
  var handler = function(event) {
    if (window.innerWidth != lastWidth) {
      lastWidth = window.innerWidth;
      return
    }
    window.removeEventListener('resize', handler)
    fulfill(true)
  }
  window.addEventListener('resize', handler)
})
)";

bool ConvertPseudoType(const PseudoType pseudo_type,
                       dom::PseudoType* pseudo_type_output) {
  switch (pseudo_type) {
    case PseudoType::UNDEFINED:
      break;
    case PseudoType::FIRST_LINE:
      *pseudo_type_output = dom::PseudoType::FIRST_LINE;
      return true;
    case PseudoType::FIRST_LETTER:
      *pseudo_type_output = dom::PseudoType::FIRST_LETTER;
      return true;
    case PseudoType::BEFORE:
      *pseudo_type_output = dom::PseudoType::BEFORE;
      return true;
    case PseudoType::AFTER:
      *pseudo_type_output = dom::PseudoType::AFTER;
      return true;
    case PseudoType::BACKDROP:
      *pseudo_type_output = dom::PseudoType::BACKDROP;
      return true;
    case PseudoType::SELECTION:
      *pseudo_type_output = dom::PseudoType::SELECTION;
      return true;
    case PseudoType::FIRST_LINE_INHERITED:
      *pseudo_type_output = dom::PseudoType::FIRST_LINE_INHERITED;
      return true;
    case PseudoType::SCROLLBAR:
      *pseudo_type_output = dom::PseudoType::SCROLLBAR;
      return true;
    case PseudoType::SCROLLBAR_THUMB:
      *pseudo_type_output = dom::PseudoType::SCROLLBAR_THUMB;
      return true;
    case PseudoType::SCROLLBAR_BUTTON:
      *pseudo_type_output = dom::PseudoType::SCROLLBAR_BUTTON;
      return true;
    case PseudoType::SCROLLBAR_TRACK:
      *pseudo_type_output = dom::PseudoType::SCROLLBAR_TRACK;
      return true;
    case PseudoType::SCROLLBAR_TRACK_PIECE:
      *pseudo_type_output = dom::PseudoType::SCROLLBAR_TRACK_PIECE;
      return true;
    case PseudoType::SCROLLBAR_CORNER:
      *pseudo_type_output = dom::PseudoType::SCROLLBAR_CORNER;
      return true;
    case PseudoType::RESIZER:
      *pseudo_type_output = dom::PseudoType::RESIZER;
      return true;
    case PseudoType::INPUT_LIST_BUTTON:
      *pseudo_type_output = dom::PseudoType::INPUT_LIST_BUTTON;
      return true;
  }
  return false;
}

// Builds a ClientStatus appropriate for an unexpected error.
//
// This should only be used in situations where getting an error cannot be
// anything but a bug in the client.
ClientStatus UnexpectedErrorStatus(const std::string& file, int line) {
  ClientStatus status(OTHER_ACTION_STATUS);
  auto* info = status.mutable_details()->mutable_unexpected_error_info();
  info->set_source_file(file);
  info->set_source_line_number(line);
  return status;
}

// Builds a ClientStatus appropriate for a JavaScript error.
ClientStatus JavaScriptErrorStatus(const std::string& file,
                                   int line,
                                   const runtime::ExceptionDetails* exception) {
  ClientStatus status(UNEXPECTED_JS_ERROR);
  auto* info = status.mutable_details()->mutable_unexpected_error_info();
  info->set_source_file(file);
  info->set_source_line_number(line);
  if (exception) {
    if (exception->HasException() &&
        exception->GetException()->HasClassName()) {
      info->set_js_exception_classname(
          exception->GetException()->GetClassName());
    }
    info->set_js_exception_line_number(exception->GetLineNumber());
    info->set_js_exception_column_number(exception->GetColumnNumber());
  }
  return status;
}

// Makes sure that the given EvaluateResult exists, is successful and contain a
// result.
template <typename T>
ClientStatus CheckJavaScriptResult(T* result, const char* file, int line) {
  if (!result)
    return JavaScriptErrorStatus(file, line, nullptr);
  if (result->HasExceptionDetails())
    return JavaScriptErrorStatus(file, line, result->GetExceptionDetails());
  if (!result->GetResult())
    return JavaScriptErrorStatus(file, line, nullptr);
  return OkClientStatus();
}

// Safely gets an object id from a RemoteObject
bool SafeGetObjectId(const runtime::RemoteObject* result, std::string* out) {
  if (result && result->HasObjectId()) {
    *out = result->GetObjectId();
    return true;
  }
  return false;
}

// Safely gets a string value from a RemoteObject
bool SafeGetStringValue(const runtime::RemoteObject* result, std::string* out) {
  if (result && result->HasValue() && result->GetValue()->is_string()) {
    *out = result->GetValue()->GetString();
    return true;
  }
  return false;
}

// Safely gets a int value from a RemoteObject.
bool SafeGetIntValue(const runtime::RemoteObject* result, int* out) {
  if (result && result->HasValue() && result->GetValue()->is_int()) {
    *out = result->GetValue()->GetInt();
    return true;
  }
  *out = 0;
  return false;
}

// Safely gets a boolean value from a RemoteObject
bool SafeGetBool(const runtime::RemoteObject* result, bool* out) {
  if (result && result->HasValue() && result->GetValue()->is_bool()) {
    *out = result->GetValue()->GetBool();
    return true;
  }
  *out = false;
  return false;
}

// Converts a int that correspond to the DocumentReadyState enum into an
// equivalent quoted Javascript string.
std::string DocumentReadyStateToQuotedJsString(int state) {
  switch (static_cast<DocumentReadyState>(state)) {
    case DOCUMENT_UNKNOWN_READY_STATE:
      return "''";
    case DOCUMENT_UNINITIALIZED:
      return "'uninitialized'";
    case DOCUMENT_LOADING:
      return "'loading'";
    case DOCUMENT_LOADED:
      return "'loaded'";
    case DOCUMENT_INTERACTIVE:
      return "'interactive'";
    case DOCUMENT_COMPLETE:
      return "'complete'";

      // No default, to get a compilation error if a new enum value is left
      // unsupported.
  }

  // If the enum values aren't sequential, just add empty strings to fill in the
  // blanks.
  return "''";
}

// Appends to |out| the definition of a function that'll wait for a
// ready state, expressed as a DocumentReadyState enum value.
void AppendWaitForDocumentReadyStateFunction(std::string* out) {
  // quoted_names covers all possible DocumentReadyState values.
  std::vector<std::string> quoted_names(DOCUMENT_MAX_READY_STATE + 1);
  for (int i = 0; i <= DOCUMENT_MAX_READY_STATE; i++) {
    quoted_names[i] = DocumentReadyStateToQuotedJsString(i);
  }
  base::StrAppend(out, {R"(function (minReadyStateNum) {
  return new Promise((fulfill, reject) => {
    let handler = function(event) {
      let readyState = document.readyState;
      let readyStates = [)",
                        base::JoinString(quoted_names, ", "), R"(];
      let readyStateNum = readyStates.indexOf(readyState);
      if (readyStateNum == -1) readyStateNum = 0;
      if (readyStateNum >= minReadyStateNum) {
        document.removeEventListener('readystatechange', handler);
        fulfill(readyStateNum);
      }
    }
    document.addEventListener('readystatechange', handler)
    handler();
  })
})"});
}

// Forward the result of WaitForDocumentReadyState to the callback. The same
// code work on both EvaluateResult and CallFunctionOnResult.
template <typename T>
void OnWaitForDocumentReadyState(
    base::OnceCallback<void(const ClientStatus&, DocumentReadyState)> callback,
    std::unique_ptr<T> result) {
  ClientStatus status = CheckJavaScriptResult(result.get(), __FILE__, __LINE__);
  DVLOG_IF(1, !status.ok())
      << __func__ << " Failed to get document ready state.";
  int ready_state;
  SafeGetIntValue(result->GetResult(), &ready_state);
  std::move(callback).Run(status, static_cast<DocumentReadyState>(ready_state));
}
}  // namespace

class WebController::Worker {
 public:
  Worker();
  virtual ~Worker();

 private:
  DISALLOW_COPY_AND_ASSIGN(Worker);
};
WebController::Worker::Worker() = default;
WebController::Worker::~Worker() = default;

class WebController::ElementPositionGetter : public WebController::Worker {
 public:
  ElementPositionGetter(const ClientSettings& settings);
  ~ElementPositionGetter() override;

  // |devtools_client| must be valid for the lifetime of the instance, which is
  // guaranteed since workers are owned by WebController.
  void Start(content::RenderFrameHost* frame_host,
             DevtoolsClient* devtools_client,
             std::string element_object_id,
             ElementPositionCallback callback);

 private:
  void OnVisualStateUpdatedCallback(bool success);
  void GetAndWaitBoxModelStable();
  void OnGetBoxModelForStableCheck(
      std::unique_ptr<dom::GetBoxModelResult> result);
  void OnScrollIntoView(std::unique_ptr<runtime::CallFunctionOnResult> result);
  void OnResult(int x, int y);
  void OnError();

  // Time to wait between two box model checks.
  const base::TimeDelta check_interval_;
  // Maximum number of checks to run.
  const int max_rounds_;

  DevtoolsClient* devtools_client_ = nullptr;
  std::string object_id_;
  int remaining_rounds_ = 0;
  ElementPositionCallback callback_;
  bool visual_state_updated_ = false;

  // If |has_point_| is true, |point_x_| and |point_y_| contain the last
  // computed center of the element, in viewport coordinates. Note that
  // negative coordinates are valid, in case the element is above or to the
  // left of the viewport.
  bool has_point_ = false;
  int point_x_ = 0;
  int point_y_ = 0;

  base::WeakPtrFactory<ElementPositionGetter> weak_ptr_factory_;
};

WebController::ElementPositionGetter::ElementPositionGetter(
    const ClientSettings& settings)
    : check_interval_(settings.box_model_check_interval),
      max_rounds_(settings.box_model_check_count),
      weak_ptr_factory_(this) {}

WebController::ElementPositionGetter::~ElementPositionGetter() = default;

void WebController::ElementPositionGetter::Start(
    content::RenderFrameHost* frame_host,
    DevtoolsClient* devtools_client,
    std::string element_object_id,
    ElementPositionCallback callback) {
  devtools_client_ = devtools_client;
  object_id_ = element_object_id;
  callback_ = std::move(callback);
  remaining_rounds_ = max_rounds_;
  // TODO(crbug/806868): Consider using autofill_assistant::RetryTimer

  // Wait for a roundtrips through the renderer and compositor pipeline,
  // otherwise touch event may be dropped because of missing handler.
  // Note that mouse left button will always be send to the renderer, but it
  // is slightly better to wait for the changes, like scroll, to be visualized
  // in compositor as real interaction.
  frame_host->InsertVisualStateCallback(base::BindOnce(
      &WebController::ElementPositionGetter::OnVisualStateUpdatedCallback,
      weak_ptr_factory_.GetWeakPtr()));
  GetAndWaitBoxModelStable();
}

void WebController::ElementPositionGetter::OnVisualStateUpdatedCallback(
    bool success) {
  if (success) {
    visual_state_updated_ = true;
    return;
  }

  OnError();
}

void WebController::ElementPositionGetter::GetAndWaitBoxModelStable() {
  devtools_client_->GetDOM()->GetBoxModel(
      dom::GetBoxModelParams::Builder().SetObjectId(object_id_).Build(),
      base::BindOnce(
          &WebController::ElementPositionGetter::OnGetBoxModelForStableCheck,
          weak_ptr_factory_.GetWeakPtr()));
}

void WebController::ElementPositionGetter::OnGetBoxModelForStableCheck(
    std::unique_ptr<dom::GetBoxModelResult> result) {
  if (!result || !result->GetModel() || !result->GetModel()->GetContent()) {
    DVLOG(1) << __func__ << " Failed to get box model.";
    OnError();
    return;
  }

  // Return the center of the element.
  const std::vector<double>* content_box = result->GetModel()->GetContent();
  DCHECK_EQ(content_box->size(), 8u);
  int new_point_x =
      round((round((*content_box)[0]) + round((*content_box)[2])) * 0.5);
  int new_point_y =
      round((round((*content_box)[3]) + round((*content_box)[5])) * 0.5);

  // Wait for at least three rounds (~600ms = 3*check_interval_) for visual
  // state update callback since it might take longer time to return or never
  // return if no updates.
  DCHECK(max_rounds_ > 2 && max_rounds_ >= remaining_rounds_);
  if (has_point_ && new_point_x == point_x_ && new_point_y == point_y_ &&
      (visual_state_updated_ || remaining_rounds_ + 2 < max_rounds_)) {
    // Note that there is still a chance that the element's position has been
    // changed after the last call of GetBoxModel, however, it might be safe to
    // assume the element's position will not be changed before issuing click or
    // tap event after stable for check_interval_. In addition, checking again
    // after issuing click or tap event doesn't help since the change may be
    // expected.
    OnResult(new_point_x, new_point_y);
    return;
  }

  if (remaining_rounds_ <= 0) {
    OnError();
    return;
  }

  bool is_first_round = !has_point_;
  has_point_ = true;
  point_x_ = new_point_x;
  point_y_ = new_point_y;

  // Scroll the element into view again if it was moved out of view, starting
  // from the second round.
  if (!is_first_round) {
    std::vector<std::unique_ptr<runtime::CallArgument>> argument;
    argument.emplace_back(
        runtime::CallArgument::Builder().SetObjectId(object_id_).Build());
    devtools_client_->GetRuntime()->CallFunctionOn(
        runtime::CallFunctionOnParams::Builder()
            .SetObjectId(object_id_)
            .SetArguments(std::move(argument))
            .SetFunctionDeclaration(std::string(kScrollIntoViewIfNeededScript))
            .SetReturnByValue(true)
            .Build(),
        base::BindOnce(&WebController::ElementPositionGetter::OnScrollIntoView,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  --remaining_rounds_;
  base::PostDelayedTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(
          &WebController::ElementPositionGetter::GetAndWaitBoxModelStable,
          weak_ptr_factory_.GetWeakPtr()),
      check_interval_);
}

void WebController::ElementPositionGetter::OnScrollIntoView(
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  ClientStatus status = CheckJavaScriptResult(result.get(), __FILE__, __LINE__);
  if (!status.ok()) {
    DVLOG(1) << __func__ << " Failed to scroll the element: " << status;
    OnError();
    return;
  }

  --remaining_rounds_;
  base::PostDelayedTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(
          &WebController::ElementPositionGetter::GetAndWaitBoxModelStable,
          weak_ptr_factory_.GetWeakPtr()),
      check_interval_);
}

void WebController::ElementPositionGetter::OnResult(int x, int y) {
  if (callback_) {
    std::move(callback_).Run(/* success= */ true, x, y);
  }
}

void WebController::ElementPositionGetter::OnError() {
  if (callback_) {
    std::move(callback_).Run(/* success= */ false, /* x= */ 0, /* y= */ 0);
  }
}

class WebController::ElementFinder : public WebController::Worker {
 public:
  // |devtools_client| and |web_contents| must be valid for the lifetime of the
  // instance, which is guaranteed since workers are owned by WebController.
  ElementFinder(content::WebContents* web_contents_,
                DevtoolsClient* devtools_client,
                const Selector& selector,
                bool strict);
  ~ElementFinder() override;

  // Finds the element and call the callback.
  void Start(FindElementCallback callback_);

 private:
  void SendResult(const ClientStatus& status);
  void OnGetDocumentElement(std::unique_ptr<runtime::EvaluateResult> result);
  void RecursiveFindElement(const std::string& object_id, size_t index);
  void OnQuerySelectorAll(
      size_t index,
      std::unique_ptr<runtime::CallFunctionOnResult> result);
  void OnDescribeNodeForPseudoElement(
      dom::PseudoType pseudo_type,
      std::unique_ptr<dom::DescribeNodeResult> result);
  void OnResolveNodeForPseudoElement(
      std::unique_ptr<dom::ResolveNodeResult> result);
  void OnDescribeNode(const std::string& object_id,
                      size_t index,
                      std::unique_ptr<dom::DescribeNodeResult> result);
  void OnResolveNode(size_t index,
                     std::unique_ptr<dom::ResolveNodeResult> result);
  content::RenderFrameHost* FindCorrespondingRenderFrameHost(
      std::string name,
      std::string document_url);

  content::WebContents* const web_contents_;
  DevtoolsClient* const devtools_client_;
  const Selector selector_;
  const bool strict_;
  FindElementCallback callback_;
  std::unique_ptr<FindElementResult> element_result_;

  base::WeakPtrFactory<ElementFinder> weak_ptr_factory_;
};

WebController::ElementFinder::ElementFinder(content::WebContents* web_contents,
                                            DevtoolsClient* devtools_client,
                                            const Selector& selector,
                                            bool strict)
    : web_contents_(web_contents),
      devtools_client_(devtools_client),
      selector_(selector),
      strict_(strict),
      element_result_(std::make_unique<FindElementResult>()),
      weak_ptr_factory_(this) {}

WebController::ElementFinder::~ElementFinder() = default;

void WebController::ElementFinder::Start(FindElementCallback callback) {
  callback_ = std::move(callback);

  if (selector_.empty()) {
    SendResult(ClientStatus(INVALID_SELECTOR));
    return;
  }
  devtools_client_->GetRuntime()->Evaluate(
      std::string(kGetDocumentElement),
      base::BindOnce(&WebController::ElementFinder::OnGetDocumentElement,
                     weak_ptr_factory_.GetWeakPtr()));
}

void WebController::ElementFinder::SendResult(const ClientStatus& status) {
  DCHECK(callback_);
  DCHECK(element_result_);
  std::move(callback_).Run(status, std::move(element_result_));
}

void WebController::ElementFinder::OnGetDocumentElement(
    std::unique_ptr<runtime::EvaluateResult> result) {
  ClientStatus status = CheckJavaScriptResult(result.get(), __FILE__, __LINE__);
  if (!status.ok()) {
    DVLOG(1) << __func__ << " Failed to get document root element.";
    SendResult(status);
    return;
  }
  std::string object_id;
  if (!SafeGetObjectId(result->GetResult(), &object_id)) {
    DVLOG(1) << __func__ << " Failed to get document root element.";
    SendResult(ClientStatus(ELEMENT_RESOLUTION_FAILED));
    return;
  }
  element_result_->container_frame_host = web_contents_->GetMainFrame();
  element_result_->container_frame_selector_index = 0;
  element_result_->object_id = "";
  RecursiveFindElement(object_id, 0);
}

void WebController::ElementFinder::RecursiveFindElement(
    const std::string& object_id,
    size_t index) {
  std::vector<std::unique_ptr<runtime::CallArgument>> argument;
  argument.emplace_back(runtime::CallArgument::Builder()
                            .SetValue(base::Value::ToUniquePtrValue(
                                base::Value(selector_.selectors[index])))
                            .Build());
  // For finding intermediate elements, strict mode would be more appropriate,
  // as long as the logic does not support more than one intermediate match.
  //
  // TODO(b/129387787): first, add logging to figure out whether it matters and
  // decide between strict mode and full support for multiple matching
  // intermeditate elements.
  argument.emplace_back(
      runtime::CallArgument::Builder()
          .SetValue(base::Value::ToUniquePtrValue(base::Value(strict_)))
          .Build());
  std::string function;
  if (index == (selector_.selectors.size() - 1)) {
    if (selector_.must_be_visible || !selector_.inner_text_pattern.empty() ||
        !selector_.value_pattern.empty()) {
      function.assign(kQuerySelectorWithConditions);
      argument.emplace_back(runtime::CallArgument::Builder()
                                .SetValue(base::Value::ToUniquePtrValue(
                                    base::Value(selector_.must_be_visible)))
                                .Build());
      argument.emplace_back(runtime::CallArgument::Builder()
                                .SetValue(base::Value::ToUniquePtrValue(
                                    base::Value(selector_.inner_text_pattern)))
                                .Build());
      argument.emplace_back(runtime::CallArgument::Builder()
                                .SetValue(base::Value::ToUniquePtrValue(
                                    base::Value(selector_.value_pattern)))
                                .Build());
    }
  }
  if (function.empty()) {
    function.assign(kQuerySelector);
  }
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(object_id)
          .SetArguments(std::move(argument))
          .SetFunctionDeclaration(function)
          .Build(),
      base::BindOnce(&WebController::ElementFinder::OnQuerySelectorAll,
                     weak_ptr_factory_.GetWeakPtr(), index));
}

void WebController::ElementFinder::OnQuerySelectorAll(
    size_t index,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  if (!result) {
    // It is possible for a document element to already exist, but not be
    // available yet to query because the document hasn't been loaded. This
    // results in OnQuerySelectorAll getting a nullptr result. For this specific
    // call, it is expected.
    DVLOG(1) << __func__ << ": Context doesn't exist yet to query selector "
             << index << " of " << selector_;
    SendResult(ClientStatus(ELEMENT_RESOLUTION_FAILED));
    return;
  }
  ClientStatus status = CheckJavaScriptResult(result.get(), __FILE__, __LINE__);
  if (!status.ok()) {
    DVLOG(1) << __func__ << ": Failed to query selector " << index << " of "
             << selector_;
    SendResult(status);
    return;
  }
  int int_result;
  if (SafeGetIntValue(result->GetResult(), &int_result)) {
    DCHECK(int_result == TOO_MANY_ELEMENTS);
    SendResult(ClientStatus(TOO_MANY_ELEMENTS));
    return;
  }
  std::string object_id;
  if (!SafeGetObjectId(result->GetResult(), &object_id)) {
    SendResult(ClientStatus(ELEMENT_RESOLUTION_FAILED));
    return;
  }

  if (selector_.selectors.size() == index + 1) {
    // The pseudo type is associated to the final element matched by
    // |selector_|, which means that we currently don't handle matching an
    // element inside a pseudo element.
    if (selector_.pseudo_type == PseudoType::UNDEFINED) {
      // Return object id of the element.
      element_result_->object_id = object_id;
      SendResult(OkClientStatus());
      return;
    }

    // We are looking for a pseudo element associated with this element.
    dom::PseudoType pseudo_type;
    if (!ConvertPseudoType(selector_.pseudo_type, &pseudo_type)) {
      // Return empty result.
      SendResult(ClientStatus(INVALID_ACTION));
      return;
    }

    devtools_client_->GetDOM()->DescribeNode(
        dom::DescribeNodeParams::Builder().SetObjectId(object_id).Build(),
        base::BindOnce(
            &WebController::ElementFinder::OnDescribeNodeForPseudoElement,
            weak_ptr_factory_.GetWeakPtr(), pseudo_type));
    return;
  }

  devtools_client_->GetDOM()->DescribeNode(
      dom::DescribeNodeParams::Builder().SetObjectId(object_id).Build(),
      base::BindOnce(&WebController::ElementFinder::OnDescribeNode,
                     weak_ptr_factory_.GetWeakPtr(), object_id, index));
}

void WebController::ElementFinder::OnDescribeNodeForPseudoElement(
    dom::PseudoType pseudo_type,
    std::unique_ptr<dom::DescribeNodeResult> result) {
  if (!result || !result->GetNode()) {
    DVLOG(1) << __func__ << " Failed to describe the node for pseudo element.";
    SendResult(UnexpectedErrorStatus(__FILE__, __LINE__));
    return;
  }

  auto* node = result->GetNode();
  if (node->HasPseudoElements()) {
    for (const auto& pseudo_element : *(node->GetPseudoElements())) {
      if (pseudo_element->HasPseudoType() &&
          pseudo_element->GetPseudoType() == pseudo_type) {
        devtools_client_->GetDOM()->ResolveNode(
            dom::ResolveNodeParams::Builder()
                .SetBackendNodeId(pseudo_element->GetBackendNodeId())
                .Build(),
            base::BindOnce(
                &WebController::ElementFinder::OnResolveNodeForPseudoElement,
                weak_ptr_factory_.GetWeakPtr()));
        return;
      }
    }
  }

  // Failed to find the pseudo element: run the callback with empty result.
  SendResult(ClientStatus(ELEMENT_RESOLUTION_FAILED));
}

void WebController::ElementFinder::OnResolveNodeForPseudoElement(
    std::unique_ptr<dom::ResolveNodeResult> result) {
  if (result && result->GetObject() && result->GetObject()->HasObjectId()) {
    element_result_->object_id = result->GetObject()->GetObjectId();
  }
  SendResult(OkClientStatus());
}

void WebController::ElementFinder::OnDescribeNode(
    const std::string& object_id,
    size_t index,
    std::unique_ptr<dom::DescribeNodeResult> result) {
  if (!result || !result->GetNode()) {
    DVLOG(1) << __func__ << " Failed to describe the node.";
    SendResult(UnexpectedErrorStatus(__FILE__, __LINE__));
    return;
  }

  auto* node = result->GetNode();
  std::vector<int> backend_ids;
  if (node->HasContentDocument()) {
    backend_ids.emplace_back(node->GetContentDocument()->GetBackendNodeId());

    element_result_->container_frame_selector_index = index;

    // Find out the corresponding render frame host through document url and
    // name.
    // TODO(crbug.com/806868): Use more attributes to find out the render frame
    // host if name and document url are not enough to uniquely identify it.
    std::string frame_name;
    if (node->HasAttributes()) {
      const std::vector<std::string>* attributes = node->GetAttributes();
      for (size_t i = 0; i < attributes->size();) {
        if ((*attributes)[i] == "name") {
          frame_name = (*attributes)[i + 1];
          break;
        }
        // Jump two positions since attribute name and value are always paired.
        i = i + 2;
      }
    }
    element_result_->container_frame_host = FindCorrespondingRenderFrameHost(
        frame_name, node->GetContentDocument()->GetDocumentURL());
    if (!element_result_->container_frame_host) {
      DVLOG(1) << __func__ << " Failed to find corresponding owner frame.";
      SendResult(UnexpectedErrorStatus(__FILE__, __LINE__));
      return;
    }
  } else if (node->HasFrameId()) {
    // TODO(crbug.com/806868): Support out-of-process iframe.
    DVLOG(3) << "Warning (unsupported): the element is inside an OOPIF.";
    SendResult(ClientStatus(UNSUPPORTED));
    return;
  }

  if (node->HasShadowRoots()) {
    // TODO(crbug.com/806868): Support multiple shadow roots.
    backend_ids.emplace_back(
        node->GetShadowRoots()->front()->GetBackendNodeId());
  }

  if (!backend_ids.empty()) {
    devtools_client_->GetDOM()->ResolveNode(
        dom::ResolveNodeParams::Builder()
            .SetBackendNodeId(backend_ids[0])
            .Build(),
        base::BindOnce(&WebController::ElementFinder::OnResolveNode,
                       weak_ptr_factory_.GetWeakPtr(), index));
    return;
  }

  RecursiveFindElement(object_id, ++index);
}

void WebController::ElementFinder::OnResolveNode(
    size_t index,
    std::unique_ptr<dom::ResolveNodeResult> result) {
  if (!result || !result->GetObject() || !result->GetObject()->HasObjectId()) {
    DVLOG(1) << __func__ << " Failed to resolve object id from backend id.";
    SendResult(UnexpectedErrorStatus(__FILE__, __LINE__));
    return;
  }

  RecursiveFindElement(result->GetObject()->GetObjectId(), ++index);
}

content::RenderFrameHost*
WebController::ElementFinder::FindCorrespondingRenderFrameHost(
    std::string name,
    std::string document_url) {
  content::RenderFrameHost* ret_frame = nullptr;
  for (auto* frame : web_contents_->GetAllFrames()) {
    if (frame->GetFrameName() == name &&
        frame->GetLastCommittedURL().spec() == document_url) {
      DCHECK(!ret_frame);
      ret_frame = frame;
    }
  }

  return ret_frame;
}

// static
std::unique_ptr<WebController> WebController::CreateForWebContents(
    content::WebContents* web_contents,
    const ClientSettings* settings) {
  return std::make_unique<WebController>(
      web_contents,
      std::make_unique<DevtoolsClient>(
          content::DevToolsAgentHost::GetOrCreateFor(web_contents)),
      settings);
}

WebController::WebController(content::WebContents* web_contents,
                             std::unique_ptr<DevtoolsClient> devtools_client,
                             const ClientSettings* settings)
    : web_contents_(web_contents),
      devtools_client_(std::move(devtools_client)),
      settings_(settings),
      weak_ptr_factory_(this) {}

WebController::~WebController() {}

WebController::FillFormInputData::FillFormInputData() {}

WebController::FillFormInputData::~FillFormInputData() {}

void WebController::LoadURL(const GURL& url) {
  DVLOG(3) << __func__ << " " << url;
  web_contents_->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(url));
}

void WebController::ClickOrTapElement(
    const Selector& selector,
    ClickAction::ClickType click_type,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  DVLOG(3) << __func__ << " " << selector;
  DCHECK(!selector.empty());
  FindElement(selector,
              /* strict_mode= */ true,
              base::BindOnce(&WebController::OnFindElementForClickOrTap,
                             weak_ptr_factory_.GetWeakPtr(),
                             std::move(callback), click_type));
}

void WebController::OnFindElementForClickOrTap(
    base::OnceCallback<void(const ClientStatus&)> callback,
    ClickAction::ClickType click_type,
    const ClientStatus& status,
    std::unique_ptr<FindElementResult> result) {
  // Found element must belong to a frame.
  if (!status.ok()) {
    DVLOG(1) << __func__ << " Failed to find the element to click or tap.";
    std::move(callback).Run(status);
    return;
  }

  std::string element_object_id = result->object_id;
  WaitForDocumentToBecomeInteractive(
      settings_->document_ready_check_count, element_object_id,
      base::BindOnce(
          &WebController::OnWaitDocumentToBecomeInteractiveForClickOrTap,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback), click_type,
          std::move(result)));
}

void WebController::OnWaitDocumentToBecomeInteractiveForClickOrTap(
    base::OnceCallback<void(const ClientStatus&)> callback,
    ClickAction::ClickType click_type,
    std::unique_ptr<FindElementResult> target_element,
    bool result) {
  if (!result) {
    std::move(callback).Run(ClientStatus(TIMED_OUT));
    return;
  }

  ClickOrTapElement(std::move(target_element), click_type, std::move(callback));
}

void WebController::ClickOrTapElement(
    std::unique_ptr<FindElementResult> target_element,
    ClickAction::ClickType click_type,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  std::string element_object_id = target_element->object_id;
  std::vector<std::unique_ptr<runtime::CallArgument>> argument;
  argument.emplace_back(
      runtime::CallArgument::Builder().SetObjectId(element_object_id).Build());
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(element_object_id)
          .SetArguments(std::move(argument))
          .SetFunctionDeclaration(std::string(kScrollIntoViewCenterScript))
          .SetReturnByValue(true)
          .Build(),
      base::BindOnce(&WebController::OnScrollIntoView,
                     weak_ptr_factory_.GetWeakPtr(), std::move(target_element),
                     std::move(callback), click_type));
}

void WebController::OnScrollIntoView(
    std::unique_ptr<FindElementResult> target_element,
    base::OnceCallback<void(const ClientStatus&)> callback,
    ClickAction::ClickType click_type,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  ClientStatus status = CheckJavaScriptResult(result.get(), __FILE__, __LINE__);
  if (!status.ok()) {
    DVLOG(1) << __func__ << " Failed to scroll the element.";
    std::move(callback).Run(status);
    return;
  }

  if (click_type == ClickAction::JAVASCRIPT) {
    std::string element_object_id = target_element->object_id;
    std::vector<std::unique_ptr<runtime::CallArgument>> argument;
    argument.emplace_back(runtime::CallArgument::Builder()
                              .SetObjectId(element_object_id)
                              .Build());
    devtools_client_->GetRuntime()->CallFunctionOn(
        runtime::CallFunctionOnParams::Builder()
            .SetObjectId(element_object_id)
            .SetArguments(std::move(argument))
            .SetFunctionDeclaration(kClickElement)
            .Build(),
        base::BindOnce(&WebController::OnClickJS,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }

  std::unique_ptr<ElementPositionGetter> getter =
      std::make_unique<ElementPositionGetter>(*settings_);
  ElementPositionGetter* getter_ptr = getter.get();
  pending_workers_[getter_ptr] = std::move(getter);
  getter_ptr->Start(target_element->container_frame_host,
                    devtools_client_.get(), target_element->object_id,
                    base::BindOnce(&WebController::TapOrClickOnCoordinates,
                                   weak_ptr_factory_.GetWeakPtr(),
                                   base::Unretained(getter_ptr),
                                   std::move(callback), click_type));
}

void WebController::OnClickJS(
    base::OnceCallback<void(const ClientStatus&)> callback,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  ClientStatus status = CheckJavaScriptResult(result.get(), __FILE__, __LINE__);
  if (!status.ok()) {
    DVLOG(1) << __func__ << " Failed to click (javascript) the element.";
  }
  std::move(callback).Run(status);
}

void WebController::TapOrClickOnCoordinates(
    ElementPositionGetter* getter_to_release,
    base::OnceCallback<void(const ClientStatus&)> callback,
    ClickAction::ClickType click_type,
    bool has_coordinates,
    int x,
    int y) {
  pending_workers_.erase(getter_to_release);

  if (!has_coordinates) {
    DVLOG(1) << __func__ << " Failed to get element position.";
    std::move(callback).Run(ClientStatus(ELEMENT_UNSTABLE));
    return;
  }

  DCHECK(click_type == ClickAction::TAP || click_type == ClickAction::CLICK);
  if (click_type == ClickAction::CLICK) {
    devtools_client_->GetInput()->DispatchMouseEvent(
        input::DispatchMouseEventParams::Builder()
            .SetX(x)
            .SetY(y)
            .SetClickCount(1)
            .SetButton(input::DispatchMouseEventButton::LEFT)
            .SetType(input::DispatchMouseEventType::MOUSE_PRESSED)
            .Build(),
        base::BindOnce(&WebController::OnDispatchPressMouseEvent,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback), x,
                       y));
    return;
  }

  std::vector<std::unique_ptr<::autofill_assistant::input::TouchPoint>>
      touch_points;
  touch_points.emplace_back(
      input::TouchPoint::Builder().SetX(x).SetY(y).Build());
  devtools_client_->GetInput()->DispatchTouchEvent(
      input::DispatchTouchEventParams::Builder()
          .SetType(input::DispatchTouchEventType::TOUCH_START)
          .SetTouchPoints(std::move(touch_points))
          .Build(),
      base::BindOnce(&WebController::OnDispatchTouchEventStart,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnDispatchPressMouseEvent(
    base::OnceCallback<void(const ClientStatus&)> callback,
    int x,
    int y,
    std::unique_ptr<input::DispatchMouseEventResult> result) {
  if (!result) {
    DVLOG(1) << __func__
             << " Failed to dispatch mouse left button pressed event.";
    std::move(callback).Run(UnexpectedErrorStatus(__FILE__, __LINE__));
    return;
  }

  devtools_client_->GetInput()->DispatchMouseEvent(
      input::DispatchMouseEventParams::Builder()
          .SetX(x)
          .SetY(y)
          .SetClickCount(1)
          .SetButton(input::DispatchMouseEventButton::LEFT)
          .SetType(input::DispatchMouseEventType::MOUSE_RELEASED)
          .Build(),
      base::BindOnce(&WebController::OnDispatchReleaseMouseEvent,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnDispatchReleaseMouseEvent(
    base::OnceCallback<void(const ClientStatus&)> callback,
    std::unique_ptr<input::DispatchMouseEventResult> result) {
  std::move(callback).Run(OkClientStatus());
}

void WebController::OnDispatchTouchEventStart(
    base::OnceCallback<void(const ClientStatus&)> callback,
    std::unique_ptr<input::DispatchTouchEventResult> result) {
  if (!result) {
    DVLOG(1) << __func__ << " Failed to dispatch touch start event.";
    std::move(callback).Run(UnexpectedErrorStatus(__FILE__, __LINE__));
    return;
  }

  std::vector<std::unique_ptr<::autofill_assistant::input::TouchPoint>>
      touch_points;
  devtools_client_->GetInput()->DispatchTouchEvent(
      input::DispatchTouchEventParams::Builder()
          .SetType(input::DispatchTouchEventType::TOUCH_END)
          .SetTouchPoints(std::move(touch_points))
          .Build(),
      base::BindOnce(&WebController::OnDispatchTouchEventEnd,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnDispatchTouchEventEnd(
    base::OnceCallback<void(const ClientStatus&)> callback,
    std::unique_ptr<input::DispatchTouchEventResult> result) {
  DCHECK(result);
  std::move(callback).Run(OkClientStatus());
}

void WebController::ElementCheck(const Selector& selector,
                                 bool strict,
                                 base::OnceCallback<void(bool)> callback) {
  DCHECK(!selector.empty());
  FindElement(
      selector, strict,
      base::BindOnce(&WebController::OnFindElementForCheck,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnFindElementForCheck(
    base::OnceCallback<void(bool)> callback,
    const ClientStatus& status,
    std::unique_ptr<FindElementResult> result) {
  DVLOG_IF(1,
           !status.ok() && status.proto_status() != ELEMENT_RESOLUTION_FAILED)
      << __func__ << ": " << status;
  std::move(callback).Run(status.ok());
}

void WebController::WaitForWindowHeightChange(
    base::OnceCallback<void(const ClientStatus&)> callback) {
  devtools_client_->GetRuntime()->Evaluate(
      runtime::EvaluateParams::Builder()
          .SetExpression(kWaitForWindowHeightChange)
          .SetAwaitPromise(true)
          .Build(),
      base::BindOnce(&WebController::OnWaitForWindowHeightChange,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnWaitForWindowHeightChange(
    base::OnceCallback<void(const ClientStatus&)> callback,
    std::unique_ptr<runtime::EvaluateResult> result) {
  std::move(callback).Run(
      CheckJavaScriptResult(result.get(), __FILE__, __LINE__));
}

void WebController::GetDocumentReadyState(
    const Selector& optional_frame,
    base::OnceCallback<void(const ClientStatus&, DocumentReadyState)>
        callback) {
  WaitForDocumentReadyState(optional_frame, DOCUMENT_UNKNOWN_READY_STATE,
                            std::move(callback));
}

void WebController::WaitForDocumentReadyState(
    const Selector& optional_frame,
    DocumentReadyState min_ready_state,
    base::OnceCallback<void(const ClientStatus&, DocumentReadyState)>
        callback) {
  if (optional_frame.empty()) {
    std::string expression;
    expression.append("(");
    AppendWaitForDocumentReadyStateFunction(&expression);
    base::StringAppendF(&expression, ")(%d)",
                        static_cast<int>(min_ready_state));
    devtools_client_->GetRuntime()->Evaluate(
        runtime::EvaluateParams::Builder()
            .SetExpression(expression)
            .SetReturnByValue(true)
            .SetAwaitPromise(true)
            .Build(),
        base::BindOnce(&OnWaitForDocumentReadyState<runtime::EvaluateResult>,
                       std::move(callback)));
    return;
  }
  FindElement(
      optional_frame, /* strict= */ false,
      base::BindOnce(&WebController::OnFindElementForWaitForDocumentReadyState,
                     weak_ptr_factory_.GetWeakPtr(), min_ready_state,
                     std::move(callback)));
}

void WebController::OnFindElementForWaitForDocumentReadyState(
    DocumentReadyState min_ready_state,
    base::OnceCallback<void(const ClientStatus&, DocumentReadyState)> callback,
    const ClientStatus& status,
    std::unique_ptr<FindElementResult> element) {
  if (!status.ok()) {
    std::move(callback).Run(status, DOCUMENT_UNKNOWN_READY_STATE);
    return;
  }

  std::string function_declaration;
  AppendWaitForDocumentReadyStateFunction(&function_declaration);

  std::vector<std::unique_ptr<runtime::CallArgument>> arguments;
  arguments.emplace_back(
      runtime::CallArgument::Builder()
          .SetValue(base::Value::ToUniquePtrValue(
              base::Value(static_cast<int>(min_ready_state))))
          .Build());
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(element ? element->object_id : "")
          .SetFunctionDeclaration(function_declaration)
          .SetArguments(std::move(arguments))
          .SetReturnByValue(true)
          .SetAwaitPromise(true)
          .Build(),
      base::BindOnce(
          &OnWaitForDocumentReadyState<runtime::CallFunctionOnResult>,
          std::move(callback)));
}

void WebController::FindElement(const Selector& selector,
                                bool strict_mode,
                                FindElementCallback callback) {
  auto finder = std::make_unique<ElementFinder>(
      web_contents_, devtools_client_.get(), selector, strict_mode);
  ElementFinder* ptr = finder.get();
  pending_workers_[ptr] = std::move(finder);
  ptr->Start(base::BindOnce(&WebController::OnFindElementResult,
                            base::Unretained(this), ptr, std::move(callback)));
}

void WebController::OnFindElementResult(
    ElementFinder* finder_to_release,
    FindElementCallback callback,
    const ClientStatus& status,
    std::unique_ptr<FindElementResult> result) {
  pending_workers_.erase(finder_to_release);
  std::move(callback).Run(status, std::move(result));
}

void WebController::OnFindElementForFocusElement(
    const TopPadding& top_padding,
    base::OnceCallback<void(const ClientStatus&)> callback,
    const ClientStatus& status,
    std::unique_ptr<FindElementResult> element_result) {
  if (!status.ok()) {
    DVLOG(1) << __func__ << " Failed to find the element to focus on.";
    std::move(callback).Run(status);
    return;
  }

  std::string element_object_id = element_result->object_id;
  WaitForDocumentToBecomeInteractive(
      settings_->document_ready_check_count, element_object_id,
      base::BindOnce(
          &WebController::OnWaitDocumentToBecomeInteractiveForFocusElement,
          weak_ptr_factory_.GetWeakPtr(), top_padding, std::move(callback),
          std::move(element_result)));
}

void WebController::OnWaitDocumentToBecomeInteractiveForFocusElement(
    const TopPadding& top_padding,
    base::OnceCallback<void(const ClientStatus&)> callback,
    std::unique_ptr<FindElementResult> target_element,
    bool result) {
  if (!result) {
    std::move(callback).Run(ClientStatus(ELEMENT_UNSTABLE));
    return;
  }

  std::vector<std::unique_ptr<runtime::CallArgument>> arguments;
  arguments.emplace_back(runtime::CallArgument::Builder()
                             .SetObjectId(target_element->object_id)
                             .Build());
  arguments.emplace_back(runtime::CallArgument::Builder()
                             .SetValue(base::Value::ToUniquePtrValue(
                                 base::Value(top_padding.pixels())))
                             .Build());
  arguments.emplace_back(runtime::CallArgument::Builder()
                             .SetValue(base::Value::ToUniquePtrValue(
                                 base::Value(top_padding.ratio())))
                             .Build());
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(target_element->object_id)
          .SetArguments(std::move(arguments))
          .SetFunctionDeclaration(std::string(kScrollIntoViewWithPaddingScript))
          .SetReturnByValue(true)
          .Build(),
      base::BindOnce(&WebController::OnFocusElement,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnFocusElement(
    base::OnceCallback<void(const ClientStatus&)> callback,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  ClientStatus status = CheckJavaScriptResult(result.get(), __FILE__, __LINE__);
  DVLOG_IF(1, !status.ok()) << __func__ << " Failed to focus on element.";
  std::move(callback).Run(status);
}

void WebController::FillAddressForm(
    const autofill::AutofillProfile* profile,
    const Selector& selector,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  DVLOG(3) << __func__ << selector;
  auto data_to_autofill = std::make_unique<FillFormInputData>();
  data_to_autofill->profile =
      std::make_unique<autofill::AutofillProfile>(*profile);
  FindElement(selector,
              /* strict_mode= */ true,
              base::BindOnce(&WebController::OnFindElementForFillingForm,
                             weak_ptr_factory_.GetWeakPtr(),
                             std::move(data_to_autofill), selector,
                             std::move(callback)));
}

void WebController::OnFindElementForFillingForm(
    std::unique_ptr<FillFormInputData> data_to_autofill,
    const Selector& selector,
    base::OnceCallback<void(const ClientStatus&)> callback,
    const ClientStatus& status,
    std::unique_ptr<FindElementResult> element_result) {
  if (!status.ok()) {
    DVLOG(1) << __func__ << " Failed to find the element for filling the form.";
    std::move(callback).Run(status);
    return;
  }

  ContentAutofillDriver* driver = ContentAutofillDriver::GetForRenderFrameHost(
      element_result->container_frame_host);
  DCHECK(!selector.empty());
  // TODO(crbug.com/806868): Figure out whether there are cases where we need
  // more than one selector, and come up with a solution that can figure out the
  // right number of selectors to include.
  driver->GetAutofillAgent()->GetElementFormAndFieldData(
      std::vector<std::string>(1, selector.selectors.back()),
      base::BindOnce(&WebController::OnGetFormAndFieldDataForFillingForm,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(data_to_autofill), std::move(callback),
                     element_result->container_frame_host));
}

void WebController::OnGetFormAndFieldDataForFillingForm(
    std::unique_ptr<FillFormInputData> data_to_autofill,
    base::OnceCallback<void(const ClientStatus&)> callback,
    content::RenderFrameHost* container_frame_host,
    const autofill::FormData& form_data,
    const autofill::FormFieldData& form_field) {
  if (form_data.fields.empty()) {
    DVLOG(1) << __func__ << " Failed to get form data to fill form.";
    std::move(callback).Run(
        UnexpectedErrorStatus(__FILE__, __LINE__));  // unexpected
    return;
  }

  ContentAutofillDriver* driver =
      ContentAutofillDriver::GetForRenderFrameHost(container_frame_host);
  if (!driver) {
    DVLOG(1) << __func__ << " Failed to get the autofill driver.";
    std::move(callback).Run(
        UnexpectedErrorStatus(__FILE__, __LINE__));  // unexpected
    return;
  }

  if (data_to_autofill->card) {
    driver->autofill_manager()->FillCreditCardForm(
        autofill::kNoQueryId, form_data, form_field, *data_to_autofill->card,
        data_to_autofill->cvc);
  } else {
    driver->autofill_manager()->FillProfileForm(*data_to_autofill->profile,
                                                form_data, form_field);
  }

  std::move(callback).Run(OkClientStatus());
}

void WebController::FillCardForm(
    std::unique_ptr<autofill::CreditCard> card,
    const base::string16& cvc,
    const Selector& selector,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  DVLOG(3) << __func__ << " " << selector;
  auto data_to_autofill = std::make_unique<FillFormInputData>();
  data_to_autofill->card = std::move(card);
  data_to_autofill->cvc = cvc;
  FindElement(selector,
              /* strict_mode= */ true,
              base::BindOnce(&WebController::OnFindElementForFillingForm,
                             weak_ptr_factory_.GetWeakPtr(),
                             std::move(data_to_autofill), selector,
                             std::move(callback)));
}

void WebController::SelectOption(
    const Selector& selector,
    const std::string& selected_option,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  DVLOG(3) << __func__ << " " << selector << ", option=" << selected_option;
  FindElement(selector,
              /* strict_mode= */ true,
              base::BindOnce(&WebController::OnFindElementForSelectOption,
                             weak_ptr_factory_.GetWeakPtr(), selected_option,
                             std::move(callback)));
}

void WebController::OnFindElementForSelectOption(
    const std::string& selected_option,
    base::OnceCallback<void(const ClientStatus&)> callback,
    const ClientStatus& status,
    std::unique_ptr<FindElementResult> element_result) {
  if (!status.ok()) {
    DVLOG(1) << __func__ << " Failed to find the element to select an option.";
    std::move(callback).Run(status);
    return;
  }

  std::vector<std::unique_ptr<runtime::CallArgument>> argument;
  argument.emplace_back(
      runtime::CallArgument::Builder()
          .SetValue(base::Value::ToUniquePtrValue(base::Value(selected_option)))
          .Build());
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(element_result->object_id)
          .SetArguments(std::move(argument))
          .SetFunctionDeclaration(std::string(kSelectOptionScript))
          .SetReturnByValue(true)
          .Build(),
      base::BindOnce(&WebController::OnSelectOption,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnSelectOption(
    base::OnceCallback<void(const ClientStatus&)> callback,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  ClientStatus status = CheckJavaScriptResult(result.get(), __FILE__, __LINE__);
  if (!status.ok()) {
    DVLOG(1) << __func__ << " Failed to select option.";
    std::move(callback).Run(status);
    return;
  }
  bool found;
  if (!SafeGetBool(result->GetResult(), &found)) {
    std::move(callback).Run(UnexpectedErrorStatus(__FILE__, __LINE__));
    return;
  }
  if (!found) {
    DVLOG(1) << __func__ << " Failed to find option.";
    std::move(callback).Run(ClientStatus(OPTION_VALUE_NOT_FOUND));
    return;
  }
  std::move(callback).Run(OkClientStatus());
}

void WebController::HighlightElement(
    const Selector& selector,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  DVLOG(3) << __func__ << " " << selector;
  FindElement(
      selector,
      /* strict_mode= */ true,
      base::BindOnce(&WebController::OnFindElementForHighlightElement,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnFindElementForHighlightElement(
    base::OnceCallback<void(const ClientStatus&)> callback,
    const ClientStatus& status,
    std::unique_ptr<FindElementResult> element_result) {
  if (!status.ok()) {
    DVLOG(1) << __func__ << " Failed to find the element to highlight.";
    std::move(callback).Run(status);
    return;
  }

  const std::string& object_id = element_result->object_id;
  std::vector<std::unique_ptr<runtime::CallArgument>> argument;
  argument.emplace_back(
      runtime::CallArgument::Builder().SetObjectId(object_id).Build());
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(object_id)
          .SetArguments(std::move(argument))
          .SetFunctionDeclaration(std::string(kHighlightElementScript))
          .SetReturnByValue(true)
          .Build(),
      base::BindOnce(&WebController::OnHighlightElement,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnHighlightElement(
    base::OnceCallback<void(const ClientStatus&)> callback,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  ClientStatus status = CheckJavaScriptResult(result.get(), __FILE__, __LINE__);
  DVLOG_IF(1, !status.ok()) << __func__ << " Failed to highlight element.";
  std::move(callback).Run(status);
}

void WebController::FocusElement(
    const Selector& selector,
    const TopPadding& top_padding,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  DVLOG(3) << __func__ << " " << selector;
  DCHECK(!selector.empty());
  FindElement(selector,
              /* strict_mode= */ false,
              base::BindOnce(&WebController::OnFindElementForFocusElement,
                             weak_ptr_factory_.GetWeakPtr(), top_padding,
                             std::move(callback)));
}

void WebController::GetFieldValue(
    const Selector& selector,
    base::OnceCallback<void(bool, const std::string&)> callback) {
  FindElement(
      selector,
      /* strict_mode= */ true,
      base::BindOnce(&WebController::OnFindElementForGetFieldValue,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnFindElementForGetFieldValue(
    base::OnceCallback<void(bool, const std::string&)> callback,
    const ClientStatus& status,
    std::unique_ptr<FindElementResult> element_result) {
  const std::string object_id = element_result->object_id;
  if (!status.ok()) {
    std::move(callback).Run(/* exists= */ false, "");
    return;
  }

  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(object_id)
          .SetFunctionDeclaration(std::string(kGetValueAttributeScript))
          .SetReturnByValue(true)
          .Build(),
      base::BindOnce(&WebController::OnGetValueAttribute,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnGetValueAttribute(
    base::OnceCallback<void(bool, const std::string&)> callback,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  std::string value;
  ClientStatus status = CheckJavaScriptResult(result.get(), __FILE__, __LINE__);
  // Read the result returned from Javascript code.
  DVLOG_IF(1, !status.ok())
      << __func__ << "Failed to get attribute value: " << status;
  SafeGetStringValue(result->GetResult(), &value);
  std::move(callback).Run(/* exists= */ true, value);
}

void WebController::SetFieldValue(
    const Selector& selector,
    const std::string& value,
    bool simulate_key_presses,
    int key_press_delay_in_millisecond,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  DVLOG(3) << __func__ << " " << selector << ", value=" << value
           << ", simulate_key_presses=" << simulate_key_presses;
  if (simulate_key_presses) {
    // We first clear the field value, and then simulate the key presses.
    // TODO(crbug.com/806868): Disable keyboard during this action and then
    // reset to previous state.
    InternalSetFieldValue(
        selector, "",
        base::BindOnce(&WebController::OnClearFieldForSendKeyboardInput,
                       weak_ptr_factory_.GetWeakPtr(), selector,
                       UTF8ToUnicode(value), key_press_delay_in_millisecond,
                       std::move(callback)));
    return;
  }
  InternalSetFieldValue(selector, value, std::move(callback));
}

void WebController::InternalSetFieldValue(
    const Selector& selector,
    const std::string& value,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  FindElement(selector,
              /* strict_mode= */ true,
              base::BindOnce(&WebController::OnFindElementForSetFieldValue,
                             weak_ptr_factory_.GetWeakPtr(), value,
                             std::move(callback)));
}

void WebController::OnClearFieldForSendKeyboardInput(
    const Selector& selector,
    const std::vector<UChar32>& codepoints,
    int key_press_delay_in_millisecond,
    base::OnceCallback<void(const ClientStatus&)> callback,
    const ClientStatus& clear_status) {
  if (!clear_status.ok()) {
    std::move(callback).Run(clear_status);
    return;
  }
  SendKeyboardInput(selector, codepoints, key_press_delay_in_millisecond,
                    std::move(callback));
}

void WebController::OnClickElementForSendKeyboardInput(
    const std::vector<UChar32>& codepoints,
    int delay_in_millisecond,
    base::OnceCallback<void(const ClientStatus&)> callback,
    const ClientStatus& click_status) {
  if (!click_status.ok()) {
    std::move(callback).Run(click_status);
    return;
  }
  DispatchKeyboardTextDownEvent(codepoints, 0, /*delay=*/false,
                                delay_in_millisecond, std::move(callback));
}

void WebController::DispatchKeyboardTextDownEvent(
    const std::vector<UChar32>& codepoints,
    size_t index,
    bool delay,
    int delay_in_millisecond,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  if (index >= codepoints.size()) {
    std::move(callback).Run(OkClientStatus());
    return;
  }

  if (delay && delay_in_millisecond > 0) {
    base::PostDelayedTaskWithTraits(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(&WebController::DispatchKeyboardTextDownEvent,
                       weak_ptr_factory_.GetWeakPtr(), codepoints, index,
                       /*delay=*/false, delay_in_millisecond,
                       std::move(callback)),
        base::TimeDelta::FromMilliseconds(delay_in_millisecond));
    return;
  }

  devtools_client_->GetInput()->DispatchKeyEvent(
      CreateKeyEventParamsForCharacter(
          autofill_assistant::input::DispatchKeyEventType::KEY_DOWN,
          codepoints[index]),
      base::BindOnce(&WebController::DispatchKeyboardTextUpEvent,
                     weak_ptr_factory_.GetWeakPtr(), codepoints, index,
                     delay_in_millisecond, std::move(callback)));
}

void WebController::DispatchKeyboardTextUpEvent(
    const std::vector<UChar32>& codepoints,
    size_t index,
    int delay_in_millisecond,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  DCHECK_LT(index, codepoints.size());
  devtools_client_->GetInput()->DispatchKeyEvent(
      CreateKeyEventParamsForCharacter(
          autofill_assistant::input::DispatchKeyEventType::KEY_UP,
          codepoints[index]),
      base::BindOnce(&WebController::DispatchKeyboardTextDownEvent,
                     weak_ptr_factory_.GetWeakPtr(), codepoints, index + 1,
                     /*delay=*/true, delay_in_millisecond,
                     std::move(callback)));
}

auto WebController::CreateKeyEventParamsForCharacter(
    autofill_assistant::input::DispatchKeyEventType type,
    UChar32 codepoint) -> DispatchKeyEventParamsPtr {
  auto params = input::DispatchKeyEventParams::Builder().SetType(type).Build();

  std::string text;
  if (AppendUnicodeToUTF8(codepoint, &text)) {
    params->SetText(text);
  } else {
    DVLOG(1) << __func__
             << ": Failed to convert codepoint to UTF-8: " << codepoint;
  }

  auto dom_key = ui::DomKey::FromCharacter(codepoint);
  if (dom_key.IsValid()) {
    params->SetKey(ui::KeycodeConverter::DomKeyToKeyString(dom_key));
  } else {
    DVLOG(1) << __func__
             << ": Failed to set DomKey for codepoint: " << codepoint;
  }

  return params;
}

void WebController::OnFindElementForSetFieldValue(
    const std::string& value,
    base::OnceCallback<void(const ClientStatus&)> callback,
    const ClientStatus& status,
    std::unique_ptr<FindElementResult> element_result) {
  if (!status.ok()) {
    std::move(callback).Run(status);
    return;
  }

  std::vector<std::unique_ptr<runtime::CallArgument>> argument;
  argument.emplace_back(
      runtime::CallArgument::Builder()
          .SetValue(base::Value::ToUniquePtrValue(base::Value(value)))
          .Build());
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(element_result->object_id)
          .SetArguments(std::move(argument))
          .SetFunctionDeclaration(std::string(kSetValueAttributeScript))
          .Build(),
      base::BindOnce(&WebController::OnSetValueAttribute,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnSetValueAttribute(
    base::OnceCallback<void(const ClientStatus&)> callback,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  std::move(callback).Run(
      CheckJavaScriptResult(result.get(), __FILE__, __LINE__));
}

void WebController::SetAttribute(
    const Selector& selector,
    const std::vector<std::string>& attribute,
    const std::string& value,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  DVLOG(3) << __func__ << " " << selector << ", attribute=["
           << base::JoinString(attribute, ",") << "], value=" << value;

  DCHECK(!selector.empty());
  DCHECK_GT(attribute.size(), 0u);
  FindElement(selector,
              /* strict_mode= */ true,
              base::BindOnce(&WebController::OnFindElementForSetAttribute,
                             weak_ptr_factory_.GetWeakPtr(), attribute, value,
                             std::move(callback)));
}

void WebController::OnFindElementForSetAttribute(
    const std::vector<std::string>& attribute,
    const std::string& value,
    base::OnceCallback<void(const ClientStatus&)> callback,
    const ClientStatus& status,
    std::unique_ptr<FindElementResult> element_result) {
  if (!status.ok()) {
    std::move(callback).Run(status);
    return;
  }

  base::Value::ListStorage attribute_values;
  for (const std::string& string : attribute) {
    attribute_values.emplace_back(base::Value(string));
  }

  std::vector<std::unique_ptr<runtime::CallArgument>> arguments;
  arguments.emplace_back(runtime::CallArgument::Builder()
                             .SetValue(base::Value::ToUniquePtrValue(
                                 base::Value(attribute_values)))
                             .Build());
  arguments.emplace_back(
      runtime::CallArgument::Builder()
          .SetValue(base::Value::ToUniquePtrValue(base::Value(value)))
          .Build());
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(element_result->object_id)
          .SetArguments(std::move(arguments))
          .SetFunctionDeclaration(std::string(kSetAttributeScript))
          .Build(),
      base::BindOnce(&WebController::OnSetAttribute,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnSetAttribute(
    base::OnceCallback<void(const ClientStatus&)> callback,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  std::move(callback).Run(
      CheckJavaScriptResult(result.get(), __FILE__, __LINE__));
}

void WebController::SendKeyboardInput(
    const Selector& selector,
    const std::vector<UChar32>& codepoints,
    const int delay_in_millisecond,
    base::OnceCallback<void(const ClientStatus&)> callback) {
  if (VLOG_IS_ON(3)) {
    std::string input_str;
    if (!UnicodeToUTF8(codepoints, &input_str)) {
      input_str.assign("<invalid input>");
    }
    DVLOG(3) << __func__ << " " << selector << ", input=" << input_str;
  }

  DCHECK(!selector.empty());
  FindElement(
      selector,
      /* strict_mode= */ true,
      base::BindOnce(&WebController::OnFindElementForSendKeyboardInput,
                     weak_ptr_factory_.GetWeakPtr(), selector, codepoints,
                     delay_in_millisecond, std::move(callback)));
}

void WebController::OnFindElementForSendKeyboardInput(
    const Selector& selector,
    const std::vector<UChar32>& codepoints,
    const int delay_in_millisecond,
    base::OnceCallback<void(const ClientStatus&)> callback,
    const ClientStatus& status,
    std::unique_ptr<FindElementResult> element_result) {
  if (!status.ok()) {
    std::move(callback).Run(status);
    return;
  }
  ClickOrTapElement(
      selector, ClickAction::CLICK,
      base::BindOnce(&WebController::OnClickElementForSendKeyboardInput,
                     weak_ptr_factory_.GetWeakPtr(), codepoints,
                     delay_in_millisecond, std::move(callback)));
}

void WebController::GetOuterHtml(
    const Selector& selector,
    base::OnceCallback<void(const ClientStatus&, const std::string&)>
        callback) {
  DVLOG(3) << __func__ << " " << selector;
  FindElement(
      selector,
      /* strict_mode= */ true,
      base::BindOnce(&WebController::OnFindElementForGetOuterHtml,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::GetVisualViewport(
    base::OnceCallback<void(bool, const RectF&)> callback) {
  devtools_client_->GetRuntime()->Evaluate(
      runtime::EvaluateParams::Builder()
          .SetExpression(std::string(kGetVisualViewport))
          .SetReturnByValue(true)
          .Build(),
      base::BindOnce(&WebController::OnGetVisualViewport,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnGetVisualViewport(
    base::OnceCallback<void(bool, const RectF&)> callback,
    std::unique_ptr<runtime::EvaluateResult> result) {
  ClientStatus status = CheckJavaScriptResult(result.get(), __FILE__, __LINE__);
  if (!status.ok() || !result->GetResult()->HasValue() ||
      !result->GetResult()->GetValue()->is_list() ||
      result->GetResult()->GetValue()->GetList().size() != 4u) {
    DVLOG(1) << __func__ << " Failed to get visual viewport: " << status;
    RectF empty;
    std::move(callback).Run(false, empty);
    return;
  }
  const auto& list = result->GetResult()->GetValue()->GetList();
  // Value::GetDouble() is safe to call without checking the value type; it'll
  // return 0.0 if the value has the wrong type.

  float left = static_cast<float>(list[0].GetDouble());
  float top = static_cast<float>(list[1].GetDouble());
  float width = static_cast<float>(list[2].GetDouble());
  float height = static_cast<float>(list[3].GetDouble());

  RectF rect;
  rect.left = left;
  rect.top = top;
  rect.right = left + width;
  rect.bottom = top + height;

  std::move(callback).Run(true, rect);
}

void WebController::GetElementPosition(
    const Selector& selector,
    base::OnceCallback<void(bool, const RectF&)> callback) {
  FindElement(
      selector, /* strict_mode= */ true,
      base::BindOnce(&WebController::OnFindElementForPosition,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnFindElementForPosition(
    base::OnceCallback<void(bool, const RectF&)> callback,
    const ClientStatus& status,
    std::unique_ptr<FindElementResult> result) {
  if (!status.ok()) {
    RectF empty;
    std::move(callback).Run(false, empty);
    return;
  }

  std::vector<std::unique_ptr<runtime::CallArgument>> argument;
  argument.emplace_back(
      runtime::CallArgument::Builder().SetObjectId(result->object_id).Build());
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(result->object_id)
          .SetArguments(std::move(argument))
          .SetFunctionDeclaration(std::string(kGetBoundingClientRectAsList))
          .SetReturnByValue(true)
          .Build(),
      base::BindOnce(&WebController::OnGetElementPositionResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnGetElementPositionResult(
    base::OnceCallback<void(bool, const RectF&)> callback,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  ClientStatus status = CheckJavaScriptResult(result.get(), __FILE__, __LINE__);
  if (!status.ok() || !result->GetResult()->HasValue() ||
      !result->GetResult()->GetValue()->is_list() ||
      result->GetResult()->GetValue()->GetList().size() != 4u) {
    DVLOG(2) << __func__ << " Failed to get element position: " << status;
    RectF empty;
    std::move(callback).Run(false, empty);
    return;
  }
  const auto& list = result->GetResult()->GetValue()->GetList();
  // Value::GetDouble() is safe to call without checking the value type; it'll
  // return 0.0 if the value has the wrong type.

  RectF rect;
  rect.left = static_cast<float>(list[0].GetDouble());
  rect.top = static_cast<float>(list[1].GetDouble());
  rect.right = static_cast<float>(list[2].GetDouble());
  rect.bottom = static_cast<float>(list[3].GetDouble());

  std::move(callback).Run(true, rect);
}

void WebController::OnFindElementForGetOuterHtml(
    base::OnceCallback<void(const ClientStatus&, const std::string&)> callback,
    const ClientStatus& status,
    std::unique_ptr<FindElementResult> element_result) {
  if (!status.ok()) {
    DVLOG(2) << __func__ << " Failed to find element for GetOuterHtml";
    std::move(callback).Run(status, "");
    return;
  }

  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(element_result->object_id)
          .SetFunctionDeclaration(std::string(kGetOuterHtmlScript))
          .SetReturnByValue(true)
          .Build(),
      base::BindOnce(&WebController::OnGetOuterHtml,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebController::OnGetOuterHtml(
    base::OnceCallback<void(const ClientStatus&, const std::string&)> callback,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  ClientStatus status = CheckJavaScriptResult(result.get(), __FILE__, __LINE__);
  if (!status.ok()) {
    DVLOG(2) << __func__ << " Failed to get HTML content for GetOuterHtml";
    std::move(callback).Run(status, "");
    return;
  }
  std::string value;
  SafeGetStringValue(result->GetResult(), &value);
  std::move(callback).Run(OkClientStatus(), value);
}

void WebController::WaitForDocumentToBecomeInteractive(
    int remaining_rounds,
    std::string object_id,
    base::OnceCallback<void(bool)> callback) {
  devtools_client_->GetRuntime()->CallFunctionOn(
      runtime::CallFunctionOnParams::Builder()
          .SetObjectId(object_id)
          .SetFunctionDeclaration(std::string(kIsDocumentReadyForInteract))
          .SetReturnByValue(true)
          .Build(),
      base::BindOnce(&WebController::OnWaitForDocumentToBecomeInteractive,
                     weak_ptr_factory_.GetWeakPtr(), remaining_rounds,
                     object_id, std::move(callback)));
}

void WebController::OnWaitForDocumentToBecomeInteractive(
    int remaining_rounds,
    std::string object_id,
    base::OnceCallback<void(bool)> callback,
    std::unique_ptr<runtime::CallFunctionOnResult> result) {
  ClientStatus status = CheckJavaScriptResult(result.get(), __FILE__, __LINE__);
  if (!status.ok() || remaining_rounds <= 0) {
    DVLOG(1) << __func__
             << " Failed to wait for the document to become interactive with "
                "remaining_rounds: "
             << remaining_rounds;
    std::move(callback).Run(false);
    return;
  }

  bool ready;
  if (SafeGetBool(result->GetResult(), &ready) && ready) {
    std::move(callback).Run(true);
    return;
  }

  base::PostDelayedTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&WebController::WaitForDocumentToBecomeInteractive,
                     weak_ptr_factory_.GetWeakPtr(), --remaining_rounds,
                     object_id, std::move(callback)),
      settings_->document_ready_check_interval);
}

}  // namespace autofill_assistant
