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
//   Consider the fma-fusion rewrite pattern (fadd (fmul x,y) z) --> (fma x, y,
//   z). If the 'fadd' is actually an 'llvm.vp.fadd" and the 'fmul' is actually
//   an 'llvm.vp.fmul', we can perform the rewrite using the %mask and %evl of
//   the 'fadd' node.
//
//
// @llvm.experimental.constrained.fadd(double %a, double %b,
//                                     metadata <rounding mode> metadata,
//                                     <exception behavior>)
//
//   This is an fadd with a possibly non-default rounding mode and exception
//   behavior.
//   (https://llvm.org/docs/LangRef.html#constrained-floating-point-intrinsics).
//   In this case, the operation matches the semantics of a regular 'fadd'
//   exactly, if the rounding mode is 'round.tonearest' and the exception
//   behavior is 'fpexcept.ignore'.
//   Re-considering the case of fma fusion, this time with two constrained fp
//   intrinsics.  If the rounding mode is tonearest for both and neither of the
//   'llvm.experimental.contrained.fmul' has 'fpexcept.strict',, we are good to
//   apply the rewrite and emit a contrained fma with the exception flad of the
//   'fadd'.
//
// There is also a proposal to add complex arithmetic intrinsics to LLVM. In
// that case, the operation is semantically an 'fadd', if we consider the space
// of complex floating-point numbers and their operations.
//
//===----------------------------------------------------------------------===//

// Look for comments starting with "TODO(new trait)" to see what to implement to
// establish a new instruction trait.

#ifndef LLVM_IR_TRAIT_SEMANTICTRAIT_H
#define LLVM_IR_TRAIT_SEMANTICTRAIT_H

#include <llvm/IR/Instructions.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/Operator.h>
#include <llvm/IR/Value.h>

namespace llvm {

/// Type Casting {
/// These cast operators allow you to side-step the first-class type hierarchy
/// of LLVM (Value, Instruction, BinaryOperator, ..) into your custom type
/// hierarchy.
///
/// trait_cast<Trait, Instruction>(V)
///
/// actually casts \p V to ExtInstruction<Trait>.
template <typename Trait, typename ValueDerivedType> struct TraitCast {
  using ExtType = ValueDerivedType;
};

// This has to happen after all traits are defined since we are referring to
// members of template specialization for each Trait (The TraitCast::ExtType).
#define CASTING_TEMPLATE(CASTFUNC, PREFIXMOD, REFMOD)                          \
  template <typename Trait, typename ValueDerivedType>                         \
  static auto trait_##CASTFUNC(PREFIXMOD Value REFMOD V)                       \
      ->decltype(CASTFUNC<                                                     \
                 typename TraitCast<Trait, ValueDerivedType>::ExtType>(V)) {   \
    using TraitExtendedType =                                                  \
        typename TraitCast<Trait, ValueDerivedType>::ExtType;                  \
    return CASTFUNC<TraitExtendedType>(V);                                     \
  }

#define CONST_CAST_TEMPLATE(CASTFUNC, REFMOD)                                  \
  CASTING_TEMPLATE(CASTFUNC, const, REFMOD)                                    \
  CASTING_TEMPLATE(CASTFUNC, , REFMOD)

// 'dyn_cast' (allow [const] Value*)
CONST_CAST_TEMPLATE(dyn_cast, *)

// 'cast' (allow [const] Value(*|&))
CONST_CAST_TEMPLATE(cast, *)
CONST_CAST_TEMPLATE(cast, &)

// 'isa'
CONST_CAST_TEMPLATE(isa, *)
CONST_CAST_TEMPLATE(isa, &)
/// } Type Casting

// TODO
// The trait builder is a specialized IRBuilder that emits trait-compatible
// instructions.
template <typename Trait> struct TraitBuilder {};

// This is used in pattern matching to check that all instructions in the
// pattern are trait-compatible.
template <typename Trait> struct MatcherContext {
  // Check whether \p Val is compatible with this context and merge its
  // properties. \returns Whether \p Val is compatible with the current state of
  // the context.
  bool accept(const Value *Val) { return Val; }

  // Like accept() but does not modify the context.
  bool check(const Value *Val) const { return Val; }

  // Whether to allow constant folding with the currently accepted operators and
  // their operands.
  bool allowConstantFolding() const { return true; }
};

/// Empty Trait {
///
/// This defined the empty trait without properties. Type casting stays in the
/// standard llvm::Value type hierarchy.

// Trait without any difference to standard IR
struct EmptyTrait {
  // This is to block reassociation for traits that do not support it.
  static constexpr bool AllowReassociation = true;

  // Whether \p V should be considered at all with this trait.
  static bool consider(const Value *) { return true; }
};

using DefaultTrait = EmptyTrait;

/// } Empty Trait

} // end namespace llvm

#endif // LLVM_IR_TRAIT_SEMANTICTRAIT_H
