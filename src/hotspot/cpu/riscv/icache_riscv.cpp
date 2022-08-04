/*
 * Copyright (c) 1997, 2022, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2020, 2022, Huawei Technologies Co., Ltd. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 *
 */

#include "precompiled.hpp"
#include "asm/macroAssembler.hpp"
#include "runtime/icache.hpp"

#define __ _masm->

// SYSCALL_RISCV_FLUSH_ICACHE is used to flush instruction cache. The "fence.i" instruction
// only work on the current hart, so kernel provides the icache flush syscall to flush icache
// on each hart. You can pass a flag to determine a global or local icache flush.
static void icache_flush(long int start, long int end)
{
  // To make a store to instruction memory visible to all RISC-V
  // harts, the writing hart has to execute a data FENCE before
  // requesting that all remote RISC-V harts execute a FENCE.I.
  // We cannot guarantee that the flush icache syscall follow the
  // spec, so we leave a full data fence here.
  __asm__ volatile ("fence rw, rw" ::: "memory");

  const int SYSCALL_RISCV_FLUSH_ICACHE = 259;
  register long int __a7 asm ("a7") = SYSCALL_RISCV_FLUSH_ICACHE;
  register long int __a0 asm ("a0") = start;
  register long int __a1 asm ("a1") = end;
  // the flush can be applied to either all threads or only the current.
  // 0 means a global icache flush, and the icache flush will be applied
  // to other harts concurrently executing.
  register long int __a2 asm ("a2") = 0;
  __asm__ volatile ("ecall\n\t"
                    : "+r" (__a0)
                    : "r" (__a0), "r" (__a1), "r" (__a2), "r" (__a7)
                    : "memory");
}

static int icache_flush(address addr, int lines, int magic) {
  icache_flush((long int) addr, (long int) (addr + (lines << ICache::log2_line_size)));
  return magic;
}

void ICacheStubGenerator::generate_icache_flush(ICache::flush_icache_stub_t* flush_icache_stub) {
  address start = (address)icache_flush;
  *flush_icache_stub = (ICache::flush_icache_stub_t)start;

  // ICache::invalidate_range() contains explicit condition that the first
  // call is invoked on the generated icache flush stub code range.
  ICache::invalidate_range(start, 0);

  {
    StubCodeMark mark(this, "ICache", "fake_stub_for_inlined_icache_flush");
    __ ret();
  }
}

#undef __
