//===- llvm/IR/Trait/SemanticTrait.h - Basic trait definitions --*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines semantic traits.
// Some intrinsics in LLVM can be described as being a regular operation (such
// as an fadd) at their core with an additional semantic trait. We do this to
// lift optimizations that are defined in terms of standard IR operations (eg
// fadd, fmul) to these intrinsics. We keep the existing patterns and rewrite
// machinery and transparently check the rewrite is consistent with the
// semantical trait(s) that are attached to the operations.
//
// For example:
//
// @llvm.vp.fadd(<256 x double> %a, <256 x double> %b,
//               <256 x i1> %m. i32 %avl)
//
//   This is a vector-predicated fadd instruction with a %mask and %evl
//   parameter
//   (https://llvm.org/docs/LangRef.html#vector-predication-intrinsics).
//   However, at its core it is just an 'fadd'.
//
//   Consider the simplification (add (sub x,y), y) --> x. If the 'add' is
//   actually an 'llvm.vp.add" and the 'sub' is really an 'llvm.vp.sub', we can
//   do the simplification. the 'fadd' node.
//
//
// @llvm.experimental.constrained.fadd(double %a, double %b,
//                                     metadata <rounding mode> metadata,
//                                     <exception behavior>)
//
//   This is an fadd with a possibly non-default rounding mode and exception
//   behavior.
//   (https://llvm.org/docs/LangRef.html#constrained-floating-point-intrinsics).
//   The constrained fp intrinsic has exactly the semantics of a regular 'fadd',
//   if the rounding mode is 'round.tonearest' and the exception behavior is
//   'fpexcept.ignore'.
//   We can use all simplifying rewrites for regular fp arithmetic also for
//   constrained fp arithmetic where this applies.
//
// There is also a proposal to add complex arithmetic intrinsics to LLVM. In
// that case, the operation is semantically an 'fadd', if we consider the space
// of complex floating-point numbers and their operations.
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/FloatingPointMode.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Operator.h"
#include "llvm/IR/Traits/SemanticTrait.h"

#ifndef LLVM_IR_TRAITS_TRAITS_H
#define LLVM_IR_TRAITS_TRAITS_H

namespace llvm {

/// Base classes {
// Shared functionality for extended instructions
// These define all functions used in PatterMatch as 'virtual' to remind you to
// implement them.

// Make sure no member of the Ext..<Trait> hierarchy can be constructed.
struct ExtBase {
  ExtBase() = delete;
  ~ExtBase() = delete;
  ExtBase &operator=(const ExtBase &) = delete;
  void *operator new(size_t s) = delete;
};

// Mirror generic functionality of llvm::Instruction.
struct ExtInstructionBase : public ExtBase, public User {
  BasicBlock *getParent() { return cast<Instruction>(this)->getParent(); }
  const BasicBlock *getParent() const {
    return cast<const Instruction>(this)->getParent();
  }

  void copyIRFlags(const Value *V, bool IncludeWrapFlags) {
    cast<Instruction>(this)->copyIRFlags(V, IncludeWrapFlags);
  }

  FastMathFlags getFastMathFlags() const {
    if (auto *FPM = dyn_cast<FPMathOperator>(this))
      return FPM->getFastMathFlags();
    return FastMathFlags();
  }
};

/// } Base classes

/// Intrinsic-Trait Template {
// Generic instantiation for traits that want to masquerade their intrinsic as a
// regular IR instruction.

// Pretend to be a llvm::Operator.
template <typename Trait> struct ExtOperator : public ExtBase, public User {
  unsigned getOpcode() const {
    // Use the intrinsic override.
    if (const auto *Intrin = dyn_cast<typename Trait::Intrinsic>(this))
      if (auto OpcOpt = Intrin->getFunctionalOpcode())
        return *OpcOpt;

    // Default Operator opcode.
    return cast<const Operator>(this)->getOpcode();
  }

  bool hasNoSignedWrap() const {
    if (const auto *OverflowingBOp = dyn_cast<OverflowingBinaryOperator>(this))
      return OverflowingBOp->hasNoSignedWrap();
    return false;
  }

  bool hasNoUnsignedWrap() const {
    if (const auto *OverflowingBOp = dyn_cast<OverflowingBinaryOperator>(this))
      return OverflowingBOp->hasNoUnsignedWrap();
    return false;
  }

  // Every operator is also an extended operator.
  static bool classof(const Value *V) { return isa<Operator>(V); }
};

// Pretend to be a llvm::Instruction.
template <typename Trait>
struct ExtInstruction final : public ExtInstructionBase {
  unsigned getOpcode() const {
    // Use the intrinsic override.
    if (const auto *Intrin = dyn_cast<typename Trait::Intrinsic>(this))
      if (auto OpcOpt = Intrin->getFunctionalOpcode())
        return *OpcOpt;
    // Default opcode.
    return cast<const Instruction>(this)->getOpcode();
  }

  static bool classof(const Value *V) { return isa<Instruction>(V); }
};

// Pretend to be a (different) llvm::IntrinsicInst.
template <typename Trait>
struct ExtIntrinsic final : public ExtInstructionBase {
  Intrinsic::ID getIntrinsicID() const {
    // Use the intrinsic override.
    if (const auto *TraitIntrin = dyn_cast<typename Trait::Intrinsic>(this))
      return TraitIntrin->getFunctionalIntrinsic();
    // Default intrinsic opcode.
    return cast<IntrinsicInst>(this)->getIntrinsicID();
  }

  unsigned getOpcode() const {
    // We are looking at this as an intrinsic -> do not hide this.
    return cast<Instruction>(this)->getOpcode();
  }

  bool isCommutative() const {
    // The underlying intrinsic may not specify whether it is commutative.
    // Query our own interface to be sure this is done right.
    // Use the intrinsic override.
    if (const auto *TraitIntrin = dyn_cast<typename Trait::Intrinsic>(this))
      return TraitIntrin->isFunctionalCommutative();
    return cast<IntrinsicInst>(this)->isFunctionalCommutative();
  }

  static bool classof(const Value *V) { return IntrinsicInst::classof(V); }
};

template <typename Trait, typename RegularCmpInst, typename PredicateType,
          unsigned OPC>
struct ExtCmpBase : public ExtInstructionBase {
  unsigned getOpcode() const { return OPC; }

  FCmpInst::Predicate getPredicate() const {
    // Use the intrinsic override.
    if (const auto *Intrin = dyn_cast<typename Trait::Intrinsic>(this)) {
      return Intrin->getPredicate();
    }

    // Default opcode.
    return cast<const RegularCmpInst>(this)->getPredicate();
  }
};

template <typename Trait, typename RegularCmpInst, unsigned OPC>
static bool classofExtCmpBase(const Value *V) {
  return isa<const RegularCmpInst>(V) ||
         cast<typename Trait::Intrinsic>->getFunctionalOpcode() == OPC;
}

// Pretend to be a llvm::FCmpInst.
template <typename Trait>
struct ExtFCmpInst final
    : public ExtCmpBase<Trait, FCmpInst, FCmpInst::Predicate,
                        Instruction::FCmp> {
  static bool classof(const Value *V) {
    return classofExtCmpBase<Trait, FCmpInst, Instruction::FCmp>(V);
  }
};

// Pretend to be a llvm::ICmpInst.
template <typename Trait>
struct ExtICmpInst final
    : public ExtCmpBase<Trait, ICmpInst, ICmpInst::Predicate,
                        Instruction::ICmp> {
  static bool classof(const Value *V) {
    return classofExtCmpBase<Trait, ICmpInst, Instruction::ICmp>(V);
  }
};

// Pretend to be a BinaryOperator.
template <typename Trait>
struct ExtBinaryOperator final : public ExtOperator<Trait> {
  using BinaryOps = Instruction::BinaryOps;

  static bool classof(const Instruction *I) {
    if (isa<BinaryOperator>(I))
      return true;
    const auto *Intrin = dyn_cast<typename Trait::Intrinsic>(I);
    return Intrin && Intrin->isFunctionalBinaryOp();
  }
  static bool classof(const ConstantExpr *CE) {
    return isa<BinaryOperator>(CE);
  }
  static bool classof(const Value *V) {
    if (const auto *I = dyn_cast<Instruction>(V))
      return classof(I);

    if (const auto *CE = dyn_cast<ConstantExpr>(V))
      return classof(CE);

    return false;
  }
};

// Pretend to be a UnaryOperator.
template <typename Trait>
struct ExtUnaryOperator final : public ExtOperator<Trait> {
  using BinaryOps = Instruction::BinaryOps;

  static bool classof(const Instruction *I) {
    if (isa<UnaryOperator>(I))
      return true;
    const auto *Intrin = dyn_cast<typename Trait::Intrinsic>(I);
    return Intrin && Intrin->isFunctionalUnaryOp();
  }
};

// TODO Implement other extended types.

/// Template-specialization for the Ext<Something> type hierarchy {
//// Enable the ExtSOMETHING<Trait> for your trait
#define INTRINSIC_TRAIT_SPECIALIZE(TRAIT, TYPE)                                \
  template <> struct TraitCast<TRAIT, TYPE> {                                  \
    using ExtType = Ext##TYPE<TRAIT>;                                          \
  };                                                                           \
  template <> struct TraitCast<TRAIT, const TYPE> {                            \
    using ExtType = const Ext##TYPE<TRAIT>;                                    \
  };

/// } Trait Template Classes

// Constraint fp trait.
struct CFPTrait {
  using Intrinsic = ConstrainedFPIntrinsic;
  static constexpr bool AllowReassociation = false;

  // Whether \p V should be considered at all with this trait.
  // It is not possible to mix constrained and unconstrained ops.
  // Only apply this trait with the constrained variant.
  static bool consider(const Value *V) {
    return isa<ConstrainedFPIntrinsic>(V);
  }
};
INTRINSIC_TRAIT_SPECIALIZE(CFPTrait, Instruction)
INTRINSIC_TRAIT_SPECIALIZE(CFPTrait, Operator)
INTRINSIC_TRAIT_SPECIALIZE(CFPTrait, BinaryOperator)
INTRINSIC_TRAIT_SPECIALIZE(CFPTrait, UnaryOperator)
// Deflect queries for the Predicate to the ConstrainedFPCmpIntrinsic.
template <> struct ExtFCmpInst<CFPTrait> : public ExtInstructionBase {
  unsigned getOpcode() const { return Instruction::FCmp; }

  FCmpInst::Predicate getPredicate() const {
    return cast<ConstrainedFPCmpIntrinsic>(this)->getPredicate();
  }

  static bool classof(const Value *V) {
    return isa<ConstrainedFPCmpIntrinsic>(V);
  }
};
INTRINSIC_TRAIT_SPECIALIZE(CFPTrait, FCmpInst)

// Accept all constrained fp intrinsics that are actually not constrained.
template <> struct MatcherContext<CFPTrait> {
  bool accept(const Value *Val) { return check(Val); }
  bool check(const Value *Val) const {
    if (!Val)
      return false;
    const auto *CFP = dyn_cast<ConstrainedFPIntrinsic>(Val);
    if (!CFP)
      return true;
    auto RoundingOpt =
        CFP->getRoundingMode().getValueOr(RoundingMode::NearestTiesToEven);
    auto ExceptOpt =
        CFP->getExceptionBehavior().getValueOr(fp::ExceptionBehavior::ebIgnore);
    return (ExceptOpt == fp::ExceptionBehavior::ebIgnore) &&
           (RoundingOpt == RoundingMode::NearestTiesToEven);
  }
};

// Vector-predicated trait.
struct VPTrait {
  using Intrinsic = VPIntrinsic;
  // TODO Enable re-association.
  static constexpr bool AllowReassociation = false;
  // VP intrinsic mix with regular IR instructions.
  // TODO: Adapt this to work with other than arithmetic VP ops.
  static bool consider(const Value *V) {
    return V->getType()->isVectorTy() &&
           V->getType()->getScalarType()->isIntegerTy();
  }
};
INTRINSIC_TRAIT_SPECIALIZE(VPTrait, Instruction)
INTRINSIC_TRAIT_SPECIALIZE(VPTrait, Operator)
INTRINSIC_TRAIT_SPECIALIZE(VPTrait, BinaryOperator)
INTRINSIC_TRAIT_SPECIALIZE(VPTrait, UnaryOperator)

// Accept everything that passes as a VPIntrinsic.
template <> struct MatcherContext<VPTrait> {
  // TODO: pick up %mask and %evl here and use them to generate code again. We
  // only remove instructions for the moment.
  bool accept(const Value *Val) { return Val; }
  bool check(const Value *Val) const { return Val; }
};

} // namespace llvm

#undef INTRINSIC_TRAIT_SPECIALIZE

#endif // LLVM_IR_TRAITS_TRAITS_H
