// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/common/metafile_utils.h"

#include "base/time/time.h"
#include "printing/common/printing_features.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/include/core/SkTime.h"
#include "third_party/skia/include/docs/SkPDFDocument.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_update.h"

namespace {

SkTime::DateTime TimeToSkTime(base::Time time) {
  base::Time::Exploded exploded;
  time.UTCExplode(&exploded);
  SkTime::DateTime skdate;
  skdate.fTimeZoneMinutes = 0;
  skdate.fYear = exploded.year;
  skdate.fMonth = exploded.month;
  skdate.fDayOfWeek = exploded.day_of_week;
  skdate.fDay = exploded.day_of_month;
  skdate.fHour = exploded.hour;
  skdate.fMinute = exploded.minute;
  skdate.fSecond = exploded.second;
  return skdate;
}

sk_sp<SkPicture> GetEmptyPicture() {
  SkPictureRecorder rec;
  SkCanvas* canvas = rec.beginRecording(100, 100);
  // Add some ops whose net effects equal to a noop.
  canvas->save();
  canvas->restore();
  return rec.finishRecordingAsPicture();
}

// Convert an AXNode into a SkPDF::StructureElementNode in order to make a
// tagged (accessible) PDF. Returns true on success and false if we don't
// have enough data to build a valid tree.
bool RecursiveBuildStructureTree(const ui::AXNode* ax_node,
                                 SkPDF::StructureElementNode* tag) {
  bool valid = false;

  tag->fNodeId = ax_node->GetIntAttribute(ax::mojom::IntAttribute::kDOMNodeId);
  switch (ax_node->data().role) {
    case ax::mojom::Role::kRootWebArea:
      tag->fType = SkPDF::DocumentStructureType::kDocument;
      break;
    case ax::mojom::Role::kParagraph:
      tag->fType = SkPDF::DocumentStructureType::kP;
      break;
    case ax::mojom::Role::kGenericContainer:
      tag->fType = SkPDF::DocumentStructureType::kDiv;
      break;
    case ax::mojom::Role::kHeading:
      // TODO(dmazzoni): heading levels. https://crbug.com/1039816
      tag->fType = SkPDF::DocumentStructureType::kH;
      break;
    case ax::mojom::Role::kList:
      tag->fType = SkPDF::DocumentStructureType::kL;
      break;
    case ax::mojom::Role::kListMarker:
      tag->fType = SkPDF::DocumentStructureType::kLbl;
      break;
    case ax::mojom::Role::kListItem:
      tag->fType = SkPDF::DocumentStructureType::kLI;
      break;
    case ax::mojom::Role::kTable:
      tag->fType = SkPDF::DocumentStructureType::kTable;
      break;
    case ax::mojom::Role::kRow:
      tag->fType = SkPDF::DocumentStructureType::kTR;
      break;
    case ax::mojom::Role::kColumnHeader:
    case ax::mojom::Role::kRowHeader:
      tag->fType = SkPDF::DocumentStructureType::kTH;
      break;
    case ax::mojom::Role::kCell:
      tag->fType = SkPDF::DocumentStructureType::kTD;
      break;
    case ax::mojom::Role::kFigure:
    case ax::mojom::Role::kImage:
      tag->fType = SkPDF::DocumentStructureType::kFigure;
      break;
    case ax::mojom::Role::kStaticText:
      // Currently we're only marking text content, so we can't generate
      // a nonempty structure tree unless we have at least one kStaticText
      // node in the tree.
      tag->fType = SkPDF::DocumentStructureType::kNonStruct;
      valid = true;
      break;
    default:
      tag->fType = SkPDF::DocumentStructureType::kNonStruct;
  }

  tag->fChildCount = ax_node->GetUnignoredChildCount();
  // Allocated here, cleaned up in DestroyStructureElementNodeTree().
  SkPDF::StructureElementNode* children =
      new SkPDF::StructureElementNode[tag->fChildCount];
  tag->fChildren = children;
  for (size_t i = 0; i < tag->fChildCount; i++) {
    bool success = RecursiveBuildStructureTree(
        ax_node->GetUnignoredChildAtIndex(i), &children[i]);
    if (success)
      valid = true;
  }

  return valid;
}

void DestroyStructureElementNodeTree(SkPDF::StructureElementNode* node) {
  for (size_t i = 0; i < node->fChildCount; i++) {
    DestroyStructureElementNodeTree(
        const_cast<SkPDF::StructureElementNode*>(&node->fChildren[i]));
  }
  delete[] node->fChildren;
}

}  // namespace

namespace printing {

sk_sp<SkDocument> MakePdfDocument(const std::string& creator,
                                  const ui::AXTreeUpdate& accessibility_tree,
                                  SkWStream* stream) {
  SkPDF::Metadata metadata;
  SkTime::DateTime now = TimeToSkTime(base::Time::Now());
  metadata.fCreation = now;
  metadata.fModified = now;
  metadata.fCreator = creator.empty()
                          ? SkString("Chromium")
                          : SkString(creator.c_str(), creator.size());
  metadata.fRasterDPI = 300.0f;

  SkPDF::StructureElementNode tag_root = {};
  if (!accessibility_tree.nodes.empty()) {
    ui::AXTree tree(accessibility_tree);
    if (RecursiveBuildStructureTree(tree.root(), &tag_root))
      metadata.fStructureElementTreeRoot = &tag_root;
  }

  sk_sp<SkDocument> document = SkPDF::MakeDocument(stream, metadata);
  DestroyStructureElementNodeTree(&tag_root);
  return document;
}

sk_sp<SkData> SerializeOopPicture(SkPicture* pic, void* ctx) {
  const ContentToProxyIdMap* context =
      reinterpret_cast<const ContentToProxyIdMap*>(ctx);
  uint32_t pic_id = pic->uniqueID();
  auto iter = context->find(pic_id);
  if (iter == context->end())
    return nullptr;

  return SkData::MakeWithCopy(&pic_id, sizeof(pic_id));
}

sk_sp<SkPicture> DeserializeOopPicture(const void* data,
                                       size_t length,
                                       void* ctx) {
  uint32_t pic_id;
  if (length < sizeof(pic_id)) {
    NOTREACHED();  // Should not happen if the content is as written.
    return GetEmptyPicture();
  }
  memcpy(&pic_id, data, sizeof(pic_id));

  auto* context = reinterpret_cast<DeserializationContext*>(ctx);
  auto iter = context->find(pic_id);
  if (iter == context->end() || !iter->second) {
    // When we don't have the out-of-process picture available, we return
    // an empty picture. Returning a nullptr will cause the deserialization
    // crash.
    return GetEmptyPicture();
  }
  return iter->second;
}

SkSerialProcs SerializationProcs(SerializationContext* ctx) {
  SkSerialProcs procs;
  procs.fPictureProc = SerializeOopPicture;
  procs.fPictureCtx = ctx;
  return procs;
}

SkDeserialProcs DeserializationProcs(DeserializationContext* ctx) {
  SkDeserialProcs procs;
  procs.fPictureProc = DeserializeOopPicture;
  procs.fPictureCtx = ctx;
  return procs;
}

}  // namespace printing
