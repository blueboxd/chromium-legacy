// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_language_detection.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/command_line.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/accessibility_switches.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_tree.h"

namespace ui {

TEST(AXLanguageDetectionTest, FeatureFlag) {
  // TODO(crbug/889370): Remove this test once this feature is stable
  EXPECT_FALSE(
      ::switches::IsExperimentalAccessibilityLanguageDetectionEnabled());

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ::switches::kEnableExperimentalAccessibilityLanguageDetection);

  EXPECT_TRUE(
      ::switches::IsExperimentalAccessibilityLanguageDetectionEnabled());
}

TEST(AXLanguageDetectionTest, DynamicContentFeatureFlag) {
  // TODO(crbug/889370): Remove this test once this feature is stable
  EXPECT_FALSE(
      ::switches::IsExperimentalAccessibilityLanguageDetectionDynamicEnabled());

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ::switches::kEnableExperimentalAccessibilityLanguageDetectionDynamic);

  EXPECT_TRUE(
      ::switches::IsExperimentalAccessibilityLanguageDetectionDynamicEnabled());
}

// Tests that AXNode::GetLanguage() terminates when there is no lang attribute.
TEST(AXLanguageDetectionTest, BoringTree) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ::switches::kEnableExperimentalAccessibilityLanguageDetection);

  // This test checks the behaviour of Detect, Label, and GetLanguage on a
  // 'boring' tree.
  //
  // The tree built here contains no lang attributes, nor does it contain any
  // text to perform detection on.
  //
  // Tree:
  //      1
  //    2   3
  //  4
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(4);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].child_ids.resize(2);
  initial_state.nodes[0].child_ids[0] = 2;
  initial_state.nodes[0].child_ids[1] = 3;
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].child_ids.resize(1);
  initial_state.nodes[1].child_ids[0] = 4;
  initial_state.nodes[2].id = 3;
  initial_state.nodes[3].id = 4;

  AXTree tree(initial_state);
  ASSERT_NE(tree.language_detection_manager, nullptr);
  tree.language_detection_manager->DetectLanguageForSubtree(tree.root());
  tree.language_detection_manager->LabelLanguageForSubtree(tree.root());

  // Check that tree parenting conforms to expected shape.
  AXNode* node1 = tree.GetFromId(1);
  EXPECT_EQ(node1->parent(), nullptr);

  AXNode* node2 = tree.GetFromId(2);
  ASSERT_EQ(node2->parent(), node1);
  EXPECT_EQ(node2->parent()->parent(), nullptr);

  AXNode* node3 = tree.GetFromId(3);
  ASSERT_EQ(node3->parent(), node1);
  EXPECT_EQ(node3->parent()->parent(), nullptr);

  AXNode* node4 = tree.GetFromId(4);
  ASSERT_EQ(node4->parent(), node2);
  ASSERT_EQ(node4->parent()->parent(), node1);
  EXPECT_EQ(node4->parent()->parent()->parent(), nullptr);

  EXPECT_EQ(node1->GetLanguage(), "");
  EXPECT_EQ(node2->GetLanguage(), "");
  EXPECT_EQ(node3->GetLanguage(), "");
  EXPECT_EQ(node4->GetLanguage(), "");
}

TEST(AXLanguageDetectionTest, LangAttrInheritanceFeatureFlagOff) {
  // Test lang attribute inheritance when feature flag is off.
  //
  // Lang attribute inheritance is handled by GetLanguage.
  //
  // Tree:
  //        1
  //      2   3
  //    4
  //  5
  //
  //  1 - English lang attribute
  //  2 - French lang attribute
  //
  //  Expected:
  //    3 - inherit English from 1
  //    4 - inherit French from 2
  //    5 - inherit Frnech from 4/2
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(5);

  {
    AXNodeData& node1 = initial_state.nodes[0];
    node1.id = 1;
    node1.role = ax::mojom::Role::kGenericContainer;
    node1.child_ids.resize(2);
    node1.child_ids[0] = 2;
    node1.child_ids[1] = 3;
    node1.AddStringAttribute(ax::mojom::StringAttribute::kLanguage, "en");
  }

  {
    AXNodeData& node2 = initial_state.nodes[1];
    node2.id = 2;
    node2.role = ax::mojom::Role::kGenericContainer;
    node2.child_ids.resize(1);
    node2.child_ids[0] = 4;
    node2.AddStringAttribute(ax::mojom::StringAttribute::kLanguage, "fr");
  }

  {
    AXNodeData& node3 = initial_state.nodes[2];
    node3.id = 3;
    node3.role = ax::mojom::Role::kStaticText;
  }

  {
    AXNodeData& node4 = initial_state.nodes[3];
    node4.id = 4;
    node4.role = ax::mojom::Role::kStaticText;
    node4.child_ids.resize(1);
    node4.child_ids[0] = 5;
  }

  {
    AXNodeData& node5 = initial_state.nodes[4];
    node5.id = 5;
    node5.role = ax::mojom::Role::kInlineTextBox;
  }

  AXTree tree(initial_state);
  ASSERT_NE(tree.language_detection_manager, nullptr);
  tree.language_detection_manager->DetectLanguageForSubtree(tree.root());
  tree.language_detection_manager->LabelLanguageForSubtree(tree.root());

  {
    AXNode* node1 = tree.GetFromId(1);
    EXPECT_EQ(node1->GetLanguageInfo(), nullptr);
    EXPECT_EQ(node1->GetLanguage(), "en");
  }

  {
    AXNode* node2 = tree.GetFromId(2);
    EXPECT_EQ(node2->GetLanguageInfo(), nullptr);
    EXPECT_EQ(node2->GetLanguage(), "fr");
  }

  {
    AXNode* node3 = tree.GetFromId(3);
    EXPECT_EQ(node3->GetLanguageInfo(), nullptr);
    EXPECT_EQ(node3->GetLanguage(), "en");
  }

  {
    AXNode* node4 = tree.GetFromId(4);
    EXPECT_EQ(node4->GetLanguageInfo(), nullptr);
    EXPECT_EQ(node4->GetLanguage(), "fr");
  }

  {
    AXNode* node5 = tree.GetFromId(5);
    EXPECT_EQ(node5->GetLanguageInfo(), nullptr);
    EXPECT_EQ(node5->GetLanguage(), "fr");
  }
}

TEST(AXLanguageDetectionTest, LangAttrInheritanceFeatureFlagOn) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ::switches::kEnableExperimentalAccessibilityLanguageDetection);

  // Test lang attribute inheritance in the absence of any detected language.
  //
  // Lang attribute inheritance is handled by the Label step.
  //
  // Tree:
  //        1
  //      2   3
  //    4
  //  5
  //
  //  1 - English lang attribute
  //  2 - French lang attribute
  //
  //  Expected:
  //    3 - inherit English from 1
  //    4 - inherit French from 2
  //    5 - inherit Frnech from 4/2

  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(5);

  {
    AXNodeData& node1 = initial_state.nodes[0];
    node1.id = 1;
    node1.role = ax::mojom::Role::kGenericContainer;
    node1.child_ids.resize(2);
    node1.child_ids[0] = 2;
    node1.child_ids[1] = 3;
    node1.AddStringAttribute(ax::mojom::StringAttribute::kLanguage, "en");
  }

  {
    AXNodeData& node2 = initial_state.nodes[1];
    node2.id = 2;
    node2.role = ax::mojom::Role::kGenericContainer;
    node2.child_ids.resize(1);
    node2.child_ids[0] = 4;
    node2.AddStringAttribute(ax::mojom::StringAttribute::kLanguage, "fr");
  }

  {
    AXNodeData& node3 = initial_state.nodes[2];
    node3.id = 3;
    node3.role = ax::mojom::Role::kStaticText;
  }

  {
    AXNodeData& node4 = initial_state.nodes[3];
    node4.id = 4;
    node4.role = ax::mojom::Role::kStaticText;
    node4.child_ids.resize(1);
    node4.child_ids[0] = 5;
  }

  {
    AXNodeData& node5 = initial_state.nodes[4];
    node5.id = 5;
    node5.role = ax::mojom::Role::kInlineTextBox;
  }

  AXTree tree(initial_state);
  ASSERT_NE(tree.language_detection_manager, nullptr);
  tree.language_detection_manager->DetectLanguageForSubtree(tree.root());
  tree.language_detection_manager->LabelLanguageForSubtree(tree.root());

  {
    AXNode* node1 = tree.GetFromId(1);
    // No detection for non text nodes.
    EXPECT_EQ(node1->GetLanguageInfo(), nullptr);
    EXPECT_EQ(node1->GetLanguage(), "en");
  }

  {
    AXNode* node2 = tree.GetFromId(2);
    EXPECT_EQ(node2->GetLanguageInfo(), nullptr);
    EXPECT_EQ(node2->GetLanguage(), "fr");
  }

  {
    AXNode* node3 = tree.GetFromId(3);
    EXPECT_NE(node3->GetLanguageInfo(), nullptr);
    EXPECT_EQ(node3->GetLanguage(), "en");
  }

  {
    AXNode* node4 = tree.GetFromId(4);
    EXPECT_NE(node4->GetLanguageInfo(), nullptr);
    EXPECT_EQ(node4->GetLanguage(), "fr");
  }

  {
    AXNode* node5 = tree.GetFromId(5);
    EXPECT_NE(node5->GetLanguageInfo(), nullptr);
    EXPECT_EQ(node5->GetLanguage(), "fr");
  }
}

TEST(AXLanguageDetectionTest, LanguageDetectionBasic) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ::switches::kEnableExperimentalAccessibilityLanguageDetection);

  // Tree:
  //        1
  //      2   3
  //    4
  //  5
  //
  //  1 - German lang attribute,  no text
  //  2 - French lang attribute,  no text
  //  3 - no attribute,           French text
  //  4 - no attribute,           English text
  //  5 - no attribute,           no text
  //
  //  Expected:
  //    3 - French detected
  //    4 - English detected
  //    5 - inherit English from 4
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(5);

  {
    AXNodeData& node1 = initial_state.nodes[0];
    node1.id = 1;
    node1.role = ax::mojom::Role::kGenericContainer;
    node1.child_ids.resize(2);
    node1.child_ids[0] = 2;
    node1.child_ids[1] = 3;
    node1.AddStringAttribute(ax::mojom::StringAttribute::kLanguage, "de");
  }

  {
    AXNodeData& node2 = initial_state.nodes[1];
    node2.id = 2;
    node2.role = ax::mojom::Role::kGenericContainer;
    node2.child_ids.resize(1);
    node2.child_ids[0] = 4;
    node2.AddStringAttribute(ax::mojom::StringAttribute::kLanguage, "fr");
  }

  {
    AXNodeData& node3 = initial_state.nodes[2];
    node3.id = 3;
    node3.role = ax::mojom::Role::kStaticText;
    std::string node3_text =
        "Ce texte a été créé avec Google Translate, il est peu probable qu'il "
        "soit idiomatique dans la langue cible indiquée Ce texte est "
        "uniquement utilisé pour tester la détection de la langue.";
    node3.AddStringAttribute(ax::mojom::StringAttribute::kName, node3_text);
  }

  {
    AXNodeData& node4 = initial_state.nodes[3];
    node4.id = 4;
    node4.child_ids.resize(1);
    node4.child_ids[0] = 5;
    node4.role = ax::mojom::Role::kStaticText;
    std::string node4_text =
        "This is text created using Google Translate, it is unlikely to be "
        "idiomatic in the given target language. This text is only used to "
        "test language detection";
    node4.AddStringAttribute(ax::mojom::StringAttribute::kName, node4_text);
  }

  {
    AXNodeData& node5 = initial_state.nodes[4];
    node5.id = 5;
    node5.role = ax::mojom::Role::kInlineTextBox;
  }

  AXTree tree(initial_state);
  ASSERT_NE(tree.language_detection_manager, nullptr);
  tree.language_detection_manager->DetectLanguageForSubtree(tree.root());
  tree.language_detection_manager->LabelLanguageForSubtree(tree.root());

  {
    AXNode* node1 = tree.GetFromId(1);
    // node1 is not a text node, so no lang info should be attached.
    EXPECT_EQ(node1->GetLanguageInfo(), nullptr);
    EXPECT_EQ(node1->GetLanguage(), "de");
  }

  {
    AXNode* node2 = tree.GetFromId(2);
    // node2 is not a text node, so no lang info should be attached.
    EXPECT_EQ(node2->GetLanguageInfo(), nullptr);
    EXPECT_EQ(node2->GetLanguage(), "fr");
  }

  {
    AXNode* node3 = tree.GetFromId(3);
    EXPECT_TRUE(node3->IsText());
    EXPECT_NE(node3->GetLanguageInfo(), nullptr);
    EXPECT_EQ(node3->GetLanguage(), "fr");
  }

  {
    AXNode* node4 = tree.GetFromId(4);
    EXPECT_TRUE(node4->IsText());
    EXPECT_NE(node4->GetLanguageInfo(), nullptr);
    EXPECT_EQ(node4->GetLanguage(), "en");
  }

  {
    AXNode* node5 = tree.GetFromId(5);
    EXPECT_TRUE(node5->IsText());
    EXPECT_NE(node5->GetLanguageInfo(), nullptr);
    EXPECT_EQ(node5->GetLanguage(), "en");
  }
}

TEST(AXLanguageDetectionTest, LanguageDetectionDetectOnly) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ::switches::kEnableExperimentalAccessibilityLanguageDetection);

  // This tests a Detect step without any matching Label step.
  //
  // Tree:
  //        1
  //      2   3
  //    4
  //  5
  //
  //  1 - German lang attribute, no text
  //  2 - French lang attribute,  no text
  //  3 - no attribute,           French text
  //  4 - no attribute,           English text
  //  5 - no attribute,           no text
  //
  //  Expected:
  //    3 - French detected, never labelled, so still inherits German from 1
  //    4 - English detected, never labelled, so still inherits French from 2
  //    5 - English inherited from 4, still inherits French from 4
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(5);

  {
    AXNodeData& node1 = initial_state.nodes[0];
    node1.id = 1;
    node1.role = ax::mojom::Role::kGenericContainer;
    node1.child_ids.resize(2);
    node1.child_ids[0] = 2;
    node1.child_ids[1] = 3;
    node1.AddStringAttribute(ax::mojom::StringAttribute::kLanguage, "de");
  }

  {
    AXNodeData& node2 = initial_state.nodes[1];
    node2.id = 2;
    node2.role = ax::mojom::Role::kGenericContainer;
    node2.child_ids.resize(1);
    node2.child_ids[0] = 4;
    node2.AddStringAttribute(ax::mojom::StringAttribute::kLanguage, "fr");
  }

  {
    AXNodeData& node3 = initial_state.nodes[2];
    node3.id = 3;
    node3.role = ax::mojom::Role::kStaticText;
    std::string node3_text =
        "Ce texte a été créé avec Google Translate, il est peu probable qu'il "
        "soit idiomatique dans la langue cible indiquée Ce texte est "
        "uniquement utilisé pour tester la détection de la langue.";
    node3.AddStringAttribute(ax::mojom::StringAttribute::kName, node3_text);
  }

  {
    AXNodeData& node4 = initial_state.nodes[3];
    node4.id = 4;
    node4.child_ids.resize(1);
    node4.child_ids[0] = 5;
    node4.role = ax::mojom::Role::kStaticText;
    std::string node4_text =
        "This is text created using Google Translate, it is unlikely to be "
        "idiomatic in the given target language. This text is only used to "
        "test language detection";
    node4.AddStringAttribute(ax::mojom::StringAttribute::kName, node4_text);
  }

  {
    AXNodeData& node5 = initial_state.nodes[4];
    node5.id = 5;
    node5.role = ax::mojom::Role::kInlineTextBox;
  }

  AXTree tree(initial_state);
  ASSERT_NE(tree.language_detection_manager, nullptr);
  tree.language_detection_manager->DetectLanguageForSubtree(tree.root());
  // Purposefully not calling Label so we can test Detect in isolation.

  {
    AXNode* node1 = tree.GetFromId(1);
    // node1 is not a text node, so no lang info should be attached.
    EXPECT_EQ(node1->GetLanguageInfo(), nullptr);
    EXPECT_EQ(node1->GetLanguage(), "de");
  }

  {
    AXNode* node2 = tree.GetFromId(2);
    // node2 is not a text node, so no lang info should be attached.
    EXPECT_EQ(node2->GetLanguageInfo(), nullptr);
    EXPECT_EQ(node2->GetLanguage(), "fr");
  }

  {
    AXNode* node3 = tree.GetFromId(3);
    EXPECT_TRUE(node3->IsText());
    ASSERT_NE(node3->GetLanguageInfo(), nullptr);
    ASSERT_GT(node3->GetLanguageInfo()->detected_languages.size(), (unsigned)0);
    ASSERT_EQ(node3->GetLanguageInfo()->detected_languages[0], "fr");
    EXPECT_TRUE(node3->GetLanguageInfo()->language.empty());
    EXPECT_EQ(node3->GetLanguage(), "de");
  }

  {
    AXNode* node4 = tree.GetFromId(4);
    EXPECT_TRUE(node4->IsText());
    ASSERT_NE(node4->GetLanguageInfo(), nullptr);
    ASSERT_GT(node4->GetLanguageInfo()->detected_languages.size(), (unsigned)0);
    ASSERT_EQ(node4->GetLanguageInfo()->detected_languages[0], "en");
    EXPECT_TRUE(node4->GetLanguageInfo()->language.empty());
    EXPECT_EQ(node4->GetLanguage(), "fr");
  }

  {
    AXNode* node5 = tree.GetFromId(5);
    EXPECT_TRUE(node5->IsText());
    ASSERT_NE(node5->GetLanguageInfo(), nullptr);
    ASSERT_EQ(node5->GetLanguageInfo()->detected_languages.size(), (unsigned)0);
    EXPECT_TRUE(node5->GetLanguageInfo()->language.empty());
    EXPECT_EQ(node5->GetLanguage(), "fr");
  }
}

TEST(AXLanguageDetectionTest, kLanguageUntouched) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ::switches::kEnableExperimentalAccessibilityLanguageDetection);

  // This test is to ensure that the kLanguage string attribute is not updated
  // during language detection and labelling, even when it disagrees with the
  // detected language.

  // Built tree:
  //        1
  //      2   3
  //
  //  1 - German lang attribute,  no text
  //  2 - English lang attribute, French text
  //  3 - French lang attribute,  English text
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(3);

  {
    AXNodeData& node1 = initial_state.nodes[0];
    node1.id = 1;
    node1.role = ax::mojom::Role::kGenericContainer;
    node1.child_ids.resize(2);
    node1.child_ids[0] = 2;
    node1.child_ids[1] = 3;
    node1.AddStringAttribute(ax::mojom::StringAttribute::kLanguage, "de");
  }

  {
    AXNodeData& node2 = initial_state.nodes[1];
    node2.id = 2;
    node2.role = ax::mojom::Role::kStaticText;
    node2.AddStringAttribute(ax::mojom::StringAttribute::kLanguage, "en");
    std::string node2_text =
        "Ce texte a été créé avec Google Translate, il est peu probable qu'il "
        "soit idiomatique dans la langue cible indiquée Ce texte est "
        "uniquement utilisé pour tester la détection de la langue.";
    node2.AddStringAttribute(ax::mojom::StringAttribute::kName, node2_text);
  }

  {
    AXNodeData& node3 = initial_state.nodes[2];
    node3.id = 3;
    node3.role = ax::mojom::Role::kStaticText;
    node3.AddStringAttribute(ax::mojom::StringAttribute::kLanguage, "fr");
    std::string node3_text =
        "This is text created using Google Translate, it is unlikely to be "
        "idiomatic in the given target language. This text is only used to "
        "test language detection";
    node3.AddStringAttribute(ax::mojom::StringAttribute::kName, node3_text);
  }

  AXTree tree(initial_state);
  ASSERT_NE(tree.language_detection_manager, nullptr);
  tree.language_detection_manager->DetectLanguageForSubtree(tree.root());
  tree.language_detection_manager->LabelLanguageForSubtree(tree.root());

  {
    AXNode* node1 = tree.GetFromId(1);
    ASSERT_EQ(node1->GetLanguageInfo(), nullptr);
    EXPECT_EQ(node1->GetLanguage(), "de");
  }

  {
    // French should be detected, original English attr should be untouched.
    AXNode* node2 = tree.GetFromId(2);
    ASSERT_NE(node2->GetLanguageInfo(), nullptr);
    EXPECT_EQ(node2->GetLanguageInfo()->language, "fr");
    EXPECT_EQ(node2->GetStringAttribute(ax::mojom::StringAttribute::kLanguage),
              "en");
    EXPECT_EQ(node2->GetLanguage(), "fr");
  }

  {
    // English should be detected, original French attr should be untouched.
    AXNode* node3 = tree.GetFromId(3);
    ASSERT_NE(node3->GetLanguageInfo(), nullptr);
    EXPECT_EQ(node3->GetLanguageInfo()->language, "en");
    EXPECT_EQ(node3->GetStringAttribute(ax::mojom::StringAttribute::kLanguage),
              "fr");
    EXPECT_EQ(node3->GetLanguage(), "en");
  }
}

TEST(AXLanguageDetectionTest, AXLanguageInfoStatsBasic) {
  AXLanguageInfoStats stats;

  {
    std::vector<std::string> detected_languages;
    detected_languages.push_back("en");
    detected_languages.push_back("fr");
    detected_languages.push_back("ja");
    stats.Add(detected_languages);
  }

  ASSERT_EQ(stats.GetScore("en"), 3);
  ASSERT_EQ(stats.GetScore("fr"), 2);
  ASSERT_EQ(stats.GetScore("ja"), 1);

  EXPECT_TRUE(stats.CheckLanguageWithinTop("en"));
  EXPECT_TRUE(stats.CheckLanguageWithinTop("fr"));
  EXPECT_TRUE(stats.CheckLanguageWithinTop("ja"));

  {
    std::vector<std::string> detected_languages;
    detected_languages.push_back("en");
    detected_languages.push_back("de");
    detected_languages.push_back("fr");
    stats.Add(detected_languages);
  }

  ASSERT_EQ(stats.GetScore("en"), 6);
  ASSERT_EQ(stats.GetScore("fr"), 3);
  ASSERT_EQ(stats.GetScore("de"), 2);
  ASSERT_EQ(stats.GetScore("ja"), 1);

  EXPECT_TRUE(stats.CheckLanguageWithinTop("en"));
  EXPECT_TRUE(stats.CheckLanguageWithinTop("fr"));
  EXPECT_TRUE(stats.CheckLanguageWithinTop("de"));

  EXPECT_FALSE(stats.CheckLanguageWithinTop("ja"));

  {
    std::vector<std::string> detected_languages;
    detected_languages.push_back("fr");
    stats.Add(detected_languages);
  }

  ASSERT_EQ(stats.GetScore("en"), 6);
  ASSERT_EQ(stats.GetScore("fr"), 6);
  ASSERT_EQ(stats.GetScore("de"), 2);
  ASSERT_EQ(stats.GetScore("ja"), 1);

  EXPECT_TRUE(stats.CheckLanguageWithinTop("en"));
  EXPECT_TRUE(stats.CheckLanguageWithinTop("fr"));
  EXPECT_TRUE(stats.CheckLanguageWithinTop("de"));

  EXPECT_FALSE(stats.CheckLanguageWithinTop("ja"));

  {
    std::vector<std::string> detected_languages;
    detected_languages.push_back("ja");
    detected_languages.push_back("qq");
    detected_languages.push_back("zz");
    stats.Add(detected_languages);
  }

  ASSERT_EQ(stats.GetScore("en"), 6);
  ASSERT_EQ(stats.GetScore("fr"), 6);
  ASSERT_EQ(stats.GetScore("ja"), 4);
  ASSERT_EQ(stats.GetScore("de"), 2);
  ASSERT_EQ(stats.GetScore("qq"), 2);
  ASSERT_EQ(stats.GetScore("zz"), 1);

  EXPECT_TRUE(stats.CheckLanguageWithinTop("en"));
  EXPECT_TRUE(stats.CheckLanguageWithinTop("fr"));
  EXPECT_TRUE(stats.CheckLanguageWithinTop("ja"));

  EXPECT_FALSE(stats.CheckLanguageWithinTop("de"));
  EXPECT_FALSE(stats.CheckLanguageWithinTop("qq"));
  EXPECT_FALSE(stats.CheckLanguageWithinTop("zz"));
}

TEST(AXLanguageDetectionTest, ShortLanguageDetectorLabeledTest) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ::switches::kEnableExperimentalAccessibilityLanguageDetection);
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(2);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].child_ids = {2};
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].AddStringAttribute(ax::mojom::StringAttribute::kName,
                                            "Hello");
  initial_state.nodes[1].AddStringAttribute(
      ax::mojom::StringAttribute::kLanguage, "en");
  AXTree tree(initial_state);

  AXNode* item = tree.GetFromId(2);
  std::vector<AXLanguageSpan> annotation;
  ASSERT_NE(tree.language_detection_manager, nullptr);
  // Empty output.
  annotation =
      tree.language_detection_manager->GetLanguageAnnotationForStringAttribute(
          *item, ax::mojom::StringAttribute::kInnerHtml);
  ASSERT_EQ(0, (int)annotation.size());
  // Returns single AXLanguageSpan.
  annotation =
      tree.language_detection_manager->GetLanguageAnnotationForStringAttribute(
          *item, ax::mojom::StringAttribute::kName);
  ASSERT_EQ(1, (int)annotation.size());
  AXLanguageSpan* lang_span = &annotation[0];
  ASSERT_EQ("en", lang_span->language);
  std::string name =
      item->GetStringAttribute(ax::mojom::StringAttribute::kName);
  ASSERT_EQ("Hello",
            name.substr(lang_span->start_index,
                        lang_span->end_index - lang_span->start_index));
}

TEST(AXLanguageDetectionTest, ShortLanguageDetectorCharacterTest) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ::switches::kEnableExperimentalAccessibilityLanguageDetection);
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(2);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].child_ids = {2};
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].AddStringAttribute(ax::mojom::StringAttribute::kName,
                                            "δ");
  AXTree tree(initial_state);

  AXNode* item = tree.GetFromId(2);
  std::vector<AXLanguageSpan> annotation;
  ASSERT_NE(tree.language_detection_manager, nullptr);
  // Returns single LanguageSpan.
  annotation =
      tree.language_detection_manager->GetLanguageAnnotationForStringAttribute(
          *item, ax::mojom::StringAttribute::kName);
  ASSERT_EQ(1, (int)annotation.size());
  AXLanguageSpan* lang_span = &annotation[0];
  ASSERT_EQ("el", lang_span->language);
  std::string name =
      item->GetStringAttribute(ax::mojom::StringAttribute::kName);
  ASSERT_EQ("δ", name.substr(lang_span->start_index,
                             lang_span->end_index - lang_span->start_index));
}

TEST(AXLanguageDetectionTest, ShortLanguageDetectorMultipleLanguagesTest) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ::switches::kEnableExperimentalAccessibilityLanguageDetection);
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(2);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].child_ids = {2};
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].AddStringAttribute(
      ax::mojom::StringAttribute::kName,
      "This text should be read in English. 차에 한하여 중임할 수. Followed "
      "by English.");
  AXTree tree(initial_state);

  AXNode* item = tree.GetFromId(2);
  ASSERT_NE(tree.language_detection_manager, nullptr);
  std::vector<AXLanguageSpan> annotation =
      tree.language_detection_manager->GetLanguageAnnotationForStringAttribute(
          *item, ax::mojom::StringAttribute::kName);
  ASSERT_EQ(3, (int)annotation.size());
  std::string name =
      item->GetStringAttribute(ax::mojom::StringAttribute::kName);
  AXLanguageSpan* lang_span = &annotation[0];
  ASSERT_EQ("This text should be read in English. ",
            name.substr(lang_span->start_index,
                        lang_span->end_index - lang_span->start_index));
  lang_span = &annotation[1];
  ASSERT_EQ("차에 한하여 중임할 수. ",
            name.substr(lang_span->start_index,
                        lang_span->end_index - lang_span->start_index));
  lang_span = &annotation[2];
  ASSERT_EQ("Followed by English.",
            name.substr(lang_span->start_index,
                        lang_span->end_index - lang_span->start_index));
}

// Assert that GetLanguageAnnotationForStringAttribute works for attributes
// other than kName.
TEST(AXLanguageDetectionTest, DetectLanguageForRoleTest) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ::switches::kEnableExperimentalAccessibilityLanguageDetection);
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].AddStringAttribute(ax::mojom::StringAttribute::kValue,
                                            "どうぞよろしくお願いします.");
  AXTree tree(initial_state);

  AXNode* item = tree.GetFromId(1);
  ASSERT_NE(tree.language_detection_manager, nullptr);
  std::vector<AXLanguageSpan> annotation =
      tree.language_detection_manager->GetLanguageAnnotationForStringAttribute(
          *item, ax::mojom::StringAttribute::kValue);
  ASSERT_EQ(1, (int)annotation.size());
  std::string value =
      item->GetStringAttribute(ax::mojom::StringAttribute::kValue);
  AXLanguageSpan* lang_span = &annotation[0];
  ASSERT_EQ("どうぞよろしくお願いします.",
            value.substr(lang_span->start_index,
                         lang_span->end_index - lang_span->start_index));
}

}  // namespace ui
