#
# Copyright 2019 Xilinx Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

sudo cp /workspace/xclbin/u25/dpu.xclbin /usr/lib/dpu.xclbin

mkdir -p /workspace/app/imdb_sentiment_detection/model
mkdir -p /workspace/app/customer_satisfaction/model
mkdir -p /workspace/app/open_information_extraction/model
cp /workspace/models/u25/imdb_sentiment_detection/*  /workspace/app/imdb_sentiment_detection/model 
cp /workspace/models/u25/customer_satisfaction/*  /workspace/app/customer_satisfaction/model 
cp /workspace/models/u25/open_information_extraction/*  /workspace/app/open_information_extraction/model 
mv /workspace/app/open_information_extraction/model/weights.th  /workspace/app/open_information_extraction/weights

echo "set HW and models successfully"
