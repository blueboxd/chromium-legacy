// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ACCESSIBILITY_TREE_FORMATTER_H_
#define CONTENT_PUBLIC_BROWSER_ACCESSIBILITY_TREE_FORMATTER_H_

#include <stdint.h>

#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/process/process_handle.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "content/common/content_export.h"
#include "ui/gfx/native_widget_types.h"

namespace base {
class CommandLine;
}

namespace content {

class BrowserAccessibility;
class BrowserAccessibilityManager;

// A utility class for formatting platform-specific accessibility information,
// for use in testing, debugging, and developer tools.
// This is extended by a subclass for each platform where accessibility is
// implemented.
class CONTENT_EXPORT AccessibilityTreeFormatter {
 public:
  virtual ~AccessibilityTreeFormatter() = default;

  // A single property filter specification. See GetAllowString() and
  // GetDenyString() for more information.
  struct PropertyFilter {
    enum Type { ALLOW, ALLOW_EMPTY, DENY };
    base::string16 match_str;
    Type type;

    PropertyFilter(base::string16 match_str, Type type)
        : match_str(match_str), type(type) {}
  };

  // A single node filter specification  which will exclude any node where the
  // value of the named property matches the given pattern.
  //
  // This can be used to exclude nodes based on properties like role, for
  // example to exclude all inlineTextBox nodes under blink we would use a
  // NodeFilter of the form:
  //   {property='internalRole', pattern='inlineTextBox'};
  struct NodeFilter {
    std::string property;
    base::string16 pattern;

    NodeFilter(std::string property, base::string16 pattern)
        : property(property), pattern(pattern) {}
  };

  // Create the appropriate native subclass of AccessibilityTreeFormatter.
  static std::unique_ptr<AccessibilityTreeFormatter> Create();

  // Get a set of factory methods to create tree-formatters, one for each test
  // pass; see |DumpAccessibilityTestBase|.
  using FormatterFactory = std::unique_ptr<AccessibilityTreeFormatter> (*)();
  using CommandLineHelper = void (*)(base::CommandLine* command_line);
  struct TestPass {
    const char* name;
    FormatterFactory create_formatter;
    CommandLineHelper set_up_command_line;
  };
  static std::vector<TestPass> GetTestPasses();

  // Gets the test pass at the given index.
  static TestPass GetTestPass(size_t index);

  virtual void AddDefaultFilters(
      std::vector<PropertyFilter>* property_filters) = 0;

  static bool MatchesPropertyFilters(
      const std::vector<PropertyFilter>& property_filters,
      const base::string16& text,
      bool default_result);

  // Check if the given dictionary matches any of the supplied NodeFilter(s).
  static bool MatchesNodeFilters(const std::vector<NodeFilter>& node_filters,
                                 const base::DictionaryValue& dict);

  // Populates the given DictionaryValue with the accessibility tree.
  // The dictionary contains a key/value pair for each attribute of the node,
  // plus a "children" attribute containing a list of all child nodes.
  // {
  //   "AXName": "node",  /* actual attributes will vary by platform */
  //   "position": {  /* some attributes may be dictionaries */
  //     "x": 0,
  //     "y": 0
  //   },
  //   /* ... more attributes of |node| */
  //   "children": [ {  /* list of children created recursively */
  //     "AXName": "child node 1",
  //     /* ... more attributes */
  //     "children": [ ]
  //   }, {
  //     "AXName": "child name 2",
  //     /* ... more attributes */
  //     "children": [ ]
  //   } ]
  // }
  // Build an accessibility tree for the current Chrome app.
  virtual std::unique_ptr<base::DictionaryValue> BuildAccessibilityTree(
      BrowserAccessibility* root) = 0;

  // Build an accessibility tree for any process with a window.
  virtual std::unique_ptr<base::DictionaryValue>
  BuildAccessibilityTreeForProcess(base::ProcessId pid) = 0;

  // Build an accessibility tree for any window.
  virtual std::unique_ptr<base::DictionaryValue>
  BuildAccessibilityTreeForWindow(gfx::AcceleratedWidget widget) = 0;

  // Build an accessibility tree for an application with a name matching the
  // given pattern.
  virtual std::unique_ptr<base::DictionaryValue>
  BuildAccessibilityTreeForPattern(const base::StringPiece& pattern) = 0;

  // Returns a filtered accesibility tree using the current property and node
  // filters.
  virtual std::unique_ptr<base::DictionaryValue> FilterAccessibilityTree(
      const base::DictionaryValue& dict) = 0;

  // Dumps a BrowserAccessibility tree into a string.
  virtual void FormatAccessibilityTree(BrowserAccessibility* root,
                                       base::string16* contents) = 0;
  virtual void FormatAccessibilityTree(const base::DictionaryValue& tree_node,
                                       base::string16* contents) = 0;

  static base::string16 DumpAccessibilityTreeFromManager(
      BrowserAccessibilityManager* ax_mgr,
      bool internal);

  // Set regular expression filters that apply to each property of every node
  // before it's output.
  virtual void SetPropertyFilters(
      const std::vector<PropertyFilter>& property_filters) = 0;

  // Set regular expression filters that apply to every node before output.
  virtual void SetNodeFilters(const std::vector<NodeFilter>& node_filters) = 0;

  // If true, the internal accessibility id of each node will be included
  // in its output.
  virtual void set_show_ids(bool show_ids) = 0;

  // Suffix of the expectation file corresponding to html file.
  // Overridden by each platform subclass.
  // Example:
  // HTML test:      test-file.html
  // Expected:       test-file-expected-mac.txt.
  virtual const base::FilePath::StringType GetExpectedFileSuffix() = 0;
  // Some Platforms expect different outputs depending on the version.
  // Most test outputs are identical but this allows a version specific
  // expected file to be used.
  virtual const base::FilePath::StringType
  GetVersionSpecificExpectedFileSuffix() = 0;

  // A string that indicates a given line in a file is an allow-empty,
  // allow or deny filter. Overridden by each platform subclass. Example:
  // Mac values:
  //   GetAllowEmptyString() -> "@MAC-ALLOW-EMPTY:"
  //   GetAllowString() -> "@MAC-ALLOW:"
  //   GetDenyString() -> "@MAC-DENY:"
  //   GetDenyNodeString() -> "@MAC-DENY-NODE:"
  // Example html:
  // <!--
  // @MAC-ALLOW-EMPTY:description*
  // @MAC-ALLOW:roleDescription*
  // @MAC-DENY:subrole*
  // @BLINK-DENY-NODE:internalRole=inlineTextBox
  // -->
  // <p>Text</p>
  virtual const std::string GetAllowEmptyString() = 0;
  virtual const std::string GetAllowString() = 0;
  virtual const std::string GetDenyString() = 0;
  virtual const std::string GetDenyNodeString() = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_FORMATTER_H_
