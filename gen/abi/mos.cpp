//===-- mos.cpp -----------------------------------------------------------===//
//
//                         LDC – the LLVM D compiler
//
// This file is distributed under the BSD-style LDC license. See the LICENSE
// file for details.
//
//===----------------------------------------------------------------------===//
//
// The MOS 6502 C ABI (matches llvm-mos / clang lib/CodeGen/Targets/MOS.cpp):
//   * aggregates  > 4 bytes (or non-POD) -> passed/returned indirectly
//   * aggregates <= 4 bytes (POD)        -> passed/returned *directly*, i.e.
//     as a first-class value the MOS backend decomposes into A/X/imaginary
//     registers. This is what C, C++, Zig and (since 2026) Rust all do.
//
// LDC's generic ABI passes every in-memory POD aggregate `byval`, so a small
// struct arrives as a *pointer* the callee dereferences -> garbage across an
// FFI boundary (dlang-mos by-value-struct hole). Dropping `byval` for the
// <=4-byte case is the whole fix: with no rewrite applied the aggregate is
// passed first-class (like Zig's `%Small`), which is ABI-compatible with
// clang's expanded `(i8, i8)`.
//
// The default `returnInArg` is `!isPOD || passByVal`, so overriding only
// `passByVal` also gives the matching return rule (sret only for >4 bytes).
//
//===----------------------------------------------------------------------===//

#include "gen/abi/generic.h"

using namespace dmd;

struct MOSTargetABI : TargetABI {
  bool passByVal(TypeFunction *, Type *t) override {
    // Only aggregates larger than 4 bytes go byval/indirect; smaller POD
    // aggregates are passed directly and decomposed by the backend.
    return DtoIsInMemoryOnly(t) && isPOD(t) && size(t) > 4;
  }
};

// The public getter for abi.cpp.
TargetABI *getMOSTargetABI() { return new MOSTargetABI; }
