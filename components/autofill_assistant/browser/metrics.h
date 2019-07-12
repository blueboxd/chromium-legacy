// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_METRICS_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_METRICS_H_

#include <ostream>

namespace autofill_assistant {

// A class to generate Autofill Assistant related histograms.
class Metrics {
 public:
  // The different ways that autofill assistant can stop.
  //
  // GENERATED_JAVA_ENUM_PACKAGE: (
  // org.chromium.chrome.browser.autofill_assistant.metrics)
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: DropOutReason
  //
  // This enum is used in histograms, do not remove/renumber entries. Only add
  // at the end and update NUM_ENTRIES. Also remember to update the
  // AutofillAssistantDropOutReason enum listing in
  // tools/metrics/histograms/enums.xml.
  enum class DropOutReason {
    AA_START = 0,
    AUTOSTART_TIMEOUT = 1,
    NO_SCRIPTS = 2,
    CUSTOM_TAB_CLOSED = 3,
    DECLINED = 4,
    SHEET_CLOSED = 5,
    SCRIPT_FAILED = 6,
    NAVIGATION = 7,
    OVERLAY_STOP = 8,
    PR_FAILED = 9,
    CONTENT_DESTROYED = 10,
    RENDER_PROCESS_GONE = 11,
    INTERSTITIAL_PAGE = 12,
    SCRIPT_SHUTDOWN = 13,
    SAFETY_NET_TERMINATE = 14,  // This is a "should never happen" entry.
    TAB_DETACHED = 15,
    TAB_CHANGED = 16,
    GET_SCRIPTS_FAILED = 17,
    GET_SCRIPTS_UNPARSABLE = 18,
    NO_INITIAL_SCRIPTS = 19,
    DFM_INSTALL_FAILED = 20,

    NUM_ENTRIES = 21
  };

  // The different ways that autofill assistant can stop.
  //
  // GENERATED_JAVA_ENUM_PACKAGE: (
  // org.chromium.chrome.browser.autofill_assistant.metrics)
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: OnBoarding
  //
  // This enum is used in histograms, do not remove/renumber entries. Only add
  // at the end and update NUM_ENTRIES. Also remember to update the
  // AutofillAssistantOnBoarding enum listing in
  // tools/metrics/histograms/enums.xml.
  enum class OnBoarding {
    OB_SHOWN = 0,
    OB_NOT_SHOWN = 1,
    OB_ACCEPTED = 2,
    OB_CANCELLED = 3,

    NUM_ENTRIES = 4
  };

  // The different ways for payment request to succeed or fail, broken down by
  // whether the PR initially presented to the user was completely pre-filled
  // or not.
  //
  // This enum is used in histograms, do not remove/renumber entries. Only add
  // at the end and update NUM_ENTRIES. Also remember to update the
  // AutofillAssistantPaymentRequestPrefilled enum listing in
  // tools/metrics/histograms/enums.xml.
  enum class PaymentRequestPrefilled {
    PREFILLED_SUCCESS = 0,
    NOTPREFILLED_SUCCESS = 1,
    PREFILLED_FAILURE = 2,
    NOTPREFILLED_FAILURE = 3,

    NUM_ENTRIES = 4
  };

  static void RecordDropOut(DropOutReason reason);
  static void RecordPaymentRequestPrefilledSuccess(bool initially_complete,
                                                   bool success);

  // Intended for debugging: writes string representation of |reason| to |out|.
  friend std::ostream& operator<<(std::ostream& out,
                                  const DropOutReason& reason) {
#ifdef NDEBUG
    // Non-debugging builds write the enum number.
    out << static_cast<int>(reason);
    return out;
#else
    // Debugging builds write a string representation of |reason|.
    switch (reason) {
      case DropOutReason::AA_START:
        out << "AA_START";
        break;
      case DropOutReason::AUTOSTART_TIMEOUT:
        out << "AUTOSTART_TIMEOUT";
        break;
      case DropOutReason::NO_SCRIPTS:
        out << "NO_SCRIPTS";
        break;
      case DropOutReason::CUSTOM_TAB_CLOSED:
        out << "CUSTOM_TAB_CLOSED";
        break;
      case DropOutReason::DECLINED:
        out << "DECLINED";
        break;
      case DropOutReason::SHEET_CLOSED:
        out << "SHEET_CLOSED";
        break;
      case DropOutReason::SCRIPT_FAILED:
        out << "SCRIPT_FAILED";
        break;
      case DropOutReason::NAVIGATION:
        out << "NAVIGATION";
        break;
      case DropOutReason::OVERLAY_STOP:
        out << "OVERLAY_STOP";
        break;
      case DropOutReason::PR_FAILED:
        out << "PR_FAILED";
        break;
      case DropOutReason::CONTENT_DESTROYED:
        out << "CONTENT_DESTROYED";
        break;
      case DropOutReason::RENDER_PROCESS_GONE:
        out << "RENDER_PROCESS_GONE";
        break;
      case DropOutReason::INTERSTITIAL_PAGE:
        out << "INTERSTITIAL_PAGE";
        break;
      case DropOutReason::SCRIPT_SHUTDOWN:
        out << "SCRIPT_SHUTDOWN";
        break;
      case DropOutReason::SAFETY_NET_TERMINATE:
        out << "SAFETY_NET_TERMINATE";
        break;
      case DropOutReason::TAB_DETACHED:
        out << "TAB_DETACHED";
        break;
      case DropOutReason::TAB_CHANGED:
        out << "TAB_CHANGED";
        break;
      case DropOutReason::GET_SCRIPTS_FAILED:
        out << "GET_SCRIPTS_FAILED";
        break;
      case DropOutReason::GET_SCRIPTS_UNPARSABLE:
        out << "GET_SCRIPTS_UNPARSEABLE";
        break;
      case DropOutReason::NO_INITIAL_SCRIPTS:
        out << "NO_INITIAL_SCRIPTS";
        break;
      case DropOutReason::DFM_INSTALL_FAILED:
        out << "DFM_INSTALL_FAILED";
        break;
      case DropOutReason::NUM_ENTRIES:
        out << "NUM_ENTRIES";
        break;

        // Intentionally no default case to make compilation fail
        // if a new value was added to the enum but not to this list.
    }
    return out;
#endif  // NDEBUG
  }

  // Intended for debugging: writes string representation of |metric| to |out|.
  friend std::ostream& operator<<(std::ostream& out, const OnBoarding& metric) {
#ifdef NDEBUG
    // Non-debugging builds write the enum number.
    out << static_cast<int>(metric);
    return out;
#else
    // Debugging builds write a string representation of |metric|.
    switch (metric) {
      case OnBoarding::OB_SHOWN:
        out << "OB_SHOWN";
        break;
      case OnBoarding::OB_NOT_SHOWN:
        out << "OB_NOT_SHOWN";
        break;
      case OnBoarding::OB_ACCEPTED:
        out << "OB_ACCEPTED";
        break;
      case OnBoarding::OB_CANCELLED:
        out << "OB_CANCELLED";
        break;
      case OnBoarding::NUM_ENTRIES:
        out << "NUM_ENTRIES";
        break;
    }
    return out;
#endif  // NDEBUG
  }
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_METRICS_H_
