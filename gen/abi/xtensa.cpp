//===-- gen/abi/xtensa.cpp ------------------------------------------------===//
//
//                         LDC – the LLVM D compiler
//
// This file is distributed under the BSD-style LDC license. See the LICENSE
// file for details.
//
//===----------------------------------------------------------------------===//
//
// The Xtensa (windowed) C ABI, as implemented by clang/gcc/rust and verified
// against clang/lib/CodeGen/Targets/Xtensa.cpp:
//   - integer/pointer args go in a2..a7 (6 GPR words), returns in a2..a5 (4).
//   - aggregates up to 6 words are flattened into those registers: [N x i32],
//     or [N x i64] when 8-byte aligned, or i128 when 16-byte aligned. Larger
//     aggregates are passed indirectly (LLVM byval) and >16-byte returns use
//     a hidden sret pointer.
//   - floats reuse the integer arg registers (there are no FP arg registers).
//
// Without this, LDC's generic ABI marks every struct byval/sret, which the
// Xtensa backend lowers via memory — breaking register-based C interop on
// ESP32.
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
struct XtensaTargetABI : TargetABI {
private:
  IntegerRewrite integerRewrite;         // -> iN (i32 / i128)
  CompositeToArray32 compositeToArray32; // -> [N x i32]
  CompositeToArray64 compositeToArray64; // -> [N x i64]

  static const unsigned MaxArgRegs = 6; // a2..a7 -> 24 bytes
  static const unsigned MaxRetRegs = 4; // a2..a5 -> 16 bytes

public:
  bool returnInArg(TypeFunction *tf, bool) override {
    Type *rt = tf->next->toBasetype();
    if (!isPOD(rt))
      return true;
    // > 4 words returned via a hidden sret pointer.
    return isAggregate(rt) && size(rt) > MaxRetRegs * 4;
  }

  bool passByVal(TypeFunction *, Type *t) override {
    t = t->toBasetype();
    // > 6 words passed on the stack as LLVM byval (matches clang's
    // getNaturalAlignIndirect(ByVal=true)); smaller ones are coerced to regs.
    return isPOD(t) && isAggregate(t) && size(t) > MaxArgRegs * 4;
  }

  void rewriteArgument(IrFuncTy &fty, IrFuncTyArg &arg) override {
    TargetABI::rewriteArgument(fty, arg); // non-POD -> indirect by-value
    if (arg.rewrite)
      return;

    Type *ty = arg.type->toBasetype();
    if (!isAggregate(ty))
      return;
    const uint64_t sz = size(ty);
    if (sz == 0)
      return;

    const bool isReturnVal = &arg == fty.ret;
    const uint64_t maxRegBytes = (isReturnVal ? MaxRetRegs : MaxArgRegs) * 4;
    if (sz > maxRegBytes)
      return; // passed indirectly (sret / byval), not coerced

    // Coerce to the register classes clang uses (alignment-driven). Force the
    // rewrite (not applyToIfNotObsolete): an align-4 {i32,i32} is memory-
    // equivalent to [2 x i32] but the backend only flattens the *array* form
    // reliably for under-aligned aggregates, so always emit [N x i32] like
    // esp-clang/rust/gcc.
    const unsigned align = DtoAlignment(ty);
    if (align >= 16) {
      integerRewrite.applyTo(arg); // i128
    } else if (align == 8) {
      compositeToArray64.applyTo(arg); // [N x i64]
    } else {
      compositeToArray32.applyTo(arg); // [N x i32]
    }
  }
};
} // anonymous namespace

TargetABI *getXtensaTargetABI() { return new XtensaTargetABI(); }
