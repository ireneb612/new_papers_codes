#!/bin/bash
# Copyright 2022
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ============================================================================
if [ $# != 3 ]; then
  echo "Usage: bash run_parent_standalone_train_gpu.sh [DEVICE_ID] [DATA_PATH] [VGG_CKPT_PATH]"
  exit 1
fi

python3 train.py \
--device_id  $1 \
--data_path  $2 \
--vgg_features_ckpt $3 > train.log 2>&1 &
echo "train parent net on device $1"
