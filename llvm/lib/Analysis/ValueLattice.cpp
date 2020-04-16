//===- ValueLattice.cpp - Value constraint analysis -------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/Analysis/ValueLattice.h"

namespace llvm {
raw_ostream &operator<<(raw_ostream &OS, const ValueLatticeElement &Val) {
  if (Val.isUnknown())
    return OS << "unknown";
  if (Val.isUndef())
    return OS << "undef";
  if (Val.isOverdefined())
    return OS << "overdefined";

  if (Val.isNotConstant())
    return OS << "notconstant<" << *Val.getNotConstant() << ">";

  if (Val.isConstantRangeIncludingUndef()) {
    const ConstantRange &CR = Val.getConstantRange(true);
    if (CR.getIsFloat()) {
      return OS << "constantrange-fp incl. undef <" << CR << ">";
    } else {
      return OS << "constantrange incl. undef <" << CR.getLower() << ", "
                                                 << CR.getUpper() << ">";
    }
  }

  if (Val.isConstantRange()) {
    const ConstantRange &CR = Val.getConstantRange(true);
    if (CR.getIsFloat()) {
      return OS << "constantrange-fp<" << CR << ">";
    } else {
      return OS << "constantrange<" << CR.getLower() << ", "
                                    << CR.getUpper() << ">";
    }
  }

  return OS << "constant<" << *Val.getConstant() << ">";
}
} // end namespace llvm
