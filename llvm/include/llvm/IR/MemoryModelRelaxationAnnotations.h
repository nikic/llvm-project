//===- MemoryModelRelaxationAnnotations.h -----------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
/// \file
/// This file provides utility for Memory Model Relaxation Annotations (MMRAs).
/// Those annotations are represented using Metadata. The MMRATagSet class
/// offers a simple API to parse the metadata and perform common operations on
/// it. The MMRAMetadata class is a simple tuple of MDNode that provides easy
/// access to all MMRA annotations on an instruction.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_IR_MEMORYMODELRELAXATIONANNOTATIONS_H
#define LLVM_IR_MEMORYMODELRELAXATIONANNOTATIONS_H

#include <set>
#include <string>
#include <tuple>
#include <vector>

namespace llvm {

class MDNode;
class MDTuple;
class StringRef;
class raw_ostream;
class LLVMContext;
class Instruction;

/// Helper class for `!mmra` metadata nodes which can both build MMRA MDNodes,
/// and parse them.
///
/// This can be visualized as a set of "tags", with each tag
/// representing a particular property of an instruction, as
/// explained in the MemoryModelRelaxationAnnotations docs.
///
/// This class (and the optimizer in general) does not reason
/// about the exact nature of the tags and the properties they
/// imply. It just sees the metadata as a collection of tags, which
/// are a prefix/suffix pair of strings.
class MMRAMetadata {
public:
  using TagT = std::pair<std::string, std::string>;
  using SetT = std::set<TagT>;
  using const_iterator = SetT::const_iterator;

  MMRAMetadata() = default;
  MMRAMetadata(const Instruction &I);
  MMRAMetadata(MDNode *MD);

  /// \returns whether the MMRAs on \p A and \p B are compatible.
  static bool checkCompatibility(const Instruction &A, const Instruction &B) {
    return MMRAMetadata(A).isCompatibleWith(B);
  }

  /// \returns whether this set of tags is compatible with \p Other.
  bool isCompatibleWith(const MMRAMetadata &Other) const;

  /// Combines this set of tags with \p Other.
  /// \returns a new set of tags containing the result.
  MMRAMetadata combine(const MMRAMetadata &Other) const;

  MMRAMetadata &addTag(StringRef Prefix, StringRef Suffix);
  MMRAMetadata &addTag(const TagT &Tag) {
    Tags.insert(Tag);
    return *this;
  }

  bool hasTag(StringRef Prefix, StringRef Suffix) const;
  bool hasTag(const TagT &Tag) const { return Tags.count(Tag); }

  bool hasTagWithPrefix(StringRef Prefix) const;

  std::vector<TagT> getAllTagsWithPrefix(StringRef Prefix) const;

  MDTuple *getAsMD(LLVMContext &Ctx) const;

  const_iterator begin() const;
  const_iterator end() const;
  bool empty() const;
  unsigned size() const;

  void print(raw_ostream &OS) const;
  void dump() const;

  operator bool() const { return !Tags.empty(); }
  bool operator==(const MMRAMetadata &Other) const {
    return Tags == Other.Tags;
  }
  bool operator!=(const MMRAMetadata &Other) const {
    return Tags != Other.Tags;
  }

private:
  SetT Tags;
};

bool canInstructionHaveMMRAs(const Instruction &I);

} // namespace llvm

#endif
