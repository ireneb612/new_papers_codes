/**
 * Copyright 2022
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 * http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <sys/time.h>
#include <gflags/gflags.h>
#include <dirent.h>
#include <iostream>
#include <string>
#include <algorithm>
#include <iosfwd>
#include <vector>
#include <fstream>
#include <sstream>

#include "include/api/model.h"
#include "include/api/context.h"
#include "include/api/types.h"
#include "include/api/serialization.h"
#include "include/dataset/execute.h"
#include "include/dataset/vision.h"
#include "inc/utils.h"

using mindspore::Context;
using mindspore::Serialization;
using mindspore::Model;
using mindspore::Status;
using mindspore::MSTensor;
using mindspore::dataset::Execute;
using mindspore::ModelType;
using mindspore::GraphCell;
using mindspore::kSuccess;

DEFINE_string(mindir_path, "", "mindir path");
DEFINE_string(input0_path, ".", "input0 path");
DEFINE_int32(device_id, 0, "device id");
DEFINE_string(precision_mode, "allow_fp32_to_fp16", "precision mode");


int load_model(Model *model, std::vector<MSTensor> *model_inputs, std::string mindir_path, int device_id) {
  if (RealPath(mindir_path).empty()) {
    std::cout << "Invalid mindir" << std::endl;
    return 1;
  }

  auto context = std::make_shared<Context>();
  auto ascend310 = std::make_shared<mindspore::Ascend310DeviceInfo>();
  ascend310->SetDeviceID(device_id);
  context->MutableDeviceInfo().push_back(ascend310);
  ascend310->SetOpSelectImplMode("high_precision");
  ascend310->SetPrecisionMode("allow_fp32_to_fp16");
  mindspore::Graph graph;
  Serialization::Load(mindir_path, ModelType::kMindIR, &graph);

  Status ret = model->Build(GraphCell(graph), context);
  if (ret != kSuccess) {
    std::cout << "ERROR: Build failed." << std::endl;
    return 1;
  }

  *model_inputs = model->GetInputs();
  if (model_inputs->empty()) {
    std::cout << "Invalid model, inputs is empty." << std::endl;
    return 1;
  } else {
    std::cout << "valid model, inputs is not empty." << std::endl;
  }
  return 0;
}

int main(int argc, char **argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  namespace ms = mindspore;
  namespace ds = mindspore::dataset;

  auto context = std::make_shared<ms::Context>();
  auto ascend310_info = std::make_shared<ms::Ascend310DeviceInfo>();
  ascend310_info->SetDeviceID(FLAGS_device_id);
  context->MutableDeviceInfo().push_back(ascend310_info);

  ms::Graph graph;
  ms::Status ret = ms::Serialization::Load(FLAGS_mindir_path, ms::ModelType::kMindIR, &graph);
  ms::Model model;
  std::cout << ret << std::endl;
  Status rets = model.Build(ms::GraphCell(graph), context);
  if (rets != kSuccess) {
    std::cout << "ERROR: Build failed." << std::endl;
    return 1;
  }

  std::vector<MSTensor> model_inputs = model.GetInputs();
  if (model_inputs.empty()) {
    std::cout << "Invalid model, inputs is empty." << std::endl;
    return 1;
  }

  std::string input_path1 = FLAGS_input0_path;
  std::string input_path2 = input_path1;
  input_path2.replace(21, 1, "1");
  auto input0_files = GetAllFiles(input_path1);
  auto input1_files = GetAllFiles(input_path2);

  std::cout << "input_path1:" << input_path1 << std::endl;
  std::cout << "input_path2:" << input_path2 << std::endl;

  if (input0_files.empty() || input1_files.empty()) {
    std::cout << "ERROR: input data empty." << std::endl;
    return 1;
  }

  std::map<double, double> costTime_map;
  size_t size = input0_files.size();

  for (size_t i = 0; i < size; ++i) {
    struct timeval start = {0};
    struct timeval end = {0};
    double startTimeMs;
    double endTimeMs;
    std::vector<MSTensor> inputs;
    std::vector<MSTensor> outputs;
    std::cout << "Start predict input files:" << input0_files[i] << std::endl;

    auto input0 = ReadFileToTensor(input0_files[i]);
    auto input1 = ReadFileToTensor(input1_files[i]);

    inputs.emplace_back(model_inputs[0].Name(), model_inputs[0].DataType(), model_inputs[0].Shape(),
                         input0.Data().get(), input0.DataSize());
    inputs.emplace_back(model_inputs[1].Name(), model_inputs[1].DataType(), model_inputs[1].Shape(),
                         input1.Data().get(), input1.DataSize());
    gettimeofday(&start, nullptr);
    ret = model.Predict(inputs, &outputs);
    gettimeofday(&end, nullptr);
    if (ret != kSuccess) {
      std::cout << "Predict " << input0_files[i] << " failed." << std::endl;
      return 1;
    }
    startTimeMs = (1.0 * start.tv_sec * 1000000 + start.tv_usec) / 1000;
    endTimeMs = (1.0 * end.tv_sec * 1000000 + end.tv_usec) / 1000;
    costTime_map.insert(std::pair<double, double>(startTimeMs, endTimeMs));
    int rst = WriteResult(input0_files[i], outputs);
    if (rst != 0) {
        std::cout << "write result failed." << std::endl;
        return rst;
    }
  }
  double average = 0.0;
  int inferCount = 0;

  for (auto iter = costTime_map.begin(); iter != costTime_map.end(); iter++) {
    double diff = 0.0;
    diff = iter->second - iter->first;
    average += diff;
    inferCount++;
  }
  average = average / inferCount;
  std::stringstream timeCost;
  timeCost << "NN inference cost average time: "<< average << " ms of infer_count " << inferCount << std::endl;
  std::cout << "NN inference cost average time: "<< average << "ms of infer_count " << inferCount << std::endl;
  std::string fileName = "./time_Result" + std::string("/test_perform_static.txt");
  std::ofstream fileStream(fileName.c_str(), std::ios::trunc);
  fileStream << timeCost.str();
  fileStream.close();
  costTime_map.clear();

  return 0;
}
