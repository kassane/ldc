module mos6502;

// REQUIRES: target_MOS

// RUN: %ldc -mtriple=mos -betterC -output-ll -of=%t.ll %s && FileCheck %s < %t.ll

version (MOS) {} else static assert(0);

// CHECK directives are matched in IR-emission order: datalayout, then aggregate
// type definitions, then globals.

// 16-bit data layout (16-bit pointers, all types byte-aligned).
// CHECK: target datalayout = "e-m:e-p:16:8-p1:8:8-i16:8-i32:8-i64:8-f32:8-f64:8-a:8-Fi8-n8"

// size_t tracks the 16-bit pointer width (dlang-mos-hello-world#1):
// the trailing length field must be i16, not i32.
// CHECK: %mos6502.Slice = type { ptr, i16 }
struct Slice { void* ptr; size_t length; }
__gshared Slice gSlice;

// D `int` stays 32-bit per spec, even on an 8-bit target.
// CHECK: @_D7mos650213definedGlobali = thread_local global i32 123
int definedGlobal = 123;
// CHECK: @_D7mos650214declaredGlobali = external thread_local global i32
extern int declaredGlobal;

// Reference declaredGlobal so its external definition is emitted.
extern (C) int readDeclared() @nogc nothrow { return declaredGlobal; }
