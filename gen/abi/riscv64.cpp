//===-- gen/abi/riscv64.cpp - RISCV ABI description -------------*- C++ -*-===//
//
//                         LDC – the LLVM D compiler
//
// This file is distributed under the BSD-style LDC license. See the LICENSE
// file for details.
//
//===----------------------------------------------------------------------===//
//
// ABI spec:
// https://github.com/riscv-non-isa/riscv-elf-psabi-doc/blob/master/riscv-cc.adoc
//
// Parameterized by XLen (32 for rv32 / ESP32-C3/C6/P4, 64 for rv64) and FLen
// (the ABI float width: 0 soft / ilp32 / lp64, 32 ilp32f / lp64f, 64 ilp32d /
// lp64d). Hardware-float struct flattening (FPCC) only applies when FLen != 0.
//
//===----------------------------------------------------------------------===//

#include "gen/abi/abi.h"
#include "gen/abi/generic.h"
#include "gen/dvalue.h"
#include "gen/irstate.h"
#include "gen/llvmhelpers.h"
#include "gen/tollvm.h"

using namespace dmd;

namespace {
// Bit-casts an aggregate to a pair of XLen-wide integers (e.g. {i32,i32} on
// rv32, {i64,i64} on rv64) to avoid mis-alignment of the integer coercion.
struct IntegerPairRewrite : BaseBitcastABIRewrite {
  const unsigned xlenBits;
  explicit IntegerPairRewrite(unsigned xlenBits) : xlenBits(xlenBits) {}
  LLType *type(Type *t) override {
    auto *i = LLIntegerType::get(gIR->context(), xlenBits);
    return LLStructType::get(gIR->context(), {i, i});
  }
};

struct FlattenedFields {
  struct FlattenedField {
    Type *ty = nullptr;
    unsigned offset = 0;
  };
  FlattenedField fields[2];
  int length = 0; // use -1 to represent "not FPCC-eligible"
};

// Recursively flatten a POD struct into at most 2 FPCC-eligible fields.
// A field is eligible only if it fits the ABI register width: a float must be
// <= FLen, an integer/pointer must be <= XLen. Otherwise the struct is not
// hardware-float eligible (length = -1) and uses the integer ABI.
FlattenedFields visitStructFields(Type *ty, unsigned baseOffset, unsigned XLen,
                                  unsigned FLen) {
  FlattenedFields result;
  if (auto ts = ty->toBasetype()->isTypeStruct()) {
    for (auto fi : ts->sym->fields) {
      auto sub =
          visitStructFields(fi->type, baseOffset + fi->offset, XLen, FLen);
      if (sub.length == -1 || result.length + sub.length > 2) {
        result.length = -1;
        return result;
      }
      for (unsigned i = 0; i < (unsigned)sub.length; ++i) {
        result.fields[result.length++] = sub.fields[i];
      }
    }
    return result;
  }
  switch (ty->toBasetype()->ty) {
  case TY::Tcomplex32: // {float32, float32}
    if (FLen < 32) {
      result.length = -1;
      break;
    }
    result.fields[0].ty = pointerTo(Type::tfloat32);
    result.fields[1].ty = pointerTo(Type::tfloat32);
    result.fields[0].offset = baseOffset;
    result.fields[1].offset = baseOffset + 4;
    result.length = 2;
    break;
  case TY::Tcomplex64: // {float64, float64}
    if (FLen < 64) {
      result.length = -1;
      break;
    }
    result.fields[0].ty = pointerTo(Type::tfloat64);
    result.fields[1].ty = pointerTo(Type::tfloat64);
    result.fields[0].offset = baseOffset;
    result.fields[1].offset = baseOffset + 8;
    result.length = 2;
    break;
  default: {
    Type *bt = ty->toBasetype();
    const auto bits = size(bt) * 8;
    const bool isFloat = bt->isFloating();
    // float must fit FLen; integer/pointer must fit XLen.
    if (isFloat ? (FLen == 0 || bits > FLen) : (bits > XLen)) {
      result.length = -1;
      break;
    }
    result.fields[0].ty = bt;
    result.fields[0].offset = baseOffset;
    result.length = 1;
    break;
  }
  }
  return result;
}

bool requireHardfloatRewrite(Type *ty, unsigned XLen, unsigned FLen) {
  if (FLen == 0) // soft-float ABI: no FPCC struct flattening
    return false;
  if (!ty->toBasetype()->isTypeStruct())
    return false;
  auto result = visitStructFields(ty, 0, XLen, FLen);
  if (result.length <= 0)
    return false;
  if (result.length == 1)
    return isFloating(result.fields[0].ty);
  return isFloating(result.fields[0].ty) || isFloating(result.fields[1].ty);
}

struct HardfloatRewrite : ABIRewrite {
  const unsigned XLen;
  const unsigned FLen;
  HardfloatRewrite(unsigned XLen, unsigned FLen) : XLen(XLen), FLen(FLen) {}
  LLValue *put(DValue *dv, bool, bool) override {
    // realign fields
    const auto flat = visitStructFields(dv->type, 0, XLen, FLen);
    LLType *asType = type(dv->type, flat);
    const unsigned alignment = getABITypeAlign(asType);
    assert(dv->isLVal());
    LLValue *address = DtoLVal(dv);
    LLValue *buffer =
        DtoRawAlloca(asType, alignment, ".HardfloatRewrite_arg_storage");
    for (unsigned i = 0; i < (unsigned)flat.length; ++i) {
      DtoMemCpy(DtoGEP(asType, buffer, 0, i),
                DtoGEP1(getI8Type(), address, flat.fields[i].offset),
                DtoConstSize_t(size(flat.fields[i].ty)));
    }
    return DtoLoad(asType, buffer, ".HardfloatRewrite_arg");
  }
  LLValue *getLVal(Type *dty, LLValue *v) override {
    // inverse operation of method "put"
    const auto flat = visitStructFields(dty, 0, XLen, FLen);
    LLType *asType = type(dty, flat);
    const unsigned alignment = DtoAlignment(dty);
    LLValue *buffer = DtoAllocaDump(v, asType, getABITypeAlign(asType),
                                    ".HardfloatRewrite_param");
    LLValue *ret = DtoRawAlloca(DtoType(dty), alignment,
                                ".HardfloatRewrite_param_storage");
    for (unsigned i = 0; i < (unsigned)flat.length; ++i) {
      DtoMemCpy(DtoGEP1(getI8Type(), ret, flat.fields[i].offset),
                DtoGEP(asType, buffer, 0, i),
                DtoConstSize_t(size(flat.fields[i].ty)));
    }
    return ret;
  }
  LLType *type(Type *ty, const FlattenedFields &flat) {
    if (flat.length == 1) {
      return LLStructType::get(gIR->context(), {DtoType(flat.fields[0].ty)},
                               false);
    }
    assert(flat.length == 2);
    LLType *t[2];
    for (unsigned i = 0; i < 2; ++i) {
      t[i] = isFloating(flat.fields[i].ty)
                 ? DtoType(flat.fields[i].ty)
                 : LLIntegerType::get(gIR->context(),
                                      size(flat.fields[i].ty) * 8);
    }
    return LLStructType::get(gIR->context(), {t[0], t[1]}, false);
  }
  LLType *type(Type *ty) override {
    return type(ty, visitStructFields(ty, 0, XLen, FLen));
  }
};
} // anonymous namespace

struct RISCVTargetABI : TargetABI {
private:
  const unsigned XLen;  // 32 or 64 (bits)
  const unsigned FLen;  // 0 (soft), 32 (f), 64 (d)
  HardfloatRewrite hardfloatRewrite;
  IndirectByvalRewrite indirectByvalRewrite;
  IntegerPairRewrite integerPairRewrite;
  IntegerRewrite integerRewrite;

  // 2 registers' worth of bytes — the largest aggregate passed in registers.
  unsigned maxRegBytes() const { return 2 * XLen / 8; }

public:
  RISCVTargetABI(unsigned XLen, unsigned FLen)
      : XLen(XLen), FLen(FLen), hardfloatRewrite(XLen, FLen),
        integerPairRewrite(XLen) {}

  llvm::UWTableKind defaultUnwindTableKind() override {
    return global.params.targetTriple->isOSLinux() ? llvm::UWTableKind::Async
                                                    : llvm::UWTableKind::None;
  }

  Type *vaListType() override {
    // va_list is void*
    return pointerTo(Type::tvoid);
  }
  bool returnInArg(TypeFunction *tf, bool) override {
    Type *rt = tf->next->toBasetype();
    return !isPOD(rt) || size(rt) > maxRegBytes();
  }
  bool passByVal(TypeFunction *, Type *t) override {
    t = t->toBasetype();
    if (t->ty == TY::Tcomplex80) {
      // rewrite it later to bypass the RVal problem
      return false;
    }
    return isPOD(t) && size(t) > maxRegBytes();
  }

  void rewriteVarargs(IrFuncTy &fty,
                      std::vector<IrFuncTyArg *> &args) override {
    for (auto arg : args) {
      if (!arg->byref)
        rewriteArgument(fty, *arg, /*isVararg=*/true);
    }
  }

  void rewriteArgument(IrFuncTy &fty, IrFuncTyArg &arg) override {
    rewriteArgument(fty, arg, /*isVararg=*/false);
  }

  void rewriteArgument(IrFuncTy &fty, IrFuncTyArg &arg, bool isVararg) {
    TargetABI::rewriteArgument(fty, arg);
    if (arg.rewrite)
      return;

    if (!isVararg && requireHardfloatRewrite(arg.type, XLen, FLen)) {
      hardfloatRewrite.applyTo(arg);
      return;
    }

    Type *ty = arg.type->toBasetype();
    if (ty->ty == TY::Tcomplex80) {
      // {real, real} should be passed in memory
      indirectByvalRewrite.applyTo(arg);
      return;
    }

    const unsigned xbytes = XLen / 8;
    if (isAggregate(ty) && size(ty) && size(ty) <= maxRegBytes()) {
      // Force the coercion (not applyToIfNotObsolete): on rv32 an align-4
      // {i32,i32} is memory-equivalent to the source struct and would be left
      // raw, but clang/the psABI want the explicit coerced type (and the raw
      // form is unreliable for under-aligned aggregates).
      if (size(ty) > xbytes && DtoAlignment(ty) < 2 * xbytes) {
        integerPairRewrite.applyTo(arg); // {iXLen, iXLen}
      } else {
        integerRewrite.applyTo(arg);
      }
    }
  }
};

// The public getter for abi.cpp.
TargetABI *getRISCVTargetABI(unsigned XLen, unsigned FLen) {
  return new RISCVTargetABI(XLen, FLen);
}
