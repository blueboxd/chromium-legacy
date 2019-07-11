// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_INTERNALS_LOGGING_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_INTERNALS_LOGGING_H_

#include <string>
#include <type_traits>
#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"
#include "url/gurl.h"

//
// Autofill Internals Logging
// --------------------------
// This framework serves the purpose of exposing log messages of Chrome Autofill
// (and in the future also Chrome's Password Manager) via
// chrome://autofill-internals (and in the future also
// chrome://password-manager-internals).
//
// The framework uses a so called LoggingScope as a mechanism to indicate during
// which phase of processing a website a message was generated. Logging scopes
// comprise for example the phase of parsing forms, or filling forms. Each
// logging scope is displayed in a visually distinct color.
//
// The framework also uses predefined LogMessages. Each message consists of a
// fixed label and a string. The labels allow for specialized styling (e.g.
// highlighting specific messages because they indicate failures).
//
// The desired pattern to generate log messages is to pass a scope, a log
// message and then parameters.
//
// LOG_AF_INTERNALS << LoggingScope::kSomeScope << LogMessage::kSomeLogMessage
//     << Br{} << more << Br{} << parameters;
//
// Extra parameters can be:
//
// - numeric:
//   LOG_AF_INTERNALS << ... << 42;
//
// - inline strings:
//   LOG_AF_INTERNALS << ... << "foobar";
//
// - tags:
//   LOG_AF_INTERNALS << Tag{"div"} << ... << CTag{};
//   Note that tags need to be closed (even for <br> - use Br{} as it takes care
//   of generating an opening and closing tag). You may optionally specify what
//   tag is closed: CTag{"div"}.
//   Tags can get attributes via Attrib:
//   LOG_AF_INTERNALS << Tag{"div"} << Attrib{"class", "foobar"}... << CTag{};
//
// - objects that can have an overloaded operator:
//   AutofillInternalsBuffer& operator<<(AutofillInternalsBuffer& buf,
//                                       const SampleObject& value) {...}
//   LOG_AF_INTERNALS << ... << my_sample_object;
//
// - complex messages that require for loops:
//   AutofillInternalsBuffer buffer;
//   for (...) { buffer << something; }
//   LOG_AF_INTERNALS << std::move(buffer);
//
// Note that each call of LOG_AF_INTERNALS spawns a new log entry.
//
// See components/autofill/core/common/autofill_internals/logging_scope.h for
// the definition of scopes.
// See components/autofill/core/common/autofill_internals/log_message.h for
// the definition of messages.

namespace autofill {

// Tag of HTML Element (e.g. <div> would be represented by Tag{"div"}). Note
// that every element needs to be closed with a CTag{}.
struct Tag {
  std::string name;
};

// The closing tag of an HTML Element.
struct CTag {
  CTag() = default;
  // |opt_name| is not used, and only exists for readability.
  explicit CTag(base::StringPiece opt_name) {}
};

// Attribute of an HTML Tag (e.g. class="foo" would be represented by
// Attrib{"class", "foo"}.
struct Attrib {
  std::string name;
  std::string value;
};

// An <br> HTML tag, note that this does not need to be closed.
struct Br {};

// A table row containing two cells. This generates
// <tr><td>%1</td><td>%2</td></tr>
template <typename T1, typename T2>
struct Tr2Cells {
  T1 cell1;
  T2 cell2;
};

// Helper function to create Tr2Cells entries w/o specifying the type names.
template <typename T1, typename T2>
Tr2Cells<T1, T2> MakeTr2Cells(T1&& cell1, T2&& cell2) {
  return Tr2Cells<T1, T2>{std::forward<T1>(cell1), std::forward<T2>(cell2)};
}

// A buffer into which you can stream values. See the top of this header file
// for samples.
class AutofillInternalsBuffer {
 public:
  AutofillInternalsBuffer();
  AutofillInternalsBuffer(AutofillInternalsBuffer&& other) noexcept;
  ~AutofillInternalsBuffer();

  // Returns the contents of the buffer and empties it.
  base::Value RetrieveResult();

  // Returns whether an active WebUI is listening. If false, the buffer may
  // not do any logging.
  bool active() const { return active_; }
  void set_active(bool active) { active_ = active; }

 private:
  friend AutofillInternalsBuffer& operator<<(AutofillInternalsBuffer& buf,
                                             Tag&& tag);
  friend AutofillInternalsBuffer& operator<<(AutofillInternalsBuffer& buf,
                                             CTag&& tag);
  friend AutofillInternalsBuffer& operator<<(AutofillInternalsBuffer& buf,
                                             Attrib&& attrib);
  friend AutofillInternalsBuffer& operator<<(AutofillInternalsBuffer& buf,
                                             base::StringPiece text);
  friend AutofillInternalsBuffer& operator<<(AutofillInternalsBuffer& buf,
                                             AutofillInternalsBuffer&& buffer);

  // The stack of values being constructed. Each item is a dictionary with the
  // following attributes:
  // - type: 'node' | 'text'
  // - value: name of tag | text content
  // - children (opt): list of child nodes
  // - attributes (opt): dictionary of name/value pairs
  // The |buffer_| serves as a stack where the last element is being
  // constructed. Once it is read (i.e. closed via a CTag), it is popped from
  // the stack and attached as a child of the previously second last element.
  std::vector<base::Value> buffer_;

  bool active_ = true;

  DISALLOW_COPY_AND_ASSIGN(AutofillInternalsBuffer);
};

// Enable streaming numbers of all types.
template <typename T,
          typename = std::enable_if_t<std::is_arithmetic<T>::value, T>>
AutofillInternalsBuffer& operator<<(AutofillInternalsBuffer& buf, T number) {
  return buf << base::NumberToString(number);
}

AutofillInternalsBuffer& operator<<(AutofillInternalsBuffer& buf, Tag&& tag);

AutofillInternalsBuffer& operator<<(AutofillInternalsBuffer& buf, CTag&& tag);

AutofillInternalsBuffer& operator<<(AutofillInternalsBuffer& buf,
                                    Attrib&& attrib);

AutofillInternalsBuffer& operator<<(AutofillInternalsBuffer& buf, Br&& tag);

AutofillInternalsBuffer& operator<<(AutofillInternalsBuffer& buf,
                                    base::StringPiece text);

AutofillInternalsBuffer& operator<<(AutofillInternalsBuffer& buf,
                                    base::StringPiece16 text);

// Sometimes you may want to fill a buffer that you then stream as a whole
// to LOG_AF_INTERNALS, which commits the data to chrome://autofill-internals:
//
//   AutofillInternalsBuffer buffer;
//   for (FormStructure* form : forms)
//     buffer << *form;
//   LOG_AF_INTERNALS << LoggingScope::kParsing << std::move(buffer);
//
// It would not be possible to report all |forms| into a single log entry
// without this.
AutofillInternalsBuffer& operator<<(AutofillInternalsBuffer& buf,
                                    AutofillInternalsBuffer&& buffer);

AutofillInternalsBuffer& operator<<(AutofillInternalsBuffer& buf,
                                    LoggingScope scope);

AutofillInternalsBuffer& operator<<(AutofillInternalsBuffer& buf,
                                    LogMessage message);

// Streams only the security origin of the URL. This is done for privacy
// reasons.
AutofillInternalsBuffer& operator<<(AutofillInternalsBuffer& buf,
                                    const GURL& url);

template <typename T1, typename T2>
AutofillInternalsBuffer& operator<<(AutofillInternalsBuffer& buf,
                                    Tr2Cells<T1, T2>&& row) {
  return buf << Tag{"tr"} << Tag{"td"} << std::move(row.cell1) << CTag{}
             << Tag{"td"} << std::move(row.cell2) << CTag{} << CTag{};
}

// A container for a AutofillInternalsBuffer that submits the buffer to the
// corresponding internals page on destruction.
class AutofillInternalsMessage {
 public:
  // Whether the message will be sent to chrome://autofill-internals or
  // chrome://password-manager-internals.
  enum class Destination {
    kAutofillInternals,
    kPasswordManagerInternals,
  };

  explicit AutofillInternalsMessage(Destination destination);
  ~AutofillInternalsMessage();

  AutofillInternalsBuffer& buffer() { return buffer_; }

 private:
  Destination destination_;
  AutofillInternalsBuffer buffer_;
  DISALLOW_COPY_AND_ASSIGN(AutofillInternalsMessage);
};

// Class that forwards log messages to the WebUI for display.
class AutofillInternalsLogging {
 public:
  AutofillInternalsLogging();
  virtual ~AutofillInternalsLogging();

  // Main API function that is called when something is logged.
  static void Log(std::string message);

  static void SetAutofillInternalsLogger(
      std::unique_ptr<AutofillInternalsLogging> logger);

 private:
  static void LogRaw(base::Value&& message);
  virtual void LogHelper(const base::Value& message) = 0;
  virtual void LogRawHelper(const base::Value& message) = 0;

  // Grant access to LogRaw().
  friend class AutofillInternalsMessage;

  DISALLOW_COPY_AND_ASSIGN(AutofillInternalsLogging);
};

bool IsLogAutofillInternalsActive();

#define LOG_AF_INTERNALS                                         \
  AutofillInternalsMessage(                                      \
      AutofillInternalsMessage::Destination::kAutofillInternals) \
      .buffer()

#define LOG_PWMGR_INTERNALS                                             \
  AutofillInternalsMessage(                                             \
      AutofillInternalsMessage::Destination::kPasswordManagerInternals) \
      .buffer()

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_INTERNALS_LOGGING_H_
