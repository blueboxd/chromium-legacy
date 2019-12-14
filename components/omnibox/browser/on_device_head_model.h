// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_ON_DEVICE_HEAD_MODEL_H_
#define COMPONENTS_OMNIBOX_BROWSER_ON_DEVICE_HEAD_MODEL_H_

#include <fstream>
#include <list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"

// On device head suggest feature uses an on device model which encodes some
// top queries into a radix tree (https://en.wikipedia.org/wiki/Radix_tree), to
// help users quickly get head suggestions when they are under poor network
// condition. When serving, it performs a search in the tree similar as BFS but
// only keeping children with high scores, to find top N queries which match
// the given prefix.
//
// Each node in the tree is encoded using following format to optimize storage
// (see on_device_head_model_unittest.cc for an example tree model):
// ------------------------------------------------------------------------
// | max_score_as_root | child_0 | child_1 | ... | child_n-1 | 0 (1 byte) |
// ------------------------------------------------------------------------
//
// Usage of each block in the node:
// 1) Block max_score_as_root at the beginning of each node contains the
// maximum leaf score can be found in its subtree, which is used for pruning
// during tree traversal to improve the search performance: for example,
// imagining we have already visited some nodes, sorted them based on their
// scores and saved some of them in a structure; now we meet a node with higher
// max_score_as_root, since we know we should only show users top N suggestions
// with highest scores, we can quickly determine whether we can discard some
// node with lower max_score_as_root without physically visiting any of its
// children, as none of the children has a score higher than this low
// max_score_as_root.
// This block has following format:
//    --------------------------------------
//    | 1 (1 bit) | score_max | leaf_score |
//    --------------------------------------
//                    OR
//    -------------------------
//    | 0 (1 bit) | score_max |
///   -------------------------
//   1-bit indicator: whether there is a leaf_score at the end of this block.
//   score_max: the maximum leaf_score can be found if using current node as
//      root.
//   leaf_score: only exists when indicator is 1; it is the score of some
//      complete suggestion ends at current node.
//
// 2) Block child_i (0 <= i <= n-1) has following format:
//    -------------------------------------------------------------
//    | length of text (1 byte) | text | 1 | address of next node |
//    -------------------------------------------------------------
//                                OR
//    ---------------------------------------------------
//    | length of text (1 byte) | text | 0 | leaf_score |
//    ---------------------------------------------------
// We use 1 bit after text field as an indicator to determine whether this child
// is an intermediate node or leaf node. If it is a leaf node, the sequence of
// texts visited so far from the start node to here can be returned as a valid
// suggestion to users with leaf_score.
//
// The size of score and address will be given in the first two bytes of the
// model file.

// TODO(crbug.com/925072): make some cleanups after converting this class into
// a static class, e.g. move private class functions into anonymous namespace.
class OnDeviceHeadModel {
 public:
  // Gets top "max_num_matches_to_return" suggestions and their scores which
  // matches given prefix.
  static std::vector<std::pair<std::string, uint32_t>> GetSuggestionsForPrefix(
      const std::string& model_filename,
      const uint32_t max_num_matches_to_return,
      const std::string& prefix);

 private:
  // A useful data structure to keep track of the tree nodes should be and have
  // been visited during tree traversal.
  struct MatchCandidate {
    // The sequences of characters from the start node to current node.
    std::string text;

    // Whether the text above can be returned as a suggestion; if false it is
    // the prefix of some other complete suggestion.
    bool is_complete_suggestion;

    // If is_complete_suggestion is true, this is the score for the suggestion;
    // Otherwise it will be set as max_score_as_root of the node.
    uint32_t score;

    // The address of the node in the model file. It is not required if
    // is_complete_suggestion is true.
    uint32_t address;
  };

  // Doubly linked list structure, which will be sorted based on candidates'
  // scores (from low to high), to track nodes during tree search. We use two of
  // this list to keep max_num_matches_to_return_ nodes in total with
  // highest score during the search, and prune children and branches with low
  // score.
  // In theory, using RBTree might give a better search performance
  // (i.e. log(n)) compared with linear from linked list here when inserting
  // new candidates with high score into the struct, but since n is usually
  // small, using linked list shall be okay.
  using CandidateQueue = std::list<MatchCandidate>;

  // A mini class holds all parameters needed to access the model on disk.
  class OnDeviceModelParams {
   public:
    static std::unique_ptr<OnDeviceModelParams> Create(
        const std::string& model_filename,
        const uint32_t max_num_matches_to_return);

    std::ifstream* GetModelFileStream() { return &model_filestream_; }
    uint32_t score_size() const { return score_size_; }
    uint32_t address_size() const { return address_size_; }
    uint32_t max_num_matches_to_return() const {
      return max_num_matches_to_return_;
    }

    ~OnDeviceModelParams();

   private:
    OnDeviceModelParams();

    std::ifstream model_filestream_;
    uint32_t score_size_;
    uint32_t address_size_;
    uint32_t max_num_matches_to_return_;

    DISALLOW_COPY_AND_ASSIGN(OnDeviceModelParams);
  };

  static void InsertCandidateToQueue(const MatchCandidate& candidate,
                                     CandidateQueue* leaf_queue,
                                     CandidateQueue* non_leaf_queue);

  static uint32_t GetMinScoreFromQueues(OnDeviceModelParams* params,
                                        const CandidateQueue& queue_1,
                                        const CandidateQueue& queue_2);

  // Finds start node which matches given prefix, returns true if found and
  // the start node using param match_candidate.
  static bool FindStartNode(OnDeviceModelParams* params,
                            const std::string& prefix,
                            MatchCandidate* match_candidate);

  // Reads tree node from given match candidate, convert all possible
  // suggestions and children of this node into structure MatchCandidate.
  static std::vector<MatchCandidate> ReadTreeNode(
      OnDeviceModelParams* params,
      const MatchCandidate& current);

  // Reads block max_score_as_root at the beginning of the node from the given
  // address. If there is a leaf score at the end of the block, return the leaf
  // score using param leaf_candidate;
  static uint32_t ReadMaxScoreAsRoot(OnDeviceModelParams* params,
                                     uint32_t address,
                                     MatchCandidate* leaf_candidate,
                                     bool* is_successful);

  // Reads a child block and move ifstream cursor to next child; returns false
  // when reaching the end of the node or ifstream read error happens.
  static bool ReadNextChild(OnDeviceModelParams* params,
                            MatchCandidate* candidate);

  // Performs a search starting from the address specified by start_match and
  // returns max_num_matches_to_return_ number of complete suggestions with
  // highest scores.
  static std::vector<std::pair<std::string, uint32_t>> DoSearch(
      OnDeviceModelParams* params,
      const MatchCandidate& start_match);

  // Reads next num_bytes from the file stream.
  static bool ReadNextNumBytes(OnDeviceModelParams* params,
                               uint32_t num_bytes,
                               char* buf);
  static uint32_t ReadNextNumBytesAsInt(OnDeviceModelParams* params,
                                        uint32_t num_bytes,
                                        bool* is_successful);

  // Checks if size of score and size of address read from the model file are
  // valid.
  // For score, we use size of 2 bytes (15 bits), 3 bytes (23 bits) or 4 bytes
  // (31 bits); For address, we use size of 3 bytes (23 bits) or 4 bytes
  // (31 bits).
  static bool AreSizesValid(OnDeviceModelParams* params);

  static bool OpenModelFileStream(OnDeviceModelParams* params,
                                  const std::string& model_filename,
                                  const uint32_t start_address);
  static void MaybeCloseModelFileStream(OnDeviceModelParams* params);
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_ON_DEVICE_HEAD_MODEL_H_
