/*
 * Copyright 2019 Xilinx Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <glog/logging.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <vitis/ai/demo_accuracy.hpp>
#include <vitis/ai/multitask.hpp>
extern int g_last_frame_id;

std::string out_img_path;

namespace vitis {
namespace ai {

static std::vector<std::string> label_map{
    "background", "person", "car", "truck", "bus", "bike", "sign", "light"};

static std::vector<std::string> split(const std::string& s,
                                      const std::string& delim) {
  std::vector<std::string> elems;
  size_t pos = 0;
  size_t len = s.length();
  size_t delim_len = delim.length();
  if (delim_len == 0) return elems;
  while (pos < len) {
    int find_pos = s.find(delim, pos);
    if (find_pos < 0) {
      elems.push_back(s.substr(pos, len - pos));
      break;
    }
    elems.push_back(s.substr(pos, find_pos - pos));
    pos = find_pos + delim_len;
  }
  return elems;
}

struct MultiTaskAcc : public AccThread {
  MultiTaskAcc(std::string output_file)
      : AccThread(), of(output_file, std::ofstream::out) {
    dpu_result.frame_id = -1;
  }

  virtual ~MultiTaskAcc() { of.close(); }

  static std::shared_ptr<MultiTaskAcc> instance(std::string output_file) {
    static std::weak_ptr<MultiTaskAcc> the_instance;
    std::shared_ptr<MultiTaskAcc> ret;
    if (the_instance.expired()) {
      ret = std::make_shared<MultiTaskAcc>(output_file);
      the_instance = ret;
    }
    ret = the_instance.lock();
    assert(ret != nullptr);
    return ret;
  }

  void process_result(DpuResultInfo dpu_result) {
    auto result = (MultiTaskResult*)dpu_result.result_ptr.get();
    auto single_name = split(dpu_result.single_name, ".")[0];
    for (auto& box : result->vehicle) {
      std::string label_name = label_map[box.label];
      float xmin = box.x * dpu_result.w;
      float ymin = box.y * dpu_result.h;
      float xmax = (box.x + box.width) * dpu_result.w;
      float ymax = (box.y + box.height) * dpu_result.h;
      float confidence = box.score;
      of << single_name + ".png"
         << " " << label_name << " " << confidence << " " << xmin << " " << ymin
         << " " << xmax << " " << ymax << std::endl;
    }
    imwrite(out_img_path + "/" + single_name + ".png", result->segmentation);
  }

  virtual int run() override {
    if (g_last_frame_id == int(dpu_result.frame_id)) return -1;
    if (getQueue()->pop(dpu_result, std::chrono::milliseconds(5000))) {
      LOG_IF(INFO, ENV_PARAM(DEBUG_DEMO))
          << "[" << name() << "] process result id :" << dpu_result.frame_id
          << ", dpu queue size " << getQueue()->size();
      process_result(dpu_result);
    }
    return 0;
  }

  DpuResultInfo dpu_result;
  std::ofstream of;
};

}  // namespace ai
}  // namespace vitis

int main(int argc, char* argv[]) {
  out_img_path = argv[3];
  return vitis::ai::main_for_accuracy_demo(
      argc, argv,
      [&] { return vitis::ai::MultiTask8UC1::create("multi_task_acc"); },
      vitis::ai::MultiTaskAcc::instance(argv[2]), 1);
}
