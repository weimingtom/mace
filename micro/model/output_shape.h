// Copyright 2020 The MACE Authors. All Rights Reserved.
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

#ifndef MICRO_MODEL_OUTPUT_SHAPE_H_
#define MICRO_MODEL_OUTPUT_SHAPE_H_

#include "micro/base/serialize.h"

namespace micro {
namespace model {

class OutputShape : public Serialize {
 public:
  MACE_DEFINE_HARD_CODE_MAGIC(OutputShape)

  MACE_DECLARE_ARRAY_FUNC(int32_t, dim);

  const int32_t *dim() const;

  int32_t *mutable_dim();

 private:
  SerialArray<SerialInt32> dims_;
};

}  // namespace model
}  // namespace micro

#endif  // MICRO_MODEL_OUTPUT_SHAPE_H_
