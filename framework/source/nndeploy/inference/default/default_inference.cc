
#include "nndeploy/inference/default/default_inference.h"

namespace nndeploy {
namespace inference {

TypeInferenceRegister<TypeInferenceCreator<DefaultInference>>
    g_default_inference_register(base::kInferenceTypeDefault);

DefaultInference::DefaultInference(base::InferenceType type) : Inference(type) {
  interpret_ = nullptr;
  net_ = nullptr;
}
DefaultInference::~DefaultInference() {}

// 默认为init之前inference_param已经初始化，利用inference_param初始化DEFAULT的instance和其余各项参数
base::Status DefaultInference::init() {
  base::Status status = base::kStatusCodeOk;

  DefaultInferenceParam *default_inference_param =
      dynamic_cast<DefaultInferenceParam *>(inference_param_.get());
  is_share_context_ = true;
  if (!is_external_stream_ && stream_ == nullptr) {
    stream_ = device::createStream(default_inference_param->device_type_);
  }

  interpret_ = ir::createInterpret(default_inference_param->model_type_);
  if (interpret_ == nullptr) {
    NNDEPLOY_LOGE("ir::createInterpret failed!\n");
    return base::kStatusCodeErrorInferenceDefault;
  }
  std::vector<ir::ValueDesc> value_descs;
  if (!default_inference_param->is_dynamic_shape_ &&
      !default_inference_param->opt_shape_.empty()) {
    for (auto iter : default_inference_param->opt_shape_) {
      ir::ValueDesc value_desc(iter.first, base::dataTypeOf<float>(),
                               iter.second);
      value_descs.push_back(value_desc);
    }
  }
  status =
      interpret_->interpret(default_inference_param->model_value_, value_descs);
  if (status != base::kStatusCodeOk) {
    NNDEPLOY_LOGE("DEFAULT init failed!\n");
    return base::kStatusCodeErrorInferenceDefault;
  }
  ir::ModelDesc *md = interpret_->getModelDesc();
  if (md == nullptr) {
    NNDEPLOY_LOGE("get model desc failed\n");
    return -1;
  }

  net_ = new net::Net();
  if (net_ == nullptr) {
    NNDEPLOY_LOGE("new net::Net() failed!\n");
    return base::kStatusCodeErrorInferenceDefault;
  }
  status = net_->setModelDesc(md);
  if (status != base::kStatusCodeOk) {
    NNDEPLOY_LOGE("net_->setModelDesc(md) failed!\n");
    return base::kStatusCodeErrorInferenceDefault;
  }
  status = net_->setDeviceType(default_inference_param->device_type_);
  if (status != base::kStatusCodeOk) {
    NNDEPLOY_LOGE("net_->setDeviceType failed!\n");
    return base::kStatusCodeErrorInferenceDefault;
  }
  net_->setParallelType(default_inference_param->parallel_type_);
  net_->setWorkers(default_inference_param->worker_num_,
                   default_inference_param->device_types_);
  net_->setStream(stream_);
  status = net_->setDynamicShape(default_inference_param->is_dynamic_shape_,
                                 default_inference_param->min_shape_,
                                 default_inference_param->opt_shape_,
                                 default_inference_param->max_shape_);
  if (status != base::kStatusCodeOk) {
    NNDEPLOY_LOGE("net_->setDynamicShape failed!\n");
    return base::kStatusCodeErrorInferenceDefault;
  }
  status = net_->setTensorPoolType(default_inference_param->tensor_pool_type_);
  // NNDEPLOY_LOGE("tensor_pool_type_ = %d\n",
  //               default_inference_param->tensor_pool_type_);
  if (status != base::kStatusCodeOk) {
    NNDEPLOY_LOGE("net_->setTensorPoolType failed!\n");
    return base::kStatusCodeErrorInferenceDefault;
  }
  status = net_->init();
  if (status != base::kStatusCodeOk) {
    NNDEPLOY_LOGE("net_->init failed!\n");
    return base::kStatusCodeErrorInferenceDefault;
  }

  status = allocateInputOutputTensor();
  NNDEPLOY_RETURN_ON_NEQ(status, base::kStatusCodeOk,
                         "allocateInputOutputTensor failed!!\n");

  return status;
}

base::Status DefaultInference::deinit() {
  base::Status status = deallocateInputOutputTensor();
  NNDEPLOY_RETURN_ON_NEQ(status, base::kStatusCodeOk,
                         "deallocateInputOutputTensor failed!!\n");

  net_->deinit();
  delete net_;

  // interpret_->deinit();
  delete interpret_;

  return base::kStatusCodeOk;
}

// 获取运行需要的memory大小
int64_t DefaultInference::getMemorySize() { return net_->getMemorySize(); }

// 输入空指针就行，不用自己使用getMemorySize函数
base::Status DefaultInference::setMemory(device::Buffer *buffer) {
  return net_->setMemory(buffer);
}

base::Status DefaultInference::reshape(base::ShapeMap &shape_map) {
  bool flag = false;
  for (auto iter : shape_map) {
    auto tmp = getInputShape(iter.first);
    if (!tmp.empty()) {
      if (base::shapeEqual(iter.second, tmp)) {
        continue;
      } else {
        flag = true;
      }
    } else {
      NNDEPLOY_LOGE("Not exsit input[%s].\n", iter.first.c_str());
      return base::kStatusCodeErrorInferenceDefault;
    }
  }

  if (flag) {
    net_->reshape(shape_map);
    deallocateInputOutputTensor();
    allocateInputOutputTensor();
  }

  return base::kStatusCodeOk;
}

base::Status DefaultInference::run() {
  base::Status status = base::kStatusCodeOk;

  for (auto external_input_tensor : external_input_tensors_) {
    std::string name = external_input_tensor.first;
    device::Tensor *src_tensor = external_input_tensor.second;
    auto internal_input_tensor = input_tensors_.find(name);
    if (internal_input_tensor == input_tensors_.end()) {
      NNDEPLOY_LOGE("Not exsit input[%s].\n", name.c_str());
      continue;
    }
    net_->copyToInputTensor(src_tensor);
  }
  status = net_->preRun();
  if (status != base::kStatusCodeOk) {
    NNDEPLOY_LOGE("net_->preRun failed!\n");
    return base::kStatusCodeErrorInferenceDefault;
  }
  status = net_->run();
  if (status != base::kStatusCodeOk) {
    NNDEPLOY_LOGE("DEFAULT forward failed!\n");
    return base::kStatusCodeErrorInferenceDefault;
  }
  status = net_->postRun();
  if (status != base::kStatusCodeOk) {
    NNDEPLOY_LOGE("DEFAULT forward failed!\n");
    return base::kStatusCodeErrorInferenceDefault;
  }

  // 同步
  DefaultInferenceParam *default_inference_param =
      dynamic_cast<DefaultInferenceParam *>(inference_param_.get());
  device::Device *device =
      device::getDevice(default_inference_param->device_type_);
  status = stream_->synchronize();
  if (status != base::kStatusCodeOk) {
    NNDEPLOY_LOGE("synchronize failed!\n");
    return base::kStatusCodeErrorInferenceDefault;
  }

  return base::kStatusCodeOk;
}

device::Tensor *DefaultInference::getOutputTensorAfterRun(
    const std::string &name, base::DeviceType device_type, bool is_copy,
    base::DataFormat data_format) {
  device::Tensor *output_tensor =
      net_->getOutputTensorAfterRun(name, device_type, is_copy, data_format);
  return output_tensor;
}

base::Status DefaultInference::allocateInputOutputTensor() {
  auto input_tensors = net_->getAllInput();
  auto output_tensors = net_->getAllOutput();
  for (auto input_tensor : input_tensors) {
    input_tensors_.insert({input_tensor->getName(), input_tensor});
  }
  for (auto output_tensor : output_tensors) {
    output_tensors_.insert({output_tensor->getName(), output_tensor});
  }
  return base::kStatusCodeOk;
}

base::Status DefaultInference::deallocateInputOutputTensor() {
  input_tensors_.clear();
  output_tensors_.clear();
  return base::kStatusCodeOk;
}

}  // namespace inference
}  // namespace nndeploy