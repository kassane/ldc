// REQUIRES: target_MOS602

// RUN: %ldc -mtriple=mos -betterC -output-ll -of=%t.ll %s && FileCheck %s < %t.ll

version (MOS6502) {} else static assert(0);

// CHECK: @_D7mos650213definedGlobali = thread_local global i32 123
int definedGlobal = 123;
// CHECK: @_D7mos650214declaredGlobali = external thread_local global i32
extern int declaredGlobal;

// source_filename = "tests/codegen/mos6502.d"
// target datalayout = "e-m:e-p:16:8-p1:8:8-i16:8-i32:8-i64:8-f32:8-f64:8-a:8-Fi8-n8"