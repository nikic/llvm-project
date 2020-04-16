//===- ConstantRange.cpp - ConstantRange implementation -------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Represent a range of possible values that may occur when the program is run
// for an integral value.  This keeps track of a lower and upper bound for the
// constant, which MAY wrap around the end of the numeric range.  To do this, it
// keeps track of a [lower, upper) bound, which specifies an interval just like
// STL iterators.  When used with boolean values, the following are important
// ranges (other integral ranges use min/max values for special range values):
//
//  [F, F) = {}     = Empty set
//  [T, F) = {T}
//  [F, T) = {F}
//  [T, T) = {F, T} = Full set
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/APInt.h"
#include "llvm/Config/llvm-config.h"
#include "llvm/IR/ConstantRange.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Operator.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/KnownBits.h"
#include "llvm/Support/raw_ostream.h"
#include <algorithm>
#include <cassert>
#include <cstdint>

using namespace llvm;

static APFloat ZeroNext(APFloat X, bool Neg) {
  if (X.isZero() && X.isNegative() != Neg)
    X.changeSign();
  else
    X.next(Neg);
  return X;
}

ConstantRange::ConstantRange(uint32_t BitWidth, bool Full)
    : Lower(Full ? APInt::getMaxValue(BitWidth) : APInt::getMinValue(BitWidth)),
      Upper(Lower) {}

ConstantRange::ConstantRange(APInt V)
    : Lower(std::move(V)), Upper(Lower + 1) {}

ConstantRange::ConstantRange(APInt L, APInt U)
    : Lower(std::move(L)), Upper(std::move(U)) {
  assert(Lower.getBitWidth() == Upper.getBitWidth() &&
         "ConstantRange with unequal bit widths");
  assert((Lower != Upper || (Lower.isMaxValue() || Lower.isMinValue())) &&
         "Lower == Upper, but they aren't min or max value!");
}

ConstantRange::ConstantRange(const APFloat &Const)
    : LowerFP(Const), UpperFP(Const), isFloat(true), canBeNaN(Const.isNaN()) {}

ConstantRange::ConstantRange(APFloat Lower, APFloat Upper, bool canBeNaN)
    : LowerFP(std::move(Lower)), UpperFP(std::move(Upper)), isFloat(true),
      canBeNaN(canBeNaN) {
  assert(&Lower.getSemantics() == &Upper.getSemantics() &&
         "ConstantRange with mismatched FP semantics");
  assert(LowerFP.isNaN() == UpperFP.isNaN());
  // Check if we are wrapped range with no values outside
  APFloat Tmp = ZeroNext(LowerFP, true);
  if (!LowerFP.bitwiseIsEqual(UpperFP) && Tmp.bitwiseIsEqual(UpperFP) && !Tmp.isNaN()) {
    LowerFP = APFloat::getInf(LowerFP.getSemantics(), true); // NegInf
    UpperFP = APFloat::getInf(LowerFP.getSemantics(), false); // PosInf
  }
}

ConstantRange ConstantRange::fromKnownBits(const KnownBits &Known,
                                           bool IsSigned) {
  assert(!Known.hasConflict() && "Expected valid KnownBits");

  if (Known.isUnknown())
    return getFull(Known.getBitWidth());

  // For unsigned ranges, or signed ranges with known sign bit, create a simple
  // range between the smallest and largest possible value.
  if (!IsSigned || Known.isNegative() || Known.isNonNegative())
    return ConstantRange(Known.getMinValue(), Known.getMaxValue() + 1);

  // If we don't know the sign bit, pick the lower bound as a negative number
  // and the upper bound as a non-negative one.
  APInt Lower = Known.getMinValue(), Upper = Known.getMaxValue();
  Lower.setSignBit();
  Upper.clearSignBit();
  return ConstantRange(Lower, Upper + 1);
}

ConstantRange ConstantRange::makeAllowedFCmpRegion(CmpInst::Predicate Pred,
                                                   const ConstantRange &CR) {
  assert(CR.isFloat);
  if (CR.isEmptySet())
    return CR;

  // Nothing is ordered wrt NaN
  if (CR.LowerFP.isNaN() && CmpInst::isOrdered(Pred))
    return CR.getEmpty();
  // Everything is unordered wrt NaN
  if (CR.canBeNaN && CmpInst::isUnordered(Pred))
      return CR.getFull();

  // Useful constants
  APFloat PosInf = APFloat::getInf(CR.LowerFP.getSemantics(), false);
  APFloat NegInf = APFloat::getInf(CR.LowerFP.getSemantics(), true);
  APFloat NaN = APFloat::getNaN(CR.LowerFP.getSemantics());

  switch (Pred) {
  default:
    llvm_unreachable("Invalid FCmp predicate to makeAllowedFCmpRegion()");
  case CmpInst::FCMP_UNO:
    // CR.canBeNaN is handled above, so only NaN compares UNO to CR.
    return ConstantRange(NaN);
  case CmpInst::FCMP_ORD:
    // isnan(CR) is handled above, return no-nan range
    return ConstantRange(std::move(NegInf), std::move(PosInf), false);
  case CmpInst::FCMP_UEQ:
  case CmpInst::FCMP_OEQ: {
    // Return the sames ordered part as CR, extend boundaries if zero.
    APFloat LowerFP = CR.LowerFP.isZero() ? APFloat::getZero(CR.LowerFP.getSemantics(), true) : CR.LowerFP;
    APFloat UpperFP = CR.UpperFP.isZero() ? APFloat::getZero(CR.UpperFP.getSemantics(), false) : CR.UpperFP;
    return ConstantRange(std::move(LowerFP), std::move(UpperFP), CmpInst::isUnordered(Pred));
  }
  case CmpInst::FCMP_UNE:
  case CmpInst::FCMP_ONE:
    // bitwiseIsEqual covers singleElement + canBeNaN
    // [-0,0] should be trated as single value wrt != operator
    if (CR.LowerFP.bitwiseIsEqual(CR.UpperFP) ||
        (CR.LowerFP.isNegZero() && CR.UpperFP.isPosZero())) {
      ConstantRange Inv = CR.inverse();
      Inv.canBeNaN = CmpInst::isUnordered(Pred);
      // Handle +/- 0
      if (Inv.LowerFP.isPosZero()) // The original included -0.0
        Inv.LowerFP.next(false);
      if (Inv.UpperFP.isNegZero()) // The original included 0.0
        Inv.UpperFP.next(true);
      return Inv;
    }
    return ConstantRange(std::move(NegInf), std::move(PosInf), CmpInst::isUnordered(Pred));
  case CmpInst::FCMP_OLT:
  case CmpInst::FCMP_ULT: {
    // Nothing is LT -Inf, but NaN is unordered
    if (CR.UpperFP.bitwiseIsEqual(CR.LowerFP) &&
        CR.UpperFP.isNegative() && CR.UpperFP.isInfinity())
      return ConstantRange(NaN, NaN, CmpInst::isUnordered(Pred));
    // Almost everything is LT +Inf
    APFloat Upper = CR.contains(PosInf) ? PosInf : CR.UpperFP;
    Upper.next(true);
    return ConstantRange(std::move(NegInf), std::move(Upper), CmpInst::isUnordered(Pred));
  }
  case CmpInst::FCMP_OGT:
  case CmpInst::FCMP_UGT: {
    // Nothing is GT +Inf, but NaN unordered
    if (CR.UpperFP.bitwiseIsEqual(CR.LowerFP) &&
        !CR.UpperFP.isNegative() && CR.UpperFP.isInfinity())
      return ConstantRange(NaN, NaN, CmpInst::isUnordered(Pred));
    // Almost everything is LT -Inf
    APFloat Lower = CR.contains(NegInf) ? NegInf : CR.LowerFP;
    Lower.next(false);
    return ConstantRange(std::move(Lower), std::move(PosInf), CmpInst::isUnordered(Pred));
  }
  case CmpInst::FCMP_OLE:
  case CmpInst::FCMP_ULE: {
    // Everything is LE than +Inf, and NaN is unordered
    if (CR.contains(PosInf))
      return ConstantRange(std::move(NegInf), std::move(PosInf), CmpInst::isUnordered(Pred));
    APFloat Upper = CR.UpperFP.isZero() ? APFloat::getZero(CR.UpperFP.getSemantics()) : CR.UpperFP;
    return ConstantRange(std::move(NegInf), std::move(Upper), CmpInst::isUnordered(Pred));
  }
  case CmpInst::FCMP_OGE:
  case CmpInst::FCMP_UGE: {
    // Everything is GE than -Inf, and NaN is unordered
    if (CR.contains(NegInf))
      return ConstantRange(std::move(NegInf), std::move(PosInf), CmpInst::isUnordered(Pred));
    APFloat Lower = CR.LowerFP.isZero() ? APFloat::getZero(CR.LowerFP.getSemantics(), true) : CR.LowerFP;
    return ConstantRange(std::move(Lower), std::move(PosInf), CmpInst::isUnordered(Pred));
  }
  }
}

ConstantRange ConstantRange::makeSatisfyingFCmpRegion(CmpInst::Predicate Pred,
                                                      const ConstantRange &CR) {
  // Follows from De-Morgan's laws:
  //
  // ~(~A union ~B) == A intersect B.
  //
  return makeAllowedFCmpRegion(CmpInst::getInversePredicate(Pred), CR)
      .inverse();
}

ConstantRange ConstantRange::makeExactFCmpRegion(CmpInst::Predicate Pred,
                                                 const APFloat &C) {
  // Computes the exact range that is equal to both the constant ranges returned
  // by makeAllowedFCmpRegion and makeSatisfyingICmpRegion. This is always true
  // when RHS is a singleton such as an APFloat and so the assert is valid.
  //
  assert(makeAllowedFCmpRegion(Pred, C) == makeSatisfyingFCmpRegion(Pred, C));
  return makeAllowedFCmpRegion(Pred, C);
}

ConstantRange ConstantRange::makeAllowedICmpRegion(CmpInst::Predicate Pred,
                                                   const ConstantRange &CR) {
  if (CR.isEmptySet())
    return CR;

  uint32_t W = CR.getBitWidth();
  switch (Pred) {
  default:
    llvm_unreachable("Invalid ICmp predicate to makeAllowedICmpRegion()");
  case CmpInst::ICMP_EQ:
    return CR;
  case CmpInst::ICMP_NE:
    if (CR.isSingleElement())
      return ConstantRange(CR.getUpper(), CR.getLower());
    return getFull(W);
  case CmpInst::ICMP_ULT: {
    APInt UMax(CR.getUnsignedMax());
    if (UMax.isMinValue())
      return getEmpty(W);
    return ConstantRange(APInt::getMinValue(W), std::move(UMax));
  }
  case CmpInst::ICMP_SLT: {
    APInt SMax(CR.getSignedMax());
    if (SMax.isMinSignedValue())
      return getEmpty(W);
    return ConstantRange(APInt::getSignedMinValue(W), std::move(SMax));
  }
  case CmpInst::ICMP_ULE:
    return getNonEmpty(APInt::getMinValue(W), CR.getUnsignedMax() + 1);
  case CmpInst::ICMP_SLE:
    return getNonEmpty(APInt::getSignedMinValue(W), CR.getSignedMax() + 1);
  case CmpInst::ICMP_UGT: {
    APInt UMin(CR.getUnsignedMin());
    if (UMin.isMaxValue())
      return getEmpty(W);
    return ConstantRange(std::move(UMin) + 1, APInt::getNullValue(W));
  }
  case CmpInst::ICMP_SGT: {
    APInt SMin(CR.getSignedMin());
    if (SMin.isMaxSignedValue())
      return getEmpty(W);
    return ConstantRange(std::move(SMin) + 1, APInt::getSignedMinValue(W));
  }
  case CmpInst::ICMP_UGE:
    return getNonEmpty(CR.getUnsignedMin(), APInt::getNullValue(W));
  case CmpInst::ICMP_SGE:
    return getNonEmpty(CR.getSignedMin(), APInt::getSignedMinValue(W));
  }
}

ConstantRange ConstantRange::makeSatisfyingICmpRegion(CmpInst::Predicate Pred,
                                                      const ConstantRange &CR) {
  // Follows from De-Morgan's laws:
  //
  // ~(~A union ~B) == A intersect B.
  //
  return makeAllowedICmpRegion(CmpInst::getInversePredicate(Pred), CR)
      .inverse();
}

ConstantRange ConstantRange::makeExactICmpRegion(CmpInst::Predicate Pred,
                                                 const APInt &C) {
  // Computes the exact range that is equal to both the constant ranges returned
  // by makeAllowedICmpRegion and makeSatisfyingICmpRegion. This is always true
  // when RHS is a singleton such as an APInt and so the assert is valid.
  // However for non-singleton RHS, for example ult [2,5) makeAllowedICmpRegion
  // returns [0,4) but makeSatisfyICmpRegion returns [0,2).
  //
  assert(makeAllowedICmpRegion(Pred, C) == makeSatisfyingICmpRegion(Pred, C));
  return makeAllowedICmpRegion(Pred, C);
}

bool ConstantRange::getEquivalentICmp(CmpInst::Predicate &Pred,
                                      APInt &RHS) const {
  bool Success = false;
  assert(!isFloat);

  if (isFullSet() || isEmptySet()) {
    Pred = isEmptySet() ? CmpInst::ICMP_ULT : CmpInst::ICMP_UGE;
    RHS = APInt(getBitWidth(), 0);
    Success = true;
  } else if (auto *OnlyElt = getSingleElement()) {
    Pred = CmpInst::ICMP_EQ;
    RHS = *OnlyElt;
    Success = true;
  } else if (auto *OnlyMissingElt = getSingleMissingElement()) {
    Pred = CmpInst::ICMP_NE;
    RHS = *OnlyMissingElt;
    Success = true;
  } else if (getLower().isMinSignedValue() || getLower().isMinValue()) {
    Pred =
        getLower().isMinSignedValue() ? CmpInst::ICMP_SLT : CmpInst::ICMP_ULT;
    RHS = getUpper();
    Success = true;
  } else if (getUpper().isMinSignedValue() || getUpper().isMinValue()) {
    Pred =
        getUpper().isMinSignedValue() ? CmpInst::ICMP_SGE : CmpInst::ICMP_UGE;
    RHS = getLower();
    Success = true;
  }

  assert((!Success || ConstantRange::makeExactICmpRegion(Pred, RHS) == *this) &&
         "Bad result!");

  return Success;
}

/// Exact mul nuw region for single element RHS.
static ConstantRange makeExactMulNUWRegion(const APInt &V) {
  unsigned BitWidth = V.getBitWidth();
  if (V == 0)
    return ConstantRange::getFull(V.getBitWidth());

  return ConstantRange::getNonEmpty(
      APIntOps::RoundingUDiv(APInt::getMinValue(BitWidth), V,
                             APInt::Rounding::UP),
      APIntOps::RoundingUDiv(APInt::getMaxValue(BitWidth), V,
                             APInt::Rounding::DOWN) + 1);
}

/// Exact mul nsw region for single element RHS.
static ConstantRange makeExactMulNSWRegion(const APInt &V) {
  // Handle special case for 0, -1 and 1. See the last for reason why we
  // specialize -1 and 1.
  unsigned BitWidth = V.getBitWidth();
  if (V == 0 || V.isOneValue())
    return ConstantRange::getFull(BitWidth);

  APInt MinValue = APInt::getSignedMinValue(BitWidth);
  APInt MaxValue = APInt::getSignedMaxValue(BitWidth);
  // e.g. Returning [-127, 127], represented as [-127, -128).
  if (V.isAllOnesValue())
    return ConstantRange(-MaxValue, MinValue);

  APInt Lower, Upper;
  if (V.isNegative()) {
    Lower = APIntOps::RoundingSDiv(MaxValue, V, APInt::Rounding::UP);
    Upper = APIntOps::RoundingSDiv(MinValue, V, APInt::Rounding::DOWN);
  } else {
    Lower = APIntOps::RoundingSDiv(MinValue, V, APInt::Rounding::UP);
    Upper = APIntOps::RoundingSDiv(MaxValue, V, APInt::Rounding::DOWN);
  }
  // ConstantRange ctor take a half inclusive interval [Lower, Upper + 1).
  // Upper + 1 is guaranteed not to overflow, because |divisor| > 1. 0, -1,
  // and 1 are already handled as special cases.
  return ConstantRange(Lower, Upper + 1);
}

ConstantRange
ConstantRange::makeGuaranteedNoWrapRegion(Instruction::BinaryOps BinOp,
                                          const ConstantRange &Other,
                                          unsigned NoWrapKind) {
  using OBO = OverflowingBinaryOperator;

  assert(Instruction::isBinaryOp(BinOp) && "Binary operators only!");

  assert((NoWrapKind == OBO::NoSignedWrap ||
          NoWrapKind == OBO::NoUnsignedWrap) &&
         "NoWrapKind invalid!");

  bool Unsigned = NoWrapKind == OBO::NoUnsignedWrap;
  unsigned BitWidth = Other.getBitWidth();

  switch (BinOp) {
  default:
    llvm_unreachable("Unsupported binary op");

  case Instruction::Add: {
    if (Unsigned)
      return getNonEmpty(APInt::getNullValue(BitWidth),
                         -Other.getUnsignedMax());

    APInt SignedMinVal = APInt::getSignedMinValue(BitWidth);
    APInt SMin = Other.getSignedMin(), SMax = Other.getSignedMax();
    return getNonEmpty(
        SMin.isNegative() ? SignedMinVal - SMin : SignedMinVal,
        SMax.isStrictlyPositive() ? SignedMinVal - SMax : SignedMinVal);
  }

  case Instruction::Sub: {
    if (Unsigned)
      return getNonEmpty(Other.getUnsignedMax(), APInt::getMinValue(BitWidth));

    APInt SignedMinVal = APInt::getSignedMinValue(BitWidth);
    APInt SMin = Other.getSignedMin(), SMax = Other.getSignedMax();
    return getNonEmpty(
        SMax.isStrictlyPositive() ? SignedMinVal + SMax : SignedMinVal,
        SMin.isNegative() ? SignedMinVal + SMin : SignedMinVal);
  }

  case Instruction::Mul:
    if (Unsigned)
      return makeExactMulNUWRegion(Other.getUnsignedMax());

    return makeExactMulNSWRegion(Other.getSignedMin())
        .intersectWith(makeExactMulNSWRegion(Other.getSignedMax()));

  case Instruction::Shl: {
    // For given range of shift amounts, if we ignore all illegal shift amounts
    // (that always produce poison), what shift amount range is left?
    ConstantRange ShAmt = Other.intersectWith(
        ConstantRange(APInt(BitWidth, 0), APInt(BitWidth, (BitWidth - 1) + 1)));
    if (ShAmt.isEmptySet()) {
      // If the entire range of shift amounts is already poison-producing,
      // then we can freely add more poison-producing flags ontop of that.
      return getFull(BitWidth);
    }
    // There are some legal shift amounts, we can compute conservatively-correct
    // range of no-wrap inputs. Note that by now we have clamped the ShAmtUMax
    // to be at most bitwidth-1, which results in most conservative range.
    APInt ShAmtUMax = ShAmt.getUnsignedMax();
    if (Unsigned)
      return getNonEmpty(APInt::getNullValue(BitWidth),
                         APInt::getMaxValue(BitWidth).lshr(ShAmtUMax) + 1);
    return getNonEmpty(APInt::getSignedMinValue(BitWidth).ashr(ShAmtUMax),
                       APInt::getSignedMaxValue(BitWidth).ashr(ShAmtUMax) + 1);
  }
  }
}

ConstantRange ConstantRange::makeExactNoWrapRegion(Instruction::BinaryOps BinOp,
                                                   const APInt &Other,
                                                   unsigned NoWrapKind) {
  // makeGuaranteedNoWrapRegion() is exact for single-element ranges, as
  // "for all" and "for any" coincide in this case.
  return makeGuaranteedNoWrapRegion(BinOp, ConstantRange(Other), NoWrapKind);
}

bool ConstantRange::isFullSet() const {
  if (isFloat)
    return LowerFP.isInfinity() && LowerFP.isNegative() &&
	   UpperFP.isInfinity() && !UpperFP.isNegative() && canBeNaN;
  else
    return Lower == Upper && Lower.isMaxValue();
}

bool ConstantRange::isEmptySet() const {
  if (isFloat)
    return LowerFP.isNaN() && UpperFP.isNaN() && !canBeNaN;
  return Lower == Upper && Lower.isMinValue();
}

bool ConstantRange::isWrappedSet() const {
  if (isFloat) // Float version is the same as isUpperWrapped
    return isUpperWrapped();
  return Lower.ugt(Upper) && !Upper.isNullValue();
}

bool ConstantRange::isUpperWrapped() const {
  if (isFloat)
    return LowerFP.compare(UpperFP) == APFloat::cmpGreaterThan;
  return Lower.ugt(Upper);
}

bool ConstantRange::isSignWrappedSet() const {
  assert(!isFloat);
  return Lower.sgt(Upper) && !Upper.isMinSignedValue();
}

bool ConstantRange::isUpperSignWrapped() const {
  assert(!isFloat);
  return Lower.sgt(Upper);
}

bool
ConstantRange::isSizeStrictlySmallerThan(const ConstantRange &Other) const {
  assert(getBitWidth() == Other.getBitWidth());
  assert(!isFloat);
  if (isFullSet())
    return false;
  if (Other.isFullSet())
    return true;
  return (Upper - Lower).ult(Other.Upper - Other.Lower);
}

bool
ConstantRange::isSizeLargerThan(uint64_t MaxSize) const {
  assert(MaxSize && "MaxSize can't be 0.");
  assert(!isFloat);
  // If this a full set, we need special handling to avoid needing an extra bit
  // to represent the size.
  if (isFullSet())
    return APInt::getMaxValue(getBitWidth()).ugt(MaxSize - 1);

  return (Upper - Lower).ugt(MaxSize);
}

bool ConstantRange::isAllNegative() const {
  // Empty set is all negative, full set is not.
  if (isEmptySet())
    return true;
  if (isFullSet())
    return false;

  assert(!isFloat);
  return !isUpperSignWrapped() && !Upper.isStrictlyPositive();
}

bool ConstantRange::isAllNonNegative() const {
  // Empty and full set are automatically treated correctly.
  return !isSignWrappedSet() && Lower.isNonNegative();
}

APInt ConstantRange::getUnsignedMax() const {
  assert(!isFloat);
  if (isFullSet() || isUpperWrapped())
    return APInt::getMaxValue(getBitWidth());
  return getUpper() - 1;
}

APInt ConstantRange::getUnsignedMin() const {
  assert(!isFloat);
  if (isFullSet() || isWrappedSet())
    return APInt::getMinValue(getBitWidth());
  return getLower();
}

APInt ConstantRange::getSignedMax() const {
  assert(!isFloat);
  if (isFullSet() || isUpperSignWrapped())
    return APInt::getSignedMaxValue(getBitWidth());
  return getUpper() - 1;
}

APInt ConstantRange::getSignedMin() const {
  assert(!isFloat);
  if (isFullSet() || isSignWrappedSet())
    return APInt::getSignedMinValue(getBitWidth());
  return getLower();
}

bool ConstantRange::contains(const APInt &V) const {
  assert(!isFloat);
  if (Lower == Upper)
    return isFullSet();

  if (!isUpperWrapped())
    return Lower.ule(V) && V.ult(Upper);
  return Lower.ule(V) || V.ult(Upper);
}

bool ConstantRange::contains(const APFloat &V) const {
  assert(isFloat);
  if (V.isNaN())
    return canBeNaN;

  if (V.bitwiseIsEqual(LowerFP) || V.bitwiseIsEqual(UpperFP))
    return true;

  // Special handling for signed zeros
  if (V.isPosZero() && LowerFP.isNegZero() && !UpperFP.isNegZero())
    return true;
  if (V.isNegZero() && UpperFP.isPosZero() && !LowerFP.isPosZero())
    return true;

  if (!isUpperWrapped())
    return LowerFP < V && V < UpperFP;
  return UpperFP > V || V > LowerFP;
}

bool ConstantRange::contains(const ConstantRange &Other) const {
  assert(isFloat == Other.isFloat);
  if (isFullSet() || Other.isEmptySet()) return true;
  if (isEmptySet() || Other.isFullSet()) return false;
  if (isFloat && canBeNaN && Other.UpperFP.isNaN()) return true;

  if (!isUpperWrapped()) {
    if (Other.isUpperWrapped())
      return false;

    if (isFloat) {
      auto Lo = LowerFP.compare(Other.LowerFP);
      auto Hi = UpperFP.compare(Other.UpperFP);
      return (Lo == APFloat::cmpLessThan ||
              LowerFP.bitwiseIsEqual(Other.LowerFP) ||
              (LowerFP.isNegZero() && Other.LowerFP.isPosZero())) &&
             (Hi == APFloat::cmpGreaterThan ||
              UpperFP.bitwiseIsEqual(Other.UpperFP) ||
             (UpperFP.isPosZero() && Other.UpperFP.isNegZero())) &&
	     (canBeNaN || !Other.canBeNaN);
    }
    return Lower.ule(Other.getLower()) && Other.getUpper().ule(Upper);
  }

  if (!Other.isUpperWrapped()) {
    if (isFloat) {
      // LHS is UpperWrapped, RHS is not. We can split into two subregions.
      ConstantRange UpperHalf = ConstantRange(LowerFP,
                                  APFloat::getInf(LowerFP.getSemantics()),
				  canBeNaN);
      ConstantRange LowerHalf = ConstantRange(
                                  APFloat::getInf(LowerFP.getSemantics(), true),
                                  UpperFP, canBeNaN);
      return LowerHalf.contains(Other) || UpperHalf.contains(Other);
    }
    return Other.getUpper().ule(Upper) || Lower.ule(Other.getLower());
  }

  assert(!isFloat);
  return Other.getUpper().ule(Upper) && Lower.ule(Other.getLower());
}

ConstantRange ConstantRange::subtract(const APInt &Val) const {
  assert(Val.getBitWidth() == getBitWidth() && "Wrong bit width");
  // If the set is empty or full, don't modify the endpoints.
  if (Lower == Upper)
    return *this;
  return ConstantRange(Lower - Val, Upper - Val);
}

ConstantRange ConstantRange::difference(const ConstantRange &CR) const {
  return intersectWith(CR.inverse());
}

static ConstantRange getPreferredRange(
    const ConstantRange &CR1, const ConstantRange &CR2,
    ConstantRange::PreferredRangeType Type) {
  if (Type == ConstantRange::Unsigned) {
    if (!CR1.isWrappedSet() && CR2.isWrappedSet())
      return CR1;
    if (CR1.isWrappedSet() && !CR2.isWrappedSet())
      return CR2;
  } else if (Type == ConstantRange::Signed) {
    if (!CR1.isSignWrappedSet() && CR2.isSignWrappedSet())
      return CR1;
    if (CR1.isSignWrappedSet() && !CR2.isSignWrappedSet())
      return CR2;
  }

  if (CR1.isSizeStrictlySmallerThan(CR2))
    return CR1;
  return CR2;
}

ConstantRange ConstantRange::intersectWith(const ConstantRange &CR,
                                           PreferredRangeType Type) const {
  assert(isFloat == CR.isFloat &&
         "ConstantRange type don't agree!");
  assert(getBitWidth() == CR.getBitWidth() &&
         "ConstantRange types don't agree!");

  // Handle common cases.
  if (   isEmptySet() || CR.isFullSet()) return *this;
  if (CR.isEmptySet() ||    isFullSet()) return CR;

  if (!isUpperWrapped() && CR.isUpperWrapped())
    return CR.intersectWith(*this, Type);

  if (!isUpperWrapped() && !CR.isUpperWrapped()) {
    if (isFloat) {
      assert(Type == Smallest);
      // There are several situations handled in this block,
      // none of which can result in a wrapped or disjoint result:
      // this: L--U      | L--U   |  L--U  |  L---U |   L--U |      L--U
      // CR:        L--U |   L--U | L----U |   L-U  | L--U   | L--U
      APFloat Upper = llvm::minimum(UpperFP, CR.UpperFP);
      APFloat Lower = llvm::maximum(LowerFP, CR.LowerFP);
      auto Res = Lower.compare(Upper);
      // Explicitly allow [-0, 0]
      if (Res != APFloat::cmpLessThan && !Lower.bitwiseIsEqual(Upper) &&
	  !(Lower.isNegZero() && Upper.isPosZero()))
        return ConstantRange(APFloat::getNaN(Lower.getSemantics()),
                             APFloat::getNaN(Upper.getSemantics()),
                             canBeNaN && CR.canBeNaN);
      return ConstantRange(::std::move(Lower), ::std::move(Upper),
                           canBeNaN && CR.canBeNaN);
    }
    if (Lower.ult(CR.Lower)) {
      // L---U       : this
      //       L---U : CR
      if (Upper.ule(CR.Lower))
        return getEmpty();

      // L---U       : this
      //   L---U     : CR
      if (Upper.ult(CR.Upper))
        return ConstantRange(CR.Lower, Upper);

      // L-------U   : this
      //   L---U     : CR
      return CR;
    }
    //   L---U     : this
    // L-------U   : CR
    if (Upper.ult(CR.Upper))
      return *this;

    //   L-----U   : this
    // L-----U     : CR
    if (Lower.ult(CR.Upper))
      return ConstantRange(Lower, CR.Upper);

    //       L---U : this
    // L---U       : CR
    return getEmpty();
  }

  if (isUpperWrapped() && !CR.isUpperWrapped()) {
    // FP range is inclusive so include it here
    if ((!isFloat && CR.Lower.ult(Upper)) ||
        (isFloat && CR.LowerFP <= UpperFP)) {
      // ------U   L--- : this
      //  L--U          : CR
      if (!isFloat && CR.Upper.ult(Upper))
        return CR;
      if (isFloat && CR.UpperFP < UpperFP)
        return ConstantRange(CR.LowerFP, CR.UpperFP, canBeNaN && CR.canBeNaN);

      // ------U   L--- : this
      //  L------U      : CR
      if (!isFloat && CR.Upper.ule(Lower))
        return ConstantRange(CR.Lower, Upper);
      // FP range is inclusive so don't include it here
      if (isFloat && CR.UpperFP < LowerFP)
        return ConstantRange(CR.LowerFP, UpperFP, canBeNaN && CR.canBeNaN);

      // ------U   L--- : this
      //  L----------U  : CR
      if (isFloat)
        return ConstantRange(CR.LowerFP, CR.UpperFP, canBeNaN && CR.canBeNaN);
      return getPreferredRange(*this, CR, Type);
    }
    if ((!isFloat && CR.Lower.ult(Lower)) ||
        (isFloat && CR.LowerFP < LowerFP)) {
      // --U      L---- : this
      //     L--U       : CR
      if (!isFloat && CR.Upper.ule(Lower))
        return getEmpty();
      if (isFloat && CR.UpperFP < LowerFP)
        return ConstantRange(APFloat::getNaN(LowerFP.getSemantics()),
                             APFloat::getNaN(UpperFP.getSemantics()),
                             canBeNaN && CR.canBeNaN);

      // --U      L---- : this
      //     L------U   : CR
      if (isFloat)
        return ConstantRange(LowerFP, CR.UpperFP, canBeNaN && CR.canBeNaN);
      return ConstantRange(Lower, CR.Upper);
    }

    // --U  L------ : this
    //        L--U  : CR
    if (isFloat)
      return ConstantRange(CR.LowerFP, CR.UpperFP, canBeNaN && CR.canBeNaN);
    return CR;
  }

  // Both are upperWrapped
  if (isFloat) {
    assert(Type == Smallest);
    // Handle disjoint cases
    if (CR.LowerFP <= UpperFP || LowerFP <= CR.UpperFP)
      return (CR.LowerFP - CR.UpperFP) > (LowerFP - UpperFP) ? CR : *this;
    APFloat Upper = llvm::minimum(UpperFP, CR.UpperFP);
    APFloat Lower = llvm::maximum(LowerFP, CR.LowerFP);
    return ConstantRange(::std::move(Lower), ::std::move(Upper), canBeNaN && CR.canBeNaN);
  }
  if (CR.Upper.ult(Upper)) {
    // ------U L-- : this
    // --U L------ : CR
    if (CR.Lower.ult(Upper))
      return getPreferredRange(*this, CR, Type);

    // ----U   L-- : this
    // --U   L---- : CR
    if (CR.Lower.ult(Lower))
      return ConstantRange(Lower, CR.Upper);

    // ----U L---- : this
    // --U     L-- : CR
    return CR;
  }
  if (CR.Upper.ule(Lower)) {
    // --U     L-- : this
    // ----U L---- : CR
    if (CR.Lower.ult(Lower))
      return *this;

    // --U   L---- : this
    // ----U   L-- : CR
    return ConstantRange(CR.Lower, Upper);
  }

  // --U L------ : this
  // ------U L-- : CR
  return getPreferredRange(*this, CR, Type);
}

ConstantRange ConstantRange::unionWith(const ConstantRange &CR,
                                       PreferredRangeType Type) const {
  assert(isFloat == CR.isFloat);

  if (   isFullSet() || CR.isEmptySet()) return *this;
  if (CR.isFullSet() ||    isEmptySet()) return CR;
  // Handle union with NaN that is not empty
  if (CR.UpperFP.isNaN()) return ConstantRange(LowerFP, UpperFP, true);
  if (UpperFP.isNaN()) return ConstantRange(CR.LowerFP, CR.UpperFP, true);

  assert(getBitWidth() == CR.getBitWidth() &&
         "ConstantRange types don't agree!");

  if (!isUpperWrapped() && CR.isUpperWrapped())
    return CR.unionWith(*this, Type);

  if (!isUpperWrapped() && !CR.isUpperWrapped()) {
    if (isFloat) {
      return ConstantRange(minimum(LowerFP, CR.LowerFP),
                           maximum(UpperFP, CR.UpperFP), canBeNaN || CR.canBeNaN);
    }
    //        L---U  and  L---U        : this
    //  L---U                   L---U  : CR
    // result in one of
    //  L---------U
    // -----U L-----
    if (CR.Upper.ult(Lower) || Upper.ult(CR.Lower))
      return getPreferredRange(
          ConstantRange(Lower, CR.Upper), ConstantRange(CR.Lower, Upper), Type);

    APInt L = CR.Lower.ult(Lower) ? CR.Lower : Lower;
    APInt U = (CR.Upper - 1).ugt(Upper - 1) ? CR.Upper : Upper;

    if (L.isNullValue() && U.isNullValue())
      return getFull();

    return ConstantRange(std::move(L), std::move(U));
  }

  if (!CR.isUpperWrapped()) {
    // ------U   L-----  and  ------U   L----- : this
    //   L--U                            L--U  : CR
    if (!isFloat && (CR.Upper.ule(Upper) || CR.Lower.uge(Lower)))
      return *this;
    // Call maximum/minimum to properly handle +/-0 situations.
    if (isFloat && CR.UpperFP <= UpperFP)
      return ConstantRange(LowerFP, maximum(UpperFP, CR.UpperFP), canBeNaN || CR.canBeNaN);
    if (isFloat && CR.LowerFP >= LowerFP)
      return ConstantRange(minimum(LowerFP, CR.LowerFP), UpperFP, canBeNaN || CR.canBeNaN);

    // ------U   L----- : this
    //    L---------U   : CR
    if (!isFloat && (CR.Lower.ule(Upper) && Lower.ule(CR.Upper)))
      return getFull();
    if (isFloat && (CR.LowerFP <= UpperFP && LowerFP <= CR.UpperFP))
      return getFullFP(canBeNaN || CR.canBeNaN);

    // ----U       L---- : this
    //       L---U       : CR
    // results in one of
    // ----------U L----
    // ----U L----------
    if (!isFloat && Upper.ult(CR.Lower) && CR.Upper.ult(Lower))
      return getPreferredRange(
          ConstantRange(Lower, CR.Upper), ConstantRange(CR.Lower, Upper), Type);
    if (isFloat && UpperFP < CR.LowerFP && CR.UpperFP < LowerFP) {
      assert(Type == Smallest);
      return CR.LowerFP - UpperFP > LowerFP - CR.UpperFP ?
             ConstantRange(CR.LowerFP, UpperFP, canBeNaN || CR.canBeNaN):
             ConstantRange(LowerFP, CR.UpperFP, canBeNaN || CR.canBeNaN);
    }

    // ----U     L----- : this
    //        L----U    : CR
    if (!isFloat && Upper.ult(CR.Lower) && Lower.ule(CR.Upper))
      return ConstantRange(CR.Lower, Upper);
    if (isFloat && UpperFP < CR.LowerFP && LowerFP <= CR.UpperFP)
      return ConstantRange(CR.LowerFP, UpperFP, canBeNaN || CR.canBeNaN);

    // ------U    L---- : this
    //    L-----U       : CR
    if (isFloat)
      return ConstantRange(LowerFP, CR.UpperFP, canBeNaN || CR.canBeNaN);
    assert(CR.Lower.ule(Upper) && CR.Upper.ult(Lower) &&
           "ConstantRange::unionWith missed a case with one range wrapped");
    return ConstantRange(Lower, CR.Upper);
  }

  // ------U    L----  and  ------U    L---- : this
  // -U  L-----------  and  ------------U  L : CR
  if (!isFloat && (CR.Lower.ule(Upper) || Lower.ule(CR.Upper)))
    return getFull();
  if (isFloat && (CR.LowerFP <= UpperFP || LowerFP <= CR.UpperFP))
    return getFullFP(canBeNaN || CR.canBeNaN);

  if (isFloat)
    return ConstantRange(minimum(LowerFP, CR.LowerFP),
                         maximum(UpperFP, CR.UpperFP), canBeNaN || CR.canBeNaN);
  APInt L = CR.Lower.ult(Lower) ? CR.Lower : Lower;
  APInt U = CR.Upper.ugt(Upper) ? CR.Upper : Upper;

  return ConstantRange(std::move(L), std::move(U));
}

ConstantRange ConstantRange::castOp(Instruction::CastOps CastOp,
                                    uint32_t ResultBitWidth) const {
  switch (CastOp) {
  default:
    llvm_unreachable("unsupported cast type");
  case Instruction::Trunc:
    return truncate(ResultBitWidth);
  case Instruction::SExt:
    return signExtend(ResultBitWidth);
  case Instruction::ZExt:
    return zeroExtend(ResultBitWidth);
  case Instruction::BitCast:
    // Bitcast bitwidth needs to match. ppc_fp128 needs special handling
    assert((getBitWidth() == ResultBitWidth) ||
           (ResultBitWidth == 128 && (isFloat && &LowerFP.getSemantics() == &APFloat::PPCDoubleDouble())));
    return isFloat ? getFull(ResultBitWidth) : *this;
  case Instruction::FPToUI:
  case Instruction::FPToSI:
    // TODO: use input range if available
    return getFull(ResultBitWidth);
  case Instruction::IntToPtr:
  case Instruction::PtrToInt:
  case Instruction::AddrSpaceCast:
    // Conservatively return getFull set.
    return getFull(ResultBitWidth);
  };
}

ConstantRange ConstantRange::castOp(Instruction::CastOps CastOp,
                                    const llvm::fltSemantics &Sem) const {
  switch (CastOp) {
  default:
    llvm_unreachable("unsupported cast type");
  case Instruction::FPTrunc:
  case Instruction::FPExt: {
    bool losesInfo1 = false, losesInfo2 = false;
    APFloat ValL = LowerFP, ValU = UpperFP;
    if (isUpperWrapped()) {
      ValL.convert(Sem, APFloat::rmTowardPositive, &losesInfo1);
      ValU.convert(Sem, APFloat::rmTowardNegative, &losesInfo2);
    } else {
      ValL.convert(Sem, APFloat::rmTowardNegative, &losesInfo1);
      ValU.convert(Sem, APFloat::rmTowardPositive, &losesInfo2);
    }
    return ConstantRange(ValL, ValU, canBeNaN);
  }
  case Instruction::BitCast:
    // Bitcast bitwidth needs to match. ppc_fp128 needs special handling
    assert((getBitWidth() == APFloat::getSizeInBits(Sem)) ||
           (getBitWidth() == 128 && &Sem == &APFloat::PPCDoubleDouble()));
    return isFloat ? *this : getFull(Sem);
  case Instruction::UIToFP:
  case Instruction::SIToFP:
    // TODO: use input range if available
    return getFull(Sem);
  };
}

ConstantRange ConstantRange::zeroExtend(uint32_t DstTySize) const {
  assert(!isFloat);
  if (isEmptySet()) return getEmpty(DstTySize);

  unsigned SrcTySize = getBitWidth();
  assert(SrcTySize < DstTySize && "Not a value extension");
  if (isFullSet() || isUpperWrapped()) {
    // Change into [0, 1 << src bit width)
    APInt LowerExt(DstTySize, 0);
    if (!Upper) // special case: [X, 0) -- not really wrapping around
      LowerExt = Lower.zext(DstTySize);
    return ConstantRange(std::move(LowerExt),
                         APInt::getOneBitSet(DstTySize, SrcTySize));
  }

  return ConstantRange(Lower.zext(DstTySize), Upper.zext(DstTySize));
}

ConstantRange ConstantRange::signExtend(uint32_t DstTySize) const {
  assert(!isFloat);
  if (isEmptySet()) return getEmpty(DstTySize);

  unsigned SrcTySize = getBitWidth();
  assert(SrcTySize < DstTySize && "Not a value extension");

  // special case: [X, INT_MIN) -- not really wrapping around
  if (Upper.isMinSignedValue())
    return ConstantRange(Lower.sext(DstTySize), Upper.zext(DstTySize));

  if (isFullSet() || isSignWrappedSet()) {
    return ConstantRange(APInt::getHighBitsSet(DstTySize,DstTySize-SrcTySize+1),
                         APInt::getLowBitsSet(DstTySize, SrcTySize-1) + 1);
  }

  return ConstantRange(Lower.sext(DstTySize), Upper.sext(DstTySize));
}

ConstantRange ConstantRange::truncate(uint32_t DstTySize) const {
  assert(!isFloat);
  assert(getBitWidth() > DstTySize && "Not a value truncation");
  if (isEmptySet())
    return getEmpty(DstTySize);
  if (isFullSet())
    return getFull(DstTySize);

  APInt LowerDiv(Lower), UpperDiv(Upper);
  ConstantRange Union(DstTySize, /*isFullSet=*/false);

  // Analyze wrapped sets in their two parts: [0, Upper) \/ [Lower, MaxValue]
  // We use the non-wrapped set code to analyze the [Lower, MaxValue) part, and
  // then we do the union with [MaxValue, Upper)
  if (isUpperWrapped()) {
    // If Upper is greater than or equal to MaxValue(DstTy), it covers the whole
    // truncated range.
    if (Upper.getActiveBits() > DstTySize ||
        Upper.countTrailingOnes() == DstTySize)
      return getFull(DstTySize);

    Union = ConstantRange(APInt::getMaxValue(DstTySize),Upper.trunc(DstTySize));
    UpperDiv.setAllBits();

    // Union covers the MaxValue case, so return if the remaining range is just
    // MaxValue(DstTy).
    if (LowerDiv == UpperDiv)
      return Union;
  }

  // Chop off the most significant bits that are past the destination bitwidth.
  if (LowerDiv.getActiveBits() > DstTySize) {
    // Mask to just the signficant bits and subtract from LowerDiv/UpperDiv.
    APInt Adjust = LowerDiv & APInt::getBitsSetFrom(getBitWidth(), DstTySize);
    LowerDiv -= Adjust;
    UpperDiv -= Adjust;
  }

  unsigned UpperDivWidth = UpperDiv.getActiveBits();
  if (UpperDivWidth <= DstTySize)
    return ConstantRange(LowerDiv.trunc(DstTySize),
                         UpperDiv.trunc(DstTySize)).unionWith(Union);

  // The truncated value wraps around. Check if we can do better than fullset.
  if (UpperDivWidth == DstTySize + 1) {
    // Clear the MSB so that UpperDiv wraps around.
    UpperDiv.clearBit(DstTySize);
    if (UpperDiv.ult(LowerDiv))
      return ConstantRange(LowerDiv.trunc(DstTySize),
                           UpperDiv.trunc(DstTySize)).unionWith(Union);
  }

  return getFull(DstTySize);
}

ConstantRange ConstantRange::zextOrTrunc(uint32_t DstTySize) const {
  assert(!isFloat);
  unsigned SrcTySize = getBitWidth();
  if (SrcTySize > DstTySize)
    return truncate(DstTySize);
  if (SrcTySize < DstTySize)
    return zeroExtend(DstTySize);
  return *this;
}

ConstantRange ConstantRange::sextOrTrunc(uint32_t DstTySize) const {
  assert(!isFloat);
  unsigned SrcTySize = getBitWidth();
  if (SrcTySize > DstTySize)
    return truncate(DstTySize);
  if (SrcTySize < DstTySize)
    return signExtend(DstTySize);
  return *this;
}

ConstantRange ConstantRange::binaryOp(Instruction::BinaryOps BinOp,
                                      const ConstantRange &Other) const {
  assert(Instruction::isBinaryOp(BinOp) && "Binary operators only!");

  switch (BinOp) {
  case Instruction::Add:
    return add(Other);
  case Instruction::Sub:
    return sub(Other);
  case Instruction::Mul:
    return multiply(Other);
  case Instruction::UDiv:
    return udiv(Other);
  case Instruction::SDiv:
    return sdiv(Other);
  case Instruction::URem:
    return urem(Other);
  case Instruction::SRem:
    return srem(Other);
  case Instruction::Shl:
    return shl(Other);
  case Instruction::LShr:
    return lshr(Other);
  case Instruction::AShr:
    return ashr(Other);
  case Instruction::And:
    return binaryAnd(Other);
  case Instruction::Or:
    return binaryOr(Other);
  case Instruction::Xor:
    return binaryXor(Other);
  case Instruction::FAdd:
    if (isFloat) // We don't support vector types
      return fadd(Other);
    // Fallthrough
  case Instruction::FSub:
    if (isFloat) // We don't support vector types
      return fsub(Other);
    // Fallthrough
  case Instruction::FMul:
    if (isFloat) // We don't support vector types
      return fmultiply(Other);
    // Fallthrough
  case Instruction::FDiv:
    if (isFloat) // We don't support vector types
      return fdivide(Other);
    // Fallthrough
  default:
    // Conservatively return getFull set.
    return getFull();
  }
}

ConstantRange ConstantRange::overflowingBinaryOp(Instruction::BinaryOps BinOp,
                                                 const ConstantRange &Other,
                                                 unsigned NoWrapKind) const {
  assert(Instruction::isBinaryOp(BinOp) && "Binary operators only!");

  switch (BinOp) {
  case Instruction::Add:
    return addWithNoWrap(Other, NoWrapKind);
  case Instruction::Sub:
    return subWithNoWrap(Other, NoWrapKind);
  default:
    // Don't know about this Overflowing Binary Operation.
    // Conservatively fallback to plain binop handling.
    return binaryOp(BinOp, Other);
  }
}

ConstantRange
ConstantRange::fadd(const ConstantRange &Other) const {
  assert(isFloat && Other.isFloat);
  if (isEmptySet() || Other.isEmptySet())
    return getEmpty();

  // If one of the operands can only be NaN, it propagates.
  // Even if the other operand is full set.
  if (isSingleElementFP() && getSingleElementFP()->isNaN())
    return *this;
  if (Other.isSingleElementFP() && Other.getSingleElementFP()->isNaN())
    return Other;

  if (isFullSet() || Other.isFullSet())
    return getFull();

  // Adding infinities of oposing signs generates NaN
  APFloat PosInf = APFloat::getInf(LowerFP.getSemantics(), false);
  APFloat NegInf = APFloat::getInf(LowerFP.getSemantics(), true);
  bool ResNaN = (contains(PosInf) && Other.contains(NegInf)) ||
                (contains(NegInf) && Other.contains(PosInf)) ||
                canBeNaN || Other.canBeNaN;

  if (isUpperWrapped() || Other.isUpperWrapped())
    return ConstantRange(std::move(NegInf), std::move(PosInf), ResNaN);

  APFloat NewLower = LowerFP;
  APFloat NewUpper = UpperFP;
  NewLower.add(Other.LowerFP, APFloat::rmTowardNegative);
  NewUpper.add(Other.UpperFP, APFloat::rmTowardPositive);
  // Give up if any boundaries generate NaN
  if (NewUpper.isNaN() || NewLower.isNaN())
    return getFull();

  return ConstantRange(std::move(NewLower), std::move(NewUpper), ResNaN);
}

ConstantRange
ConstantRange::add(const ConstantRange &Other) const {
  assert(!isFloat);
  if (isEmptySet() || Other.isEmptySet())
    return getEmpty();
  if (isFullSet() || Other.isFullSet())
    return getFull();

  APInt NewLower = getLower() + Other.getLower();
  APInt NewUpper = getUpper() + Other.getUpper() - 1;
  if (NewLower == NewUpper)
    return getFull();

  ConstantRange X = ConstantRange(std::move(NewLower), std::move(NewUpper));
  if (X.isSizeStrictlySmallerThan(*this) ||
      X.isSizeStrictlySmallerThan(Other))
    // We've wrapped, therefore, full set.
    return getFull();
  return X;
}

ConstantRange ConstantRange::addWithNoWrap(const ConstantRange &Other,
                                           unsigned NoWrapKind,
                                           PreferredRangeType RangeType) const {
  // Calculate the range for "X + Y" which is guaranteed not to wrap(overflow).
  // (X is from this, and Y is from Other)
  assert(!isFloat);
  if (isEmptySet() || Other.isEmptySet())
    return getEmpty();
  if (isFullSet() && Other.isFullSet())
    return getFull();

  using OBO = OverflowingBinaryOperator;
  ConstantRange Result = add(Other);

  // If an overflow happens for every value pair in these two constant ranges,
  // we must return Empty set. In this case, we get that for free, because we
  // get lucky that intersection of add() with uadd_sat()/sadd_sat() results
  // in an empty set.

  if (NoWrapKind & OBO::NoSignedWrap)
    Result = Result.intersectWith(sadd_sat(Other), RangeType);

  if (NoWrapKind & OBO::NoUnsignedWrap)
    Result = Result.intersectWith(uadd_sat(Other), RangeType);

  return Result;
}

ConstantRange
ConstantRange::fsub(const ConstantRange &Other) const {
  assert(isFloat && Other.isFloat);
  if (isEmptySet() || Other.isEmptySet())
    return getEmpty();

  // If one of the operands can only be NaN, it propagates.
  // Even if the other operand is full set.
  if (isSingleElementFP() && getSingleElementFP()->isNaN())
    return *this;
  if (Other.isSingleElementFP() && Other.getSingleElementFP()->isNaN())
    return Other;

  if (isFullSet() || Other.isFullSet())
    return getFull();

  // Subtracting infinities of the same sign generates NaN
  APFloat PosInf = APFloat::getInf(LowerFP.getSemantics(), false);
  APFloat NegInf = APFloat::getInf(LowerFP.getSemantics(), true);
  bool ResNaN = (contains(PosInf) && Other.contains(PosInf)) ||
                (contains(NegInf) && Other.contains(NegInf)) ||
                canBeNaN || Other.canBeNaN;

  if (isUpperWrapped() || Other.isUpperWrapped())
    return ConstantRange(std::move(NegInf), std::move(PosInf), ResNaN);

  APFloat NewLower = LowerFP;
  APFloat NewUpper = UpperFP;
  NewLower.subtract(Other.UpperFP, APFloat::rmTowardNegative);
  NewUpper.subtract(Other.LowerFP, APFloat::rmTowardPositive);
  // Give up if any boundaries generate NaN
  if (NewUpper.isNaN() || NewLower.isNaN())
    return getFull();

  return ConstantRange(std::move(NewLower), std::move(NewUpper), ResNaN);
}

ConstantRange
ConstantRange::sub(const ConstantRange &Other) const {
  assert(!isFloat);
  if (isEmptySet() || Other.isEmptySet())
    return getEmpty();
  if (isFullSet() || Other.isFullSet())
    return getFull();

  APInt NewLower = getLower() - Other.getUpper() + 1;
  APInt NewUpper = getUpper() - Other.getLower();
  if (NewLower == NewUpper)
    return getFull();

  ConstantRange X = ConstantRange(std::move(NewLower), std::move(NewUpper));
  if (X.isSizeStrictlySmallerThan(*this) ||
      X.isSizeStrictlySmallerThan(Other))
    // We've wrapped, therefore, full set.
    return getFull();
  return X;
}

ConstantRange ConstantRange::subWithNoWrap(const ConstantRange &Other,
                                           unsigned NoWrapKind,
                                           PreferredRangeType RangeType) const {
  // Calculate the range for "X - Y" which is guaranteed not to wrap(overflow).
  // (X is from this, and Y is from Other)
  assert(!isFloat);
  if (isEmptySet() || Other.isEmptySet())
    return getEmpty();
  if (isFullSet() && Other.isFullSet())
    return getFull();

  using OBO = OverflowingBinaryOperator;
  ConstantRange Result = sub(Other);

  // If an overflow happens for every value pair in these two constant ranges,
  // we must return Empty set. In signed case, we get that for free, because we
  // get lucky that intersection of sub() with ssub_sat() results in an
  // empty set. But for unsigned we must perform the overflow check manually.

  if (NoWrapKind & OBO::NoSignedWrap)
    Result = Result.intersectWith(ssub_sat(Other), RangeType);

  if (NoWrapKind & OBO::NoUnsignedWrap) {
    if (getUnsignedMax().ult(Other.getUnsignedMin()))
      return getEmpty(); // Always overflows.
    Result = Result.intersectWith(usub_sat(Other), RangeType);
  }

  return Result;
}

ConstantRange
ConstantRange::fdivide(const ConstantRange &Other) const {
  assert(isFloat && Other.isFloat);
  if (isEmptySet() || Other.isEmptySet())
    return getEmpty();

  // If one of the operands can only be NaN, it propagates.
  // Even if the other operand is full set.
  if (isSingleElementFP() && getSingleElementFP()->isNaN())
    return *this;
  if (Other.isSingleElementFP() && Other.getSingleElementFP()->isNaN())
    return Other;

  if (isFullSet() || Other.isFullSet())
    return getFull();

  // Useful constants
  APFloat PosInf = APFloat::getInf(LowerFP.getSemantics(), false);
  APFloat NegInf = APFloat::getInf(LowerFP.getSemantics(), true);
  APFloat PosZero = APFloat::getZero(LowerFP.getSemantics(), false);
  APFloat NegZero = APFloat::getZero(LowerFP.getSemantics(), true);

  // Dividing Inf/Inf is NaN, as is 0/0
  auto ContainsInf = [=](const ConstantRange &CR) {
    return CR.contains(PosInf) || CR.contains(NegInf);
  };
  auto ContainsZero = [=](const ConstantRange &CR) {
    return CR.contains(PosZero) || CR.contains(NegZero);
  };
  bool ResNaN = (ContainsInf(*this) && ContainsInf(Other)) ||
                (ContainsZero(Other) && ContainsZero(*this)) ||
                canBeNaN || Other.canBeNaN;

  // Division by +/-Zero:
  // X / 0 == Inf for X > 0, X / -0 == -Inf for X < -0
  // X / 0 == -Inf for X < -0, X / -0 == Inf for X > 0
  bool ResPosInf = (Other.contains(PosZero) && (!LowerFP.isNegative() || !UpperFP.isNegative())) ||
                   (Other.contains(NegZero) && (LowerFP.isNegative() || UpperFP.isNegative()));
  bool ResNegInf = (Other.contains(PosZero) && (LowerFP.isNegative() || UpperFP.isNegative())) ||
                   (Other.contains(NegZero) && (!LowerFP.isNegative() || !UpperFP.isNegative()));
  if (ResPosInf && ResNegInf)
    return ConstantRange(std::move(NegInf), std::move(PosInf), ResNaN);

  // Division by Inf. Both bounds are Inf except for [-Inf, Inf]
  if (Other.LowerFP.isInfinity() && Other.UpperFP.isInfinity() &&
      !(!Other.UpperFP.isNegative() && Other.LowerFP.isNegative())) {
    // +/-Inf / +/- Inf can only be NaN
    if (LowerFP.isInfinity() && UpperFP.isInfinity() &&
        !(!UpperFP.isNegative() && LowerFP.isNegative()))
      return ConstantRange(APFloat::getNaN(LowerFP.getSemantics()));
    // 'this' contains other numbers than +/-Inf
    return ConstantRange(std::move(NegZero), std::move(PosZero), ResNaN);
  }

  // If upperWrapped ranges have not been handled by now, give up.
  if (isUpperWrapped() || Other.isUpperWrapped())
    return ConstantRange(std::move(NegInf), std::move(PosInf), ResNaN);

  APFloat MyBounds[] = {LowerFP, UpperFP};
  APFloat OtherBounds[] = {Other.LowerFP, Other.UpperFP};
  APFloat::roundingMode RoundingModes[] = {APFloat::rmTowardNegative, APFloat::rmTowardPositive};

  SmallVector<APFloat, 8> Bounds;
  for (const auto &MyBound:MyBounds)
    for (const auto &OtherBound:OtherBounds)
     for (auto RM:RoundingModes) {
       APFloat Res = MyBound;
       Res.divide(OtherBound, RM);
       if (!Res.isNaN())
         Bounds.push_back(std::move(Res));
     }
  if (ResPosInf)
    Bounds.push_back(std::move(PosInf));
  if (ResNegInf)
    Bounds.push_back(std::move(PosInf));
  assert(!Bounds.empty());
  auto CompareMin = [](const APFloat &A, const APFloat &B) {
    return llvm::minimum(A, B).bitwiseIsEqual(A); };
  auto CompareMax = [](const APFloat &A, const APFloat &B) {
    return llvm::maximum(A, B).bitwiseIsEqual(B); };
  return ConstantRange(*std::min_element(Bounds.begin(), Bounds.end(), CompareMin),
                       *std::max_element(Bounds.begin(), Bounds.end(), CompareMax), ResNaN);
}

ConstantRange
ConstantRange::fmultiply(const ConstantRange &Other) const {
  assert(isFloat && Other.isFloat);
  if (isEmptySet() || Other.isEmptySet())
    return getEmpty();

  // If one of the operands can only be NaN, it propagates.
  // Even if the other operand is full set.
  if (isSingleElementFP() && getSingleElementFP()->isNaN())
    return *this;
  if (Other.isSingleElementFP() && Other.getSingleElementFP()->isNaN())
    return Other;

  // Full set includes infinities. Multiplying inf results in +/-inf so just
  // return another full set
  if (isFullSet() || Other.isFullSet())
    return getFull();

  // Useful constants
  APFloat PosInf = APFloat::getInf(LowerFP.getSemantics(), false);
  APFloat NegInf = APFloat::getInf(LowerFP.getSemantics(), true);
  APFloat PosZero = APFloat::getZero(LowerFP.getSemantics(), false);
  APFloat NegZero = APFloat::getZero(LowerFP.getSemantics(), true);

  // Handle special cases, where multiplying boudnaries doesn't work:
  // [-Inf, 0] * [-Inf, 0] -- produces [0, Inf] instead of [-0, Inf]
  // [-Inf, 0] * [-0, Inf] -- produces [-Inf, -0] instead of [-Inf, 0]
  // [-0, Inf] * [-Inf, 0] -- produces [-Inf, -0] instead of [-Inf, 0]
  // [-0, Inf] * [-0, Inf] -- produces [0, Inf] instead of [-0, Inf]
  auto IsNegZeroPosInf = [](const ConstantRange &CR) {
    return CR.LowerFP.isNegZero() && CR.UpperFP.isInfinity() && !CR.UpperFP.isNegative();
  };
  auto IsNegInfPosZero = [](const ConstantRange &CR) {
    return CR.UpperFP.isPosZero() && CR.LowerFP.isInfinity() && CR.LowerFP.isNegative();
  };
  if ((IsNegInfPosZero(*this) && IsNegZeroPosInf(Other)) ||
      (IsNegZeroPosInf(*this) && IsNegInfPosZero(Other)))
    return ConstantRange(std::move(NegInf), std::move(PosZero), true);
  if ((IsNegZeroPosInf(*this) && IsNegZeroPosInf(Other)) ||
      (IsNegInfPosZero(*this) && IsNegInfPosZero(Other)))
    return ConstantRange(std::move(NegZero), std::move(PosInf), true);

  // Multiplying any zero by any inf produces NaN
  auto ContainsInf = [=](const ConstantRange &CR) {
    return CR.contains(PosInf) || CR.contains(NegInf);
  };
  auto ContainsZero = [=](const ConstantRange &CR) {
    return CR.contains(PosZero) || CR.contains(NegZero);
  };
  bool ResNaN = (ContainsZero(*this) && ContainsInf(Other)) ||
                (ContainsZero(Other) && ContainsInf(*this)) ||
                canBeNaN || Other.canBeNaN;

  // Multiplication by +/-Zero: Inf * 0 is NaN, any other value * 0 is +/-0
  // This conservatively includes both zeros.
  if ((LowerFP.isZero() && UpperFP.isZero()) ||
      (Other.LowerFP.isZero() && Other.UpperFP.isZero()))
      return ConstantRange(std::move(NegZero), std::move(PosZero), ResNaN);

  // If upperWrapped ranges have not been handled by now, give up.
  if (isUpperWrapped() || Other.isUpperWrapped())
    return ConstantRange(std::move(NegInf), std::move(PosInf), ResNaN);

  APFloat MyBounds[] = {LowerFP, UpperFP};
  APFloat OtherBounds[] = {Other.LowerFP, Other.UpperFP};
  APFloat::roundingMode RoundingModes[] = {APFloat::rmTowardNegative, APFloat::rmTowardPositive};

  SmallVector<APFloat, 8> Bounds;
  for (const auto &MyBound:MyBounds)
    for (const auto &OtherBound:OtherBounds)
     for (auto RM:RoundingModes) {
       APFloat Res = MyBound;
       Res.multiply(OtherBound, RM);
       if (!Res.isNaN())
         Bounds.push_back(std::move(Res));
     }
  assert(!Bounds.empty());

  auto CompareMin = [](const APFloat &A, const APFloat &B) {
    return llvm::minimum(A, B).bitwiseIsEqual(A); };
  auto CompareMax = [](const APFloat &A, const APFloat &B) {
    return llvm::maximum(A, B).bitwiseIsEqual(B); };
  return ConstantRange(*std::min_element(Bounds.begin(), Bounds.end(), CompareMin),
                       *std::max_element(Bounds.begin(), Bounds.end(), CompareMax), ResNaN);
}

ConstantRange
ConstantRange::multiply(const ConstantRange &Other) const {
  // TODO: If either operand is a single element and the multiply is known to
  // be non-wrapping, round the result min and max value to the appropriate
  // multiple of that element. If wrapping is possible, at least adjust the
  // range according to the greatest power-of-two factor of the single element.

  assert(!isFloat);
  if (isEmptySet() || Other.isEmptySet())
    return getEmpty();

  // Multiplication is signedness-independent. However different ranges can be
  // obtained depending on how the input ranges are treated. These different
  // ranges are all conservatively correct, but one might be better than the
  // other. We calculate two ranges; one treating the inputs as unsigned
  // and the other signed, then return the smallest of these ranges.

  // Unsigned range first.
  APInt this_min = getUnsignedMin().zext(getBitWidth() * 2);
  APInt this_max = getUnsignedMax().zext(getBitWidth() * 2);
  APInt Other_min = Other.getUnsignedMin().zext(getBitWidth() * 2);
  APInt Other_max = Other.getUnsignedMax().zext(getBitWidth() * 2);

  ConstantRange Result_zext = ConstantRange(this_min * Other_min,
                                            this_max * Other_max + 1);
  ConstantRange UR = Result_zext.truncate(getBitWidth());

  // If the unsigned range doesn't wrap, and isn't negative then it's a range
  // from one positive number to another which is as good as we can generate.
  // In this case, skip the extra work of generating signed ranges which aren't
  // going to be better than this range.
  if (!UR.isUpperWrapped() &&
      (UR.getUpper().isNonNegative() || UR.getUpper().isMinSignedValue()))
    return UR;

  // Now the signed range. Because we could be dealing with negative numbers
  // here, the lower bound is the smallest of the cartesian product of the
  // lower and upper ranges; for example:
  //   [-1,4) * [-2,3) = min(-1*-2, -1*2, 3*-2, 3*2) = -6.
  // Similarly for the upper bound, swapping min for max.

  this_min = getSignedMin().sext(getBitWidth() * 2);
  this_max = getSignedMax().sext(getBitWidth() * 2);
  Other_min = Other.getSignedMin().sext(getBitWidth() * 2);
  Other_max = Other.getSignedMax().sext(getBitWidth() * 2);

  auto L = {this_min * Other_min, this_min * Other_max,
            this_max * Other_min, this_max * Other_max};
  auto Compare = [](const APInt &A, const APInt &B) { return A.slt(B); };
  ConstantRange Result_sext(std::min(L, Compare), std::max(L, Compare) + 1);
  ConstantRange SR = Result_sext.truncate(getBitWidth());

  return UR.isSizeStrictlySmallerThan(SR) ? UR : SR;
}

ConstantRange
ConstantRange::fmax(const ConstantRange &Other) const {
  assert(isFloat);
  if (isEmptySet() || Other.isEmptySet())
    return getEmpty();
  // X fmax Y is: range(fmax(X_smin, Y_smin), fmax(X_smax, Y_smax))
  if (!isUpperWrapped() && !Other.isUpperWrapped())
    return ConstantRange(maximum(LowerFP, Other.LowerFP),
                         maximum(UpperFP, Other.UpperFP),
                         canBeNaN || Other.canBeNaN);

  if (!isUpperWrapped() && Other.isUpperWrapped())
    return Other.fmax(*this);

  // Handle NaN explicitly
  if (Other.LowerFP.isNaN())
    return ConstantRange(LowerFP, UpperFP, true);
  // If a range is upper wrapped it includes -Inf.
  // This results in all numbers from the other range to appear in the results.
  if (isUpperWrapped() && !Other.isUpperWrapped())
    return ConstantRange(Other.LowerFP, APFloat::getInf(UpperFP.getSemantics()),
                         canBeNaN || Other.canBeNaN);
  return unionWith(Other);
}

ConstantRange
ConstantRange::fmin(const ConstantRange &Other) const {
  assert(isFloat && Other.isFloat);
  if (isEmptySet() || Other.isEmptySet())
    return getEmpty();
  // X fmin Y is: range(fmin(X_smin, Y_smin), fmin(X_smax, Y_smax))
  if (!isUpperWrapped() && !Other.isUpperWrapped())
    return ConstantRange(minimum(LowerFP, Other.LowerFP),
                         minimum(UpperFP, Other.UpperFP),
                         canBeNaN || Other.canBeNaN);
  if (!isUpperWrapped() && Other.isUpperWrapped())
    return Other.fmin(*this);

  // Handle NaN explicitly
  if (Other.LowerFP.isNaN())
    return ConstantRange(LowerFP, UpperFP, true);
  // If a range is upper wrapped it includes Inf.
  // This results in all numbers from the other range to appear in the results.
  if (isUpperWrapped() && !Other.isUpperWrapped())
    return ConstantRange(APFloat::getInf(UpperFP.getSemantics(), true),
                         Other.UpperFP, canBeNaN || Other.canBeNaN);
  return unionWith(Other);
}

ConstantRange
ConstantRange::smax(const ConstantRange &Other) const {
  // X smax Y is: range(smax(X_smin, Y_smin),
  //                    smax(X_smax, Y_smax))
  assert(!isFloat);
  if (isEmptySet() || Other.isEmptySet())
    return getEmpty();
  APInt NewL = APIntOps::smax(getSignedMin(), Other.getSignedMin());
  APInt NewU = APIntOps::smax(getSignedMax(), Other.getSignedMax()) + 1;
  return getNonEmpty(std::move(NewL), std::move(NewU));
}

ConstantRange
ConstantRange::umax(const ConstantRange &Other) const {
  // X umax Y is: range(umax(X_umin, Y_umin),
  //                    umax(X_umax, Y_umax))
  assert(!isFloat);
  if (isEmptySet() || Other.isEmptySet())
    return getEmpty();
  APInt NewL = APIntOps::umax(getUnsignedMin(), Other.getUnsignedMin());
  APInt NewU = APIntOps::umax(getUnsignedMax(), Other.getUnsignedMax()) + 1;
  return getNonEmpty(std::move(NewL), std::move(NewU));
}

ConstantRange
ConstantRange::smin(const ConstantRange &Other) const {
  // X smin Y is: range(smin(X_smin, Y_smin),
  //                    smin(X_smax, Y_smax))
  assert(!isFloat);
  if (isEmptySet() || Other.isEmptySet())
    return getEmpty();
  APInt NewL = APIntOps::smin(getSignedMin(), Other.getSignedMin());
  APInt NewU = APIntOps::smin(getSignedMax(), Other.getSignedMax()) + 1;
  return getNonEmpty(std::move(NewL), std::move(NewU));
}

ConstantRange
ConstantRange::umin(const ConstantRange &Other) const {
  // X umin Y is: range(umin(X_umin, Y_umin),
  //                    umin(X_umax, Y_umax))
  assert(!isFloat);
  if (isEmptySet() || Other.isEmptySet())
    return getEmpty();
  APInt NewL = APIntOps::umin(getUnsignedMin(), Other.getUnsignedMin());
  APInt NewU = APIntOps::umin(getUnsignedMax(), Other.getUnsignedMax()) + 1;
  return getNonEmpty(std::move(NewL), std::move(NewU));
}

ConstantRange
ConstantRange::udiv(const ConstantRange &RHS) const {
  assert(!isFloat);
  if (isEmptySet() || RHS.isEmptySet() || RHS.getUnsignedMax().isNullValue())
    return getEmpty();

  APInt Lower = getUnsignedMin().udiv(RHS.getUnsignedMax());

  APInt RHS_umin = RHS.getUnsignedMin();
  if (RHS_umin.isNullValue()) {
    // We want the lowest value in RHS excluding zero. Usually that would be 1
    // except for a range in the form of [X, 1) in which case it would be X.
    if (RHS.getUpper() == 1)
      RHS_umin = RHS.getLower();
    else
      RHS_umin = 1;
  }

  APInt Upper = getUnsignedMax().udiv(RHS_umin) + 1;
  return getNonEmpty(std::move(Lower), std::move(Upper));
}

ConstantRange ConstantRange::sdiv(const ConstantRange &RHS) const {
  // We split up the LHS and RHS into positive and negative components
  // and then also compute the positive and negative components of the result
  // separately by combining division results with the appropriate signs.
  assert(!isFloat);
  APInt Zero = APInt::getNullValue(getBitWidth());
  APInt SignedMin = APInt::getSignedMinValue(getBitWidth());
  ConstantRange PosFilter(APInt(getBitWidth(), 1), SignedMin);
  ConstantRange NegFilter(SignedMin, Zero);
  ConstantRange PosL = intersectWith(PosFilter);
  ConstantRange NegL = intersectWith(NegFilter);
  ConstantRange PosR = RHS.intersectWith(PosFilter);
  ConstantRange NegR = RHS.intersectWith(NegFilter);

  ConstantRange PosRes = getEmpty();
  if (!PosL.isEmptySet() && !PosR.isEmptySet())
    // pos / pos = pos.
    PosRes = ConstantRange(PosL.Lower.sdiv(PosR.Upper - 1),
                           (PosL.Upper - 1).sdiv(PosR.Lower) + 1);

  if (!NegL.isEmptySet() && !NegR.isEmptySet()) {
    // neg / neg = pos.
    //
    // We need to deal with one tricky case here: SignedMin / -1 is UB on the
    // IR level, so we'll want to exclude this case when calculating bounds.
    // (For APInts the operation is well-defined and yields SignedMin.) We
    // handle this by dropping either SignedMin from the LHS or -1 from the RHS.
    APInt Lo = (NegL.Upper - 1).sdiv(NegR.Lower);
    if (NegL.Lower.isMinSignedValue() && NegR.Upper.isNullValue()) {
      // Remove -1 from the LHS. Skip if it's the only element, as this would
      // leave us with an empty set.
      if (!NegR.Lower.isAllOnesValue()) {
        APInt AdjNegRUpper;
        if (RHS.Lower.isAllOnesValue())
          // Negative part of [-1, X] without -1 is [SignedMin, X].
          AdjNegRUpper = RHS.Upper;
        else
          // [X, -1] without -1 is [X, -2].
          AdjNegRUpper = NegR.Upper - 1;

        PosRes = PosRes.unionWith(
            ConstantRange(Lo, NegL.Lower.sdiv(AdjNegRUpper - 1) + 1));
      }

      // Remove SignedMin from the RHS. Skip if it's the only element, as this
      // would leave us with an empty set.
      if (NegL.Upper != SignedMin + 1) {
        APInt AdjNegLLower;
        if (Upper == SignedMin + 1)
          // Negative part of [X, SignedMin] without SignedMin is [X, -1].
          AdjNegLLower = Lower;
        else
          // [SignedMin, X] without SignedMin is [SignedMin + 1, X].
          AdjNegLLower = NegL.Lower + 1;

        PosRes = PosRes.unionWith(
            ConstantRange(std::move(Lo),
                          AdjNegLLower.sdiv(NegR.Upper - 1) + 1));
      }
    } else {
      PosRes = PosRes.unionWith(
          ConstantRange(std::move(Lo), NegL.Lower.sdiv(NegR.Upper - 1) + 1));
    }
  }

  ConstantRange NegRes = getEmpty();
  if (!PosL.isEmptySet() && !NegR.isEmptySet())
    // pos / neg = neg.
    NegRes = ConstantRange((PosL.Upper - 1).sdiv(NegR.Upper - 1),
                           PosL.Lower.sdiv(NegR.Lower) + 1);

  if (!NegL.isEmptySet() && !PosR.isEmptySet())
    // neg / pos = neg.
    NegRes = NegRes.unionWith(
        ConstantRange(NegL.Lower.sdiv(PosR.Lower),
                      (NegL.Upper - 1).sdiv(PosR.Upper - 1) + 1));

  // Prefer a non-wrapping signed range here.
  ConstantRange Res = NegRes.unionWith(PosRes, PreferredRangeType::Signed);

  // Preserve the zero that we dropped when splitting the LHS by sign.
  if (contains(Zero) && (!PosR.isEmptySet() || !NegR.isEmptySet()))
    Res = Res.unionWith(ConstantRange(Zero));
  return Res;
}

ConstantRange ConstantRange::urem(const ConstantRange &RHS) const {
  assert(!isFloat);
  if (isEmptySet() || RHS.isEmptySet() || RHS.getUnsignedMax().isNullValue())
    return getEmpty();

  // L % R for L < R is L.
  if (getUnsignedMax().ult(RHS.getUnsignedMin()))
    return *this;

  // L % R is <= L and < R.
  APInt Upper = APIntOps::umin(getUnsignedMax(), RHS.getUnsignedMax() - 1) + 1;
  return getNonEmpty(APInt::getNullValue(getBitWidth()), std::move(Upper));
}

ConstantRange ConstantRange::srem(const ConstantRange &RHS) const {
  assert(!isFloat);
  if (isEmptySet() || RHS.isEmptySet())
    return getEmpty();

  ConstantRange AbsRHS = RHS.abs();
  APInt MinAbsRHS = AbsRHS.getUnsignedMin();
  APInt MaxAbsRHS = AbsRHS.getUnsignedMax();

  // Modulus by zero is UB.
  if (MaxAbsRHS.isNullValue())
    return getEmpty();

  if (MinAbsRHS.isNullValue())
    ++MinAbsRHS;

  APInt MinLHS = getSignedMin(), MaxLHS = getSignedMax();

  if (MinLHS.isNonNegative()) {
    // L % R for L < R is L.
    if (MaxLHS.ult(MinAbsRHS))
      return *this;

    // L % R is <= L and < R.
    APInt Upper = APIntOps::umin(MaxLHS, MaxAbsRHS - 1) + 1;
    return ConstantRange(APInt::getNullValue(getBitWidth()), std::move(Upper));
  }

  // Same basic logic as above, but the result is negative.
  if (MaxLHS.isNegative()) {
    if (MinLHS.ugt(-MinAbsRHS))
      return *this;

    APInt Lower = APIntOps::umax(MinLHS, -MaxAbsRHS + 1);
    return ConstantRange(std::move(Lower), APInt(getBitWidth(), 1));
  }

  // LHS range crosses zero.
  APInt Lower = APIntOps::umax(MinLHS, -MaxAbsRHS + 1);
  APInt Upper = APIntOps::umin(MaxLHS, MaxAbsRHS - 1) + 1;
  return ConstantRange(std::move(Lower), std::move(Upper));
}

ConstantRange
ConstantRange::binaryAnd(const ConstantRange &Other) const {
  assert(!isFloat);
  if (isEmptySet() || Other.isEmptySet())
    return getEmpty();

  // Use APInt's implementation of AND for single element ranges.
  if (isSingleElement() && Other.isSingleElement())
    return {*getSingleElement() & *Other.getSingleElement()};

  // TODO: replace this with something less conservative

  APInt umin = APIntOps::umin(Other.getUnsignedMax(), getUnsignedMax());
  return getNonEmpty(APInt::getNullValue(getBitWidth()), std::move(umin) + 1);
}

ConstantRange
ConstantRange::binaryOr(const ConstantRange &Other) const {
  assert(!isFloat);
  if (isEmptySet() || Other.isEmptySet())
    return getEmpty();

  // Use APInt's implementation of OR for single element ranges.
  if (isSingleElement() && Other.isSingleElement())
    return {*getSingleElement() | *Other.getSingleElement()};

  // TODO: replace this with something less conservative

  APInt umax = APIntOps::umax(getUnsignedMin(), Other.getUnsignedMin());
  return getNonEmpty(std::move(umax), APInt::getNullValue(getBitWidth()));
}

ConstantRange ConstantRange::binaryXor(const ConstantRange &Other) const {
  if (isEmptySet() || Other.isEmptySet())
    return getEmpty();

  // Use APInt's implementation of XOR for single element ranges.
  if (isSingleElement() && Other.isSingleElement())
    return {*getSingleElement() ^ *Other.getSingleElement()};

  // TODO: replace this with something less conservative
  return getFull();
}

ConstantRange
ConstantRange::shl(const ConstantRange &Other) const {
  assert(!isFloat);
  if (isEmptySet() || Other.isEmptySet())
    return getEmpty();

  APInt max = getUnsignedMax();
  APInt Other_umax = Other.getUnsignedMax();

  // If we are shifting by maximum amount of
  // zero return return the original range.
  if (Other_umax.isNullValue())
    return *this;
  // there's overflow!
  if (Other_umax.ugt(max.countLeadingZeros()))
    return getFull();

  // FIXME: implement the other tricky cases

  APInt min = getUnsignedMin();
  min <<= Other.getUnsignedMin();
  max <<= Other_umax;

  return ConstantRange(std::move(min), std::move(max) + 1);
}

ConstantRange
ConstantRange::lshr(const ConstantRange &Other) const {
  assert(!isFloat);
  if (isEmptySet() || Other.isEmptySet())
    return getEmpty();

  APInt max = getUnsignedMax().lshr(Other.getUnsignedMin()) + 1;
  APInt min = getUnsignedMin().lshr(Other.getUnsignedMax());
  return getNonEmpty(std::move(min), std::move(max));
}

ConstantRange
ConstantRange::ashr(const ConstantRange &Other) const {
  assert(!isFloat);
  if (isEmptySet() || Other.isEmptySet())
    return getEmpty();

  // May straddle zero, so handle both positive and negative cases.
  // 'PosMax' is the upper bound of the result of the ashr
  // operation, when Upper of the LHS of ashr is a non-negative.
  // number. Since ashr of a non-negative number will result in a
  // smaller number, the Upper value of LHS is shifted right with
  // the minimum value of 'Other' instead of the maximum value.
  APInt PosMax = getSignedMax().ashr(Other.getUnsignedMin()) + 1;

  // 'PosMin' is the lower bound of the result of the ashr
  // operation, when Lower of the LHS is a non-negative number.
  // Since ashr of a non-negative number will result in a smaller
  // number, the Lower value of LHS is shifted right with the
  // maximum value of 'Other'.
  APInt PosMin = getSignedMin().ashr(Other.getUnsignedMax());

  // 'NegMax' is the upper bound of the result of the ashr
  // operation, when Upper of the LHS of ashr is a negative number.
  // Since 'ashr' of a negative number will result in a bigger
  // number, the Upper value of LHS is shifted right with the
  // maximum value of 'Other'.
  APInt NegMax = getSignedMax().ashr(Other.getUnsignedMax()) + 1;

  // 'NegMin' is the lower bound of the result of the ashr
  // operation, when Lower of the LHS of ashr is a negative number.
  // Since 'ashr' of a negative number will result in a bigger
  // number, the Lower value of LHS is shifted right with the
  // minimum value of 'Other'.
  APInt NegMin = getSignedMin().ashr(Other.getUnsignedMin());

  APInt max, min;
  if (getSignedMin().isNonNegative()) {
    // Upper and Lower of LHS are non-negative.
    min = PosMin;
    max = PosMax;
  } else if (getSignedMax().isNegative()) {
    // Upper and Lower of LHS are negative.
    min = NegMin;
    max = NegMax;
  } else {
    // Upper is non-negative and Lower is negative.
    min = NegMin;
    max = PosMax;
  }
  return getNonEmpty(std::move(min), std::move(max));
}

ConstantRange ConstantRange::uadd_sat(const ConstantRange &Other) const {
  assert(!isFloat);
  if (isEmptySet() || Other.isEmptySet())
    return getEmpty();

  APInt NewL = getUnsignedMin().uadd_sat(Other.getUnsignedMin());
  APInt NewU = getUnsignedMax().uadd_sat(Other.getUnsignedMax()) + 1;
  return getNonEmpty(std::move(NewL), std::move(NewU));
}

ConstantRange ConstantRange::sadd_sat(const ConstantRange &Other) const {
  assert(!isFloat);
  if (isEmptySet() || Other.isEmptySet())
    return getEmpty();

  APInt NewL = getSignedMin().sadd_sat(Other.getSignedMin());
  APInt NewU = getSignedMax().sadd_sat(Other.getSignedMax()) + 1;
  return getNonEmpty(std::move(NewL), std::move(NewU));
}

ConstantRange ConstantRange::usub_sat(const ConstantRange &Other) const {
  assert(!isFloat);
  if (isEmptySet() || Other.isEmptySet())
    return getEmpty();

  APInt NewL = getUnsignedMin().usub_sat(Other.getUnsignedMax());
  APInt NewU = getUnsignedMax().usub_sat(Other.getUnsignedMin()) + 1;
  return getNonEmpty(std::move(NewL), std::move(NewU));
}

ConstantRange ConstantRange::ssub_sat(const ConstantRange &Other) const {
  assert(!isFloat);
  if (isEmptySet() || Other.isEmptySet())
    return getEmpty();

  APInt NewL = getSignedMin().ssub_sat(Other.getSignedMax());
  APInt NewU = getSignedMax().ssub_sat(Other.getSignedMin()) + 1;
  return getNonEmpty(std::move(NewL), std::move(NewU));
}

ConstantRange ConstantRange::umul_sat(const ConstantRange &Other) const {
  assert(!isFloat);
  if (isEmptySet() || Other.isEmptySet())
    return getEmpty();

  APInt NewL = getUnsignedMin().umul_sat(Other.getUnsignedMin());
  APInt NewU = getUnsignedMax().umul_sat(Other.getUnsignedMax()) + 1;
  return getNonEmpty(std::move(NewL), std::move(NewU));
}

ConstantRange ConstantRange::smul_sat(const ConstantRange &Other) const {
  assert(!isFloat);
  if (isEmptySet() || Other.isEmptySet())
    return getEmpty();

  // Because we could be dealing with negative numbers here, the lower bound is
  // the smallest of the cartesian product of the lower and upper ranges;
  // for example:
  //   [-1,4) * [-2,3) = min(-1*-2, -1*2, 3*-2, 3*2) = -6.
  // Similarly for the upper bound, swapping min for max.

  APInt this_min = getSignedMin().sext(getBitWidth() * 2);
  APInt this_max = getSignedMax().sext(getBitWidth() * 2);
  APInt Other_min = Other.getSignedMin().sext(getBitWidth() * 2);
  APInt Other_max = Other.getSignedMax().sext(getBitWidth() * 2);

  auto L = {this_min * Other_min, this_min * Other_max, this_max * Other_min,
            this_max * Other_max};
  auto Compare = [](const APInt &A, const APInt &B) { return A.slt(B); };

  // Note that we wanted to perform signed saturating multiplication,
  // so since we performed plain multiplication in twice the bitwidth,
  // we need to perform signed saturating truncation.
  return getNonEmpty(std::min(L, Compare).truncSSat(getBitWidth()),
                     std::max(L, Compare).truncSSat(getBitWidth()) + 1);
}

ConstantRange ConstantRange::ushl_sat(const ConstantRange &Other) const {
  assert(!isFloat);
  if (isEmptySet() || Other.isEmptySet())
    return getEmpty();

  APInt NewL = getUnsignedMin().ushl_sat(Other.getUnsignedMin());
  APInt NewU = getUnsignedMax().ushl_sat(Other.getUnsignedMax()) + 1;
  return getNonEmpty(std::move(NewL), std::move(NewU));
}

ConstantRange ConstantRange::sshl_sat(const ConstantRange &Other) const {
  assert(!isFloat);
  if (isEmptySet() || Other.isEmptySet())
    return getEmpty();

  APInt Min = getSignedMin(), Max = getSignedMax();
  APInt ShAmtMin = Other.getUnsignedMin(), ShAmtMax = Other.getUnsignedMax();
  APInt NewL = Min.sshl_sat(Min.isNonNegative() ? ShAmtMin : ShAmtMax);
  APInt NewU = Max.sshl_sat(Max.isNegative() ? ShAmtMin : ShAmtMax) + 1;
  return getNonEmpty(std::move(NewL), std::move(NewU));
}

ConstantRange ConstantRange::inverse() const {
  if (isFullSet())
    return getEmpty();
  if (isEmptySet())
    return getFull();

  if (!isFloat)
    return ConstantRange(Upper, Lower);

  // Handle 'almost full' range
  if (LowerFP.isNegative() && LowerFP.isInfinity() && !UpperFP.isNegative() && UpperFP.isInfinity())
    return APFloat::getNaN(LowerFP.getSemantics());

  APFloat NewLower = UpperFP;
  if ((NewLower.isInfinity() && !NewLower.isNegative()) || NewLower.isNaN())
    NewLower = APFloat::getInf(LowerFP.getSemantics(), true);
  else
    NewLower = ZeroNext(NewLower, false);

  APFloat NewUpper = LowerFP;
  if ((NewUpper.isInfinity() && NewUpper.isNegative()) || NewUpper.isNaN())
    NewUpper = APFloat::getInf(UpperFP.getSemantics(), false);
  else
    NewUpper = ZeroNext(NewUpper, true);

  return ConstantRange(::std::move(NewLower), ::std::move(NewUpper), !canBeNaN);
}

ConstantRange ConstantRange::abs() const {
  assert(!isFloat);
  if (isEmptySet())
    return getEmpty();

  if (isSignWrappedSet()) {
    APInt Lo;
    // Check whether the range crosses zero.
    if (Upper.isStrictlyPositive() || !Lower.isStrictlyPositive())
      Lo = APInt::getNullValue(getBitWidth());
    else
      Lo = APIntOps::umin(Lower, -Upper + 1);

    // SignedMin is included in the result range.
    return ConstantRange(Lo, APInt::getSignedMinValue(getBitWidth()) + 1);
  }

  APInt SMin = getSignedMin(), SMax = getSignedMax();

  // All non-negative.
  if (SMin.isNonNegative())
    return *this;

  // All negative.
  if (SMax.isNegative())
    return ConstantRange(-SMax, -SMin + 1);

  // Range crosses zero.
  return ConstantRange(APInt::getNullValue(getBitWidth()),
                       APIntOps::umax(-SMin, SMax) + 1);
}

ConstantRange::OverflowResult ConstantRange::unsignedAddMayOverflow(
    const ConstantRange &Other) const {
  assert(!isFloat);
  if (isEmptySet() || Other.isEmptySet())
    return OverflowResult::MayOverflow;

  APInt Min = getUnsignedMin(), Max = getUnsignedMax();
  APInt OtherMin = Other.getUnsignedMin(), OtherMax = Other.getUnsignedMax();

  // a u+ b overflows high iff a u> ~b.
  if (Min.ugt(~OtherMin))
    return OverflowResult::AlwaysOverflowsHigh;
  if (Max.ugt(~OtherMax))
    return OverflowResult::MayOverflow;
  return OverflowResult::NeverOverflows;
}

ConstantRange::OverflowResult ConstantRange::signedAddMayOverflow(
    const ConstantRange &Other) const {
  assert(!isFloat);
  if (isEmptySet() || Other.isEmptySet())
    return OverflowResult::MayOverflow;

  APInt Min = getSignedMin(), Max = getSignedMax();
  APInt OtherMin = Other.getSignedMin(), OtherMax = Other.getSignedMax();

  APInt SignedMin = APInt::getSignedMinValue(getBitWidth());
  APInt SignedMax = APInt::getSignedMaxValue(getBitWidth());

  // a s+ b overflows high iff a s>=0 && b s>= 0 && a s> smax - b.
  // a s+ b overflows low iff a s< 0 && b s< 0 && a s< smin - b.
  if (Min.isNonNegative() && OtherMin.isNonNegative() &&
      Min.sgt(SignedMax - OtherMin))
    return OverflowResult::AlwaysOverflowsHigh;
  if (Max.isNegative() && OtherMax.isNegative() &&
      Max.slt(SignedMin - OtherMax))
    return OverflowResult::AlwaysOverflowsLow;

  if (Max.isNonNegative() && OtherMax.isNonNegative() &&
      Max.sgt(SignedMax - OtherMax))
    return OverflowResult::MayOverflow;
  if (Min.isNegative() && OtherMin.isNegative() &&
      Min.slt(SignedMin - OtherMin))
    return OverflowResult::MayOverflow;

  return OverflowResult::NeverOverflows;
}

ConstantRange::OverflowResult ConstantRange::unsignedSubMayOverflow(
    const ConstantRange &Other) const {
  assert(!isFloat);
  if (isEmptySet() || Other.isEmptySet())
    return OverflowResult::MayOverflow;

  APInt Min = getUnsignedMin(), Max = getUnsignedMax();
  APInt OtherMin = Other.getUnsignedMin(), OtherMax = Other.getUnsignedMax();

  // a u- b overflows low iff a u< b.
  if (Max.ult(OtherMin))
    return OverflowResult::AlwaysOverflowsLow;
  if (Min.ult(OtherMax))
    return OverflowResult::MayOverflow;
  return OverflowResult::NeverOverflows;
}

ConstantRange::OverflowResult ConstantRange::signedSubMayOverflow(
    const ConstantRange &Other) const {
  assert(!isFloat);
  if (isEmptySet() || Other.isEmptySet())
    return OverflowResult::MayOverflow;

  APInt Min = getSignedMin(), Max = getSignedMax();
  APInt OtherMin = Other.getSignedMin(), OtherMax = Other.getSignedMax();

  APInt SignedMin = APInt::getSignedMinValue(getBitWidth());
  APInt SignedMax = APInt::getSignedMaxValue(getBitWidth());

  // a s- b overflows high iff a s>=0 && b s< 0 && a s> smax + b.
  // a s- b overflows low iff a s< 0 && b s>= 0 && a s< smin + b.
  if (Min.isNonNegative() && OtherMax.isNegative() &&
      Min.sgt(SignedMax + OtherMax))
    return OverflowResult::AlwaysOverflowsHigh;
  if (Max.isNegative() && OtherMin.isNonNegative() &&
      Max.slt(SignedMin + OtherMin))
    return OverflowResult::AlwaysOverflowsLow;

  if (Max.isNonNegative() && OtherMin.isNegative() &&
      Max.sgt(SignedMax + OtherMin))
    return OverflowResult::MayOverflow;
  if (Min.isNegative() && OtherMax.isNonNegative() &&
      Min.slt(SignedMin + OtherMax))
    return OverflowResult::MayOverflow;

  return OverflowResult::NeverOverflows;
}

ConstantRange::OverflowResult ConstantRange::unsignedMulMayOverflow(
    const ConstantRange &Other) const {
  assert(!isFloat);
  if (isEmptySet() || Other.isEmptySet())
    return OverflowResult::MayOverflow;

  APInt Min = getUnsignedMin(), Max = getUnsignedMax();
  APInt OtherMin = Other.getUnsignedMin(), OtherMax = Other.getUnsignedMax();
  bool Overflow;

  (void) Min.umul_ov(OtherMin, Overflow);
  if (Overflow)
    return OverflowResult::AlwaysOverflowsHigh;

  (void) Max.umul_ov(OtherMax, Overflow);
  if (Overflow)
    return OverflowResult::MayOverflow;

  return OverflowResult::NeverOverflows;
}

void ConstantRange::print(raw_ostream &OS) const {
  if (isFullSet())
    OS << (isFloat ? "full-set-fp" : "full-set");
  else if (isEmptySet())
    OS << (isFloat ? "empty-set-fp" : "empty-set");
  else
    if (isFloat) {
      SmallVector<char, 16> Lo, Up;
      LowerFP.toString(Lo);
      UpperFP.toString(Up);
      OS << "[" << Lo << ", " << Up << "]";
      if (LowerFP.bitwiseIsEqual(UpperFP))
        OS << "*";
      if (canBeNaN)
        OS << " or NaN";
    } else
      OS << "[" << Lower << "," << Upper << ")";
}

#if !defined(NDEBUG) || defined(LLVM_ENABLE_DUMP)
LLVM_DUMP_METHOD void ConstantRange::dump() const {
  print(dbgs());
}
#endif

ConstantRange llvm::getConstantRangeFromMetadata(const MDNode &Ranges) {
  const unsigned NumRanges = Ranges.getNumOperands() / 2;
  assert(NumRanges >= 1 && "Must have at least one range!");
  assert(Ranges.getNumOperands() % 2 == 0 && "Must be a sequence of pairs");

  auto *FirstLow = mdconst::extract<ConstantInt>(Ranges.getOperand(0));
  auto *FirstHigh = mdconst::extract<ConstantInt>(Ranges.getOperand(1));

  ConstantRange CR(FirstLow->getValue(), FirstHigh->getValue());

  for (unsigned i = 1; i < NumRanges; ++i) {
    auto *Low = mdconst::extract<ConstantInt>(Ranges.getOperand(2 * i + 0));
    auto *High = mdconst::extract<ConstantInt>(Ranges.getOperand(2 * i + 1));

    // Note: unionWith will potentially create a range that contains values not
    // contained in any of the original N ranges.
    CR = CR.unionWith(ConstantRange(Low->getValue(), High->getValue()));
  }

  return CR;
}
