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

  if (Val.isConstantRangeIncludingUndef())
    return OS << "constantrange incl. undef <"
              << Val.getConstantRange(true).getLower() << ", "
              << Val.getConstantRange(true).getUpper() << ">";

  if (Val.isConstantRange())
    return OS << "constantrange<" << Val.getConstantRange().getLower() << ", "
              << Val.getConstantRange().getUpper() << ">";
  return OS << "constant<" << *Val.getConstant() << ">";
}

ValueLatticePool::ValueLatticePool() {
  Unknown = new (BPA.Allocate()) ValueLatticeElement();
  Overdefined = new (BPA.Allocate()) ValueLatticeElement(
      ValueLatticeElement::getOverdefined());
}

const ValueLatticeElement *ValueLatticePool::getConstant(Constant *C) {
  auto It = ConstantElems.find(C);
  if (It != ConstantElems.end())
    return It->second;

  const ValueLatticeElement *Elem = getElement(ValueLatticeElement::get(C));
  ConstantElems.insert({C, Elem});
  return Elem;
}

const ValueLatticeElement *ValueLatticePool::getNotConstant(Constant *C) {
  return getElement(ValueLatticeElement::getNot(C));
}

const ValueLatticeElement *
ValueLatticePool::getRange(const ConstantRange &CR, bool MayIncludeUndef) {
  return getElement(ValueLatticeElement::getRange(CR, MayIncludeUndef));
}

const ValueLatticeElement *
ValueLatticePool::getElement(const ValueLatticeElement &Elem) {
  if (Elem.isUnknown())
    return Unknown;
  if (Elem.isOverdefined())
    return Overdefined;

  auto It = Elems.find(&Elem);
  if (It != Elems.end())
    return *It;

  const ValueLatticeElement *NewElem =
    new (BPA.Allocate()) ValueLatticeElement(Elem);
  Elems.insert(NewElem);
  return NewElem;
}

} // end namespace llvm
