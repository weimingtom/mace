// Copyright 2019 The MACE Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <arm_neon.h>
#include <algorithm>
#include <utility>

#include "mace/ops/arm/base/gemm.h"
#include "mace/port/env.h"

namespace mace {
namespace ops {
namespace arm {

template<>
void Gemm<float>::ComputeBlock(const float *packed_lhs_data,
                               const float *packed_rhs_data,
                               const index_t depth_padded,
                               float *packed_output_data) {
  /* Ref:
  for (index_t r = 0; r < block_size; ++r) {
    for (index_t c = 0; c < block_size; ++c) {
      float sum = 0;
      for (index_t d = 0; d < depth; ++d) {
        // (r, d) * (d, c)
        sum += packed_lhs_data[d * r_block_size + r]
            * packed_rhs_data[d * c_block_size + c];
      }
      packed_output_data[r * c_block_size + c] = sum;
    }
  }
  */
  const float *lhs_ptr = packed_lhs_data;
  const float *rhs_ptr = packed_rhs_data;

  const index_t depth_block_count = depth_padded / 4;

#ifdef __aarch64__
  // Register layout: (8x4) x (4,8)
  //
  //                               +--------+--------+
  //                               | v8 ... | v9 ... |
  //                       Rhs     +--------+--------+
  //                               | v10... | v11... |
  //                               +--------+--------+
  //                               | v12... | v13... |
  //                               +--------+--------+
  //                               | v14... | v15... |
  //                               +--------+--------+
  //
  //          Lhs
  //
  //  +----+----+----+----+  -  -  +--------+--------+
  //  | v0 | v2 | v4 | v6 |        | v16... | v17... |
  //  | .  |    |    |    |        | v18... | v19... |
  //  | .  |    |    |    |        | v20... | v21... |
  //  | .  |    |    |    |        | v22... | v23... |
  //  +----+----|----+----+        +--------+--------+
  //  | v1 | v3 | v5 | v7 |        | v24... | v25... |
  //  | .  |    |    |    |        | v26... | v27... |
  //  | .  |    |    |    |        | v28... | v29... |
  //  | .  |    |    |    |        | v30... | v31... |
  //  +----+----|----+----+        +--------+--------+
  //
  //                                    Accumulator
  //

  if (depth_block_count > 0) {
    index_t r_depth_block_count = depth_block_count;
    // just make compiler happy
    MACE_UNUSED(r_depth_block_count);

    asm volatile(
        "dup v16.4s, wzr \n"
        "dup v17.4s, wzr \n"
        "dup v18.4s, wzr \n"
        "dup v19.4s, wzr \n"
        "dup v20.4s, wzr \n"
        "dup v21.4s, wzr \n"
        "dup v22.4s, wzr \n"
        "dup v23.4s, wzr \n"
        "dup v24.4s, wzr \n"
        "dup v25.4s, wzr \n"
        "dup v26.4s, wzr \n"
        "dup v27.4s, wzr \n"
        "dup v28.4s, wzr \n"
        "dup v29.4s, wzr \n"
        "dup v30.4s, wzr \n"
        "dup v31.4s, wzr \n"

        // prelogue
        "ld1 {v0.4s}, [%[lhs_ptr]], #16 \n"
        "ld1 {v1.4s}, [%[lhs_ptr]], #16 \n"
        "ld1 {v2.4s}, [%[lhs_ptr]], #16 \n"
        "ld1 {v3.4s}, [%[lhs_ptr]], #16 \n"
        "ld1 {v4.4s}, [%[lhs_ptr]], #16 \n"
        "ld1 {v5.4s}, [%[lhs_ptr]], #16 \n"
        "ld1 {v6.4s}, [%[lhs_ptr]], #16 \n"
        "ld1 {v7.4s}, [%[lhs_ptr]], #16 \n"

        "ld1 {v8.4s}, [%[rhs_ptr]], #16 \n"
        "ld1 {v9.4s}, [%[rhs_ptr]], #16 \n"
        "ld1 {v10.4s}, [%[rhs_ptr]], #16 \n"
        "ld1 {v11.4s}, [%[rhs_ptr]], #16 \n"
        "ld1 {v12.4s}, [%[rhs_ptr]], #16 \n"
        "ld1 {v13.4s}, [%[rhs_ptr]], #16 \n"
        "ld1 {v14.4s}, [%[rhs_ptr]], #16 \n"
        "ld1 {v15.4s}, [%[rhs_ptr]], #16 \n"

        "subs %[r_depth_block_count], %[r_depth_block_count], #1 \n"
        "beq 1f\n"

        "0: \n"
        "fmla v16.4s, v8.4s, v0.s[0] \n"
        "fmla v17.4s, v9.4s, v0.s[0] \n"
        "fmla v18.4s, v8.4s, v0.s[1] \n"
        "fmla v19.4s, v9.4s, v0.s[1] \n"
        "fmla v20.4s, v8.4s, v0.s[2] \n"
        "fmla v21.4s, v9.4s, v0.s[2] \n"
        "fmla v22.4s, v8.4s, v0.s[3] \n"
        "fmla v23.4s, v9.4s, v0.s[3] \n"

        "ld1 {v0.4s}, [%[lhs_ptr]], #16 \n"

        "fmla v24.4s, v8.4s, v1.s[0] \n"
        "fmla v25.4s, v9.4s, v1.s[0] \n"
        "fmla v26.4s, v8.4s, v1.s[1] \n"
        "fmla v27.4s, v9.4s, v1.s[1] \n"
        "fmla v28.4s, v8.4s, v1.s[2] \n"
        "fmla v29.4s, v9.4s, v1.s[2] \n"
        "fmla v30.4s, v8.4s, v1.s[3] \n"
        "fmla v31.4s, v9.4s, v1.s[3] \n"

        "ld1 {v1.4s}, [%[lhs_ptr]], #16 \n"
        "ld1 {v8.4s}, [%[rhs_ptr]], #16 \n"
        "ld1 {v9.4s}, [%[rhs_ptr]], #16 \n"

        "fmla v16.4s, v10.4s, v2.s[0] \n"
        "fmla v17.4s, v11.4s, v2.s[0] \n"
        "fmla v18.4s, v10.4s, v2.s[1] \n"
        "fmla v19.4s, v11.4s, v2.s[1] \n"
        "fmla v20.4s, v10.4s, v2.s[2] \n"
        "fmla v21.4s, v11.4s, v2.s[2] \n"
        "fmla v22.4s, v10.4s, v2.s[3] \n"
        "fmla v23.4s, v11.4s, v2.s[3] \n"

        "ld1 {v2.4s}, [%[lhs_ptr]], #16 \n"

        "fmla v24.4s, v10.4s, v3.s[0] \n"
        "fmla v25.4s, v11.4s, v3.s[0] \n"
        "fmla v26.4s, v10.4s, v3.s[1] \n"
        "fmla v27.4s, v11.4s, v3.s[1] \n"
        "fmla v28.4s, v10.4s, v3.s[2] \n"
        "fmla v29.4s, v11.4s, v3.s[2] \n"
        "fmla v30.4s, v10.4s, v3.s[3] \n"
        "fmla v31.4s, v11.4s, v3.s[3] \n"

        "ld1 {v3.4s}, [%[lhs_ptr]], #16 \n"
        "ld1 {v10.4s}, [%[rhs_ptr]], #16 \n"
        "ld1 {v11.4s}, [%[rhs_ptr]], #16 \n"

        "fmla v16.4s, v12.4s, v4.s[0] \n"
        "fmla v17.4s, v13.4s, v4.s[0] \n"
        "fmla v18.4s, v12.4s, v4.s[1] \n"
        "fmla v19.4s, v13.4s, v4.s[1] \n"
        "fmla v20.4s, v12.4s, v4.s[2] \n"
        "fmla v21.4s, v13.4s, v4.s[2] \n"
        "fmla v22.4s, v12.4s, v4.s[3] \n"
        "fmla v23.4s, v13.4s, v4.s[3] \n"

        "ld1 {v4.4s}, [%[lhs_ptr]], #16 \n"

        "fmla v24.4s, v12.4s, v5.s[0] \n"
        "fmla v25.4s, v13.4s, v5.s[0] \n"
        "fmla v26.4s, v12.4s, v5.s[1] \n"
        "fmla v27.4s, v13.4s, v5.s[1] \n"
        "fmla v28.4s, v12.4s, v5.s[2] \n"
        "fmla v29.4s, v13.4s, v5.s[2] \n"
        "fmla v30.4s, v12.4s, v5.s[3] \n"
        "fmla v31.4s, v13.4s, v5.s[3] \n"

        "ld1 {v5.4s}, [%[lhs_ptr]], #16 \n"
        "ld1 {v12.4s}, [%[rhs_ptr]], #16 \n"
        "ld1 {v13.4s}, [%[rhs_ptr]], #16 \n"

        "fmla v16.4s, v14.4s, v6.s[0] \n"
        "fmla v17.4s, v15.4s, v6.s[0] \n"
        "fmla v18.4s, v14.4s, v6.s[1] \n"
        "fmla v19.4s, v15.4s, v6.s[1] \n"
        "fmla v20.4s, v14.4s, v6.s[2] \n"
        "fmla v21.4s, v15.4s, v6.s[2] \n"
        "fmla v22.4s, v14.4s, v6.s[3] \n"
        "fmla v23.4s, v15.4s, v6.s[3] \n"

        "ld1 {v6.4s}, [%[lhs_ptr]], #16 \n"

        "subs %[r_depth_block_count], %[r_depth_block_count], #1 \n"

        "fmla v24.4s, v14.4s, v7.s[0] \n"
        "fmla v25.4s, v15.4s, v7.s[0] \n"
        "fmla v26.4s, v14.4s, v7.s[1] \n"
        "fmla v27.4s, v15.4s, v7.s[1] \n"
        "fmla v28.4s, v14.4s, v7.s[2] \n"
        "fmla v29.4s, v15.4s, v7.s[2] \n"
        "fmla v30.4s, v14.4s, v7.s[3] \n"
        "fmla v31.4s, v15.4s, v7.s[3] \n"

        "ld1 {v7.4s}, [%[lhs_ptr]], #16 \n"
        "ld1 {v14.4s}, [%[rhs_ptr]], #16 \n"
        "ld1 {v15.4s}, [%[rhs_ptr]], #16 \n"

        "bne 0b \n"

        // prologue
        "1:\n"
        "fmla v16.4s, v8.4s, v0.s[0] \n"
        "fmla v17.4s, v9.4s, v0.s[0] \n"
        "fmla v18.4s, v8.4s, v0.s[1] \n"
        "fmla v19.4s, v9.4s, v0.s[1] \n"
        "fmla v20.4s, v8.4s, v0.s[2] \n"
        "fmla v21.4s, v9.4s, v0.s[2] \n"
        "fmla v22.4s, v8.4s, v0.s[3] \n"
        "fmla v23.4s, v9.4s, v0.s[3] \n"

        "fmla v24.4s, v8.4s, v1.s[0] \n"
        "fmla v25.4s, v9.4s, v1.s[0] \n"
        "fmla v26.4s, v8.4s, v1.s[1] \n"
        "fmla v27.4s, v9.4s, v1.s[1] \n"
        "fmla v28.4s, v8.4s, v1.s[2] \n"
        "fmla v29.4s, v9.4s, v1.s[2] \n"
        "fmla v30.4s, v8.4s, v1.s[3] \n"
        "fmla v31.4s, v9.4s, v1.s[3] \n"

        "fmla v16.4s, v10.4s, v2.s[0] \n"
        "fmla v17.4s, v11.4s, v2.s[0] \n"
        "fmla v18.4s, v10.4s, v2.s[1] \n"
        "fmla v19.4s, v11.4s, v2.s[1] \n"
        "fmla v20.4s, v10.4s, v2.s[2] \n"
        "fmla v21.4s, v11.4s, v2.s[2] \n"
        "fmla v22.4s, v10.4s, v2.s[3] \n"
        "fmla v23.4s, v11.4s, v2.s[3] \n"

        "fmla v24.4s, v10.4s, v3.s[0] \n"
        "fmla v25.4s, v11.4s, v3.s[0] \n"
        "fmla v26.4s, v10.4s, v3.s[1] \n"
        "fmla v27.4s, v11.4s, v3.s[1] \n"
        "fmla v28.4s, v10.4s, v3.s[2] \n"
        "fmla v29.4s, v11.4s, v3.s[2] \n"
        "fmla v30.4s, v10.4s, v3.s[3] \n"
        "fmla v31.4s, v11.4s, v3.s[3] \n"

        "fmla v16.4s, v12.4s, v4.s[0] \n"
        "fmla v17.4s, v13.4s, v4.s[0] \n"
        "fmla v18.4s, v12.4s, v4.s[1] \n"
        "fmla v19.4s, v13.4s, v4.s[1] \n"
        "fmla v20.4s, v12.4s, v4.s[2] \n"
        "fmla v21.4s, v13.4s, v4.s[2] \n"
        "fmla v22.4s, v12.4s, v4.s[3] \n"
        "fmla v23.4s, v13.4s, v4.s[3] \n"

        "fmla v24.4s, v12.4s, v5.s[0] \n"
        "fmla v25.4s, v13.4s, v5.s[0] \n"
        "fmla v26.4s, v12.4s, v5.s[1] \n"
        "fmla v27.4s, v13.4s, v5.s[1] \n"
        "fmla v28.4s, v12.4s, v5.s[2] \n"
        "fmla v29.4s, v13.4s, v5.s[2] \n"
        "fmla v30.4s, v12.4s, v5.s[3] \n"
        "fmla v31.4s, v13.4s, v5.s[3] \n"

        "fmla v16.4s, v14.4s, v6.s[0] \n"
        "fmla v17.4s, v15.4s, v6.s[0] \n"
        "fmla v18.4s, v14.4s, v6.s[1] \n"
        "fmla v19.4s, v15.4s, v6.s[1] \n"
        "fmla v20.4s, v14.4s, v6.s[2] \n"
        "fmla v21.4s, v15.4s, v6.s[2] \n"
        "fmla v22.4s, v14.4s, v6.s[3] \n"
        "fmla v23.4s, v15.4s, v6.s[3] \n"

        "fmla v24.4s, v14.4s, v7.s[0] \n"
        "fmla v25.4s, v15.4s, v7.s[0] \n"
        "fmla v26.4s, v14.4s, v7.s[1] \n"
        "fmla v27.4s, v15.4s, v7.s[1] \n"
        "fmla v28.4s, v14.4s, v7.s[2] \n"
        "fmla v29.4s, v15.4s, v7.s[2] \n"
        "fmla v30.4s, v14.4s, v7.s[3] \n"
        "fmla v31.4s, v15.4s, v7.s[3] \n"

        "st1 {v16.4s}, [%[packed_output_data]], #16 \n"
        "st1 {v17.4s}, [%[packed_output_data]], #16 \n"
        "st1 {v18.4s}, [%[packed_output_data]], #16 \n"
        "st1 {v19.4s}, [%[packed_output_data]], #16 \n"
        "st1 {v20.4s}, [%[packed_output_data]], #16 \n"
        "st1 {v21.4s}, [%[packed_output_data]], #16 \n"
        "st1 {v22.4s}, [%[packed_output_data]], #16 \n"
        "st1 {v23.4s}, [%[packed_output_data]], #16 \n"
        "st1 {v24.4s}, [%[packed_output_data]], #16 \n"
        "st1 {v25.4s}, [%[packed_output_data]], #16 \n"
        "st1 {v26.4s}, [%[packed_output_data]], #16 \n"
        "st1 {v27.4s}, [%[packed_output_data]], #16 \n"
        "st1 {v28.4s}, [%[packed_output_data]], #16 \n"
        "st1 {v29.4s}, [%[packed_output_data]], #16 \n"
        "st1 {v30.4s}, [%[packed_output_data]], #16 \n"
        "st1 {v31.4s}, [%[packed_output_data]], #16 \n"
    :  // outputs
    [lhs_ptr] "+r"(lhs_ptr),
    [rhs_ptr] "+r"(rhs_ptr),
    [packed_output_data] "+r"(packed_output_data),
    [r_depth_block_count] "+r"(r_depth_block_count)
    :  // inputs
    :  // clabbers
    "cc", "memory",
        "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7",
        "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15",
        "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23",
        "v24", "v25", "v26", "v27", "v28", "v29", "v30", "v31");
  }
#else  // armeabi-v7a
  // Register layout: (4x4) x (4,8)
  //
  //                               +--------+--------+
  //                               | q4 ... | q5 ... |
  //                       Rhs     +--------+--------+
  //                               | q6 ... | q7 ... |
  //                               +--------+--------+
  //                               | q4 ... | q5 ... |
  //                               +--------+--------+
  //                               | q6 ... | q7 ... |
  //                               +--------+--------+
  //
  //          Lhs
  //
  //  +----+----+----+----+  -  -  +--------+--------+
  //  | q0 | q1 | q2 | q3 |        | q8...  | q9...  |
  //  | .  |    |    |    |        | q10... | q11... |
  //  | .  |    |    |    |        | q12... | q13... |
  //  | .  |    |    |    |        | q14... | q15... |
  //  +----+----+----+----+        +--------+--------+
  //
  //                                    Accumulator
  //

  if (depth_block_count > 0) {
    index_t r_depth_block_count = depth_block_count;
    // just make compiler happy
    MACE_UNUSED(r_depth_block_count);

    asm volatile(
    "mov r0, #0\n"
    "vdup.f32 q8, r0 \n"
    "vdup.f32 q9, r0 \n"
    "vdup.f32 q10, r0 \n"
    "vdup.f32 q11, r0 \n"
    "vdup.f32 q12, r0 \n"
    "vdup.f32 q13, r0 \n"
    "vdup.f32 q14, r0 \n"
    "vdup.f32 q15, r0 \n"

    // prelogue
    "vld1.f32 {d0-d1}, [%[lhs_ptr]]! \n"
    "vld1.f32 {d2-d3}, [%[lhs_ptr]]! \n"
    "vld1.f32 {d4-d5}, [%[lhs_ptr]]! \n"
    "vld1.f32 {d6-d7}, [%[lhs_ptr]]! \n"

    "vld1.f32 {d8-d9}, [%[rhs_ptr]]! \n"
    "vld1.f32 {d10-d11}, [%[rhs_ptr]]! \n"
    "vld1.f32 {d12-d13}, [%[rhs_ptr]]! \n"
    "vld1.f32 {d14-d15}, [%[rhs_ptr]]! \n"

    "subs %[r_depth_block_count], %[r_depth_block_count], #1 \n"
    "beq 1f\n"

    "0: \n"

    "vmla.f32 q8, q4, d0[0] \n"
    "vmla.f32 q9, q5, d0[0] \n"
    "vmla.f32 q10, q4, d0[1] \n"
    "vmla.f32 q11, q5, d0[1] \n"
    "vmla.f32 q12, q4, d1[0] \n"
    "vmla.f32 q13, q5, d1[0] \n"
    "vmla.f32 q14, q4, d1[1] \n"
    "vmla.f32 q15, q5, d1[1] \n"

    "vld1.f32 {d0-d1}, [%[lhs_ptr]]! \n"
    "vld1.f32 {d8-d9}, [%[rhs_ptr]]! \n"
    "vld1.f32 {d10-d11}, [%[rhs_ptr]]! \n"

    "vmla.f32 q8, q6, d2[0] \n"
    "vmla.f32 q9, q7, d2[0] \n"
    "vmla.f32 q10, q6, d2[1] \n"
    "vmla.f32 q11, q7, d2[1] \n"
    "vmla.f32 q12, q6, d3[0] \n"
    "vmla.f32 q13, q7, d3[0] \n"
    "vmla.f32 q14, q6, d3[1] \n"
    "vmla.f32 q15, q7, d3[1] \n"

    "vld1.f32 {d2-d3}, [%[lhs_ptr]]! \n"
    "vld1.f32 {d12-d13}, [%[rhs_ptr]]! \n"
    "vld1.f32 {d14-d15}, [%[rhs_ptr]]! \n"

    "vmla.f32 q8, q4, d4[0] \n"
    "vmla.f32 q9, q5, d4[0] \n"
    "vmla.f32 q10, q4, d4[1] \n"
    "vmla.f32 q11, q5, d4[1] \n"
    "vmla.f32 q12, q4, d5[0] \n"
    "vmla.f32 q13, q5, d5[0] \n"
    "vmla.f32 q14, q4, d5[1] \n"
    "vmla.f32 q15, q5, d5[1] \n"

    "vld1.f32 {d4-d5}, [%[lhs_ptr]]! \n"
    "vld1.f32 {d8-d9}, [%[rhs_ptr]]! \n"
    "vld1.f32 {d10-d11}, [%[rhs_ptr]]! \n"

    "subs %[r_depth_block_count], %[r_depth_block_count], #1 \n"

    "vmla.f32 q8, q6, d6[0] \n"
    "vmla.f32 q9, q7, d6[0] \n"
    "vmla.f32 q10, q6, d6[1] \n"
    "vmla.f32 q11, q7, d6[1] \n"
    "vmla.f32 q12, q6, d7[0] \n"
    "vmla.f32 q13, q7, d7[0] \n"
    "vmla.f32 q14, q6, d7[1] \n"
    "vmla.f32 q15, q7, d7[1] \n"

    "vld1.f32 {d6-d7}, [%[lhs_ptr]]! \n"
    "vld1.f32 {d12-d13}, [%[rhs_ptr]]! \n"
    "vld1.f32 {d14-d15}, [%[rhs_ptr]]! \n"

    "bne 0b \n"

    // prologue
    "1:\n"
    "vmla.f32 q8, q4, d0[0] \n"
    "vmla.f32 q9, q5, d0[0] \n"
    "vmla.f32 q10, q4, d0[1] \n"
    "vmla.f32 q11, q5, d0[1] \n"
    "vmla.f32 q12, q4, d1[0] \n"
    "vmla.f32 q13, q5, d1[0] \n"
    "vmla.f32 q14, q4, d1[1] \n"
    "vmla.f32 q15, q5, d1[1] \n"

    "vld1.f32 {d8-d9}, [%[rhs_ptr]]! \n"
    "vld1.f32 {d10-d11}, [%[rhs_ptr]]! \n"

    "vmla.f32 q8, q6, d2[0] \n"
    "vmla.f32 q9, q7, d2[0] \n"
    "vmla.f32 q10, q6, d2[1] \n"
    "vmla.f32 q11, q7, d2[1] \n"
    "vmla.f32 q12, q6, d3[0] \n"
    "vmla.f32 q13, q7, d3[0] \n"
    "vmla.f32 q14, q6, d3[1] \n"
    "vmla.f32 q15, q7, d3[1] \n"

    "vld1.f32 {d12-d13}, [%[rhs_ptr]]! \n"
    "vld1.f32 {d14-d15}, [%[rhs_ptr]]! \n"

    "vmla.f32 q8, q4, d4[0] \n"
    "vmla.f32 q9, q5, d4[0] \n"
    "vmla.f32 q10, q4, d4[1] \n"
    "vmla.f32 q11, q5, d4[1] \n"
    "vmla.f32 q12, q4, d5[0] \n"
    "vmla.f32 q13, q5, d5[0] \n"
    "vmla.f32 q14, q4, d5[1] \n"
    "vmla.f32 q15, q5, d5[1] \n"

    "vmla.f32 q8, q6, d6[0] \n"
    "vmla.f32 q9, q7, d6[0] \n"
    "vmla.f32 q10, q6, d6[1] \n"
    "vmla.f32 q11, q7, d6[1] \n"
    "vmla.f32 q12, q6, d7[0] \n"
    "vmla.f32 q13, q7, d7[0] \n"
    "vmla.f32 q14, q6, d7[1] \n"
    "vmla.f32 q15, q7, d7[1] \n"

    "vst1.f32 {d16-d17}, [%[packed_output_data]]! \n"
    "vst1.f32 {d18-d19}, [%[packed_output_data]]! \n"
    "vst1.f32 {d20-d21}, [%[packed_output_data]]! \n"
    "vst1.f32 {d22-d23}, [%[packed_output_data]]! \n"
    "vst1.f32 {d24-d25}, [%[packed_output_data]]! \n"
    "vst1.f32 {d26-d27}, [%[packed_output_data]]! \n"
    "vst1.f32 {d28-d29}, [%[packed_output_data]]! \n"
    "vst1.f32 {d30-d31}, [%[packed_output_data]]! \n"
    :  // outputs
    [lhs_ptr] "+r"(lhs_ptr),
    [rhs_ptr] "+r"(rhs_ptr),
    [packed_output_data] "+r"(packed_output_data),
    [r_depth_block_count] "+r"(r_depth_block_count)
    :  // inputs
    :  // clabbers
    "cc", "memory", "r0",
        "q0", "q1", "q2", "q3", "q4", "q5", "q6", "q7",
        "q8", "q9", "q10", "q11", "q12", "q13", "q14", "q15");
  }
#endif
}

}  // namespace arm
}  // namespace ops
}  // namespace mace
