#include "flag.h"
#include "nndeploy/base/glic_stl_include.h"
#include "nndeploy/base/time_profiler.h"
#include "nndeploy/classification/classification.h"
#include "nndeploy/classification/result.h"
#include "nndeploy/codec/codec.h"
#include "nndeploy/codec/opencv/opencv_codec.h"
#include "nndeploy/dag/node.h"
#include "nndeploy/device/device.h"
#include "nndeploy/framework.h"
#include "nndeploy/thread_pool/thread_pool.h"

using namespace nndeploy;

class DrawLableNode : public dag::Node {
 public:
  // DrawLableNode(const std::string &name,
  //               std::initializer_list<dag::Edge *> inputs,
  //               std::initializer_list<dag::Edge *> outputs)
  //     : Node(name, inputs, outputs) {}
  DrawLableNode(const std::string &name, std::vector<dag::Edge *> inputs,
                std::vector<dag::Edge *> outputs)
      : Node(name, inputs, outputs) {
    this->setInputTypeInfo<cv::Mat>();
    this->setInputTypeInfo<classification::ClassificationResult>();
    this->setOutputTypeInfo<cv::Mat>();
  }
  virtual ~DrawLableNode() {}

  virtual base::Status run() {
    cv::Mat *input_mat = inputs_[0]->getCvMat(this);
    classification::ClassificationResult *result =
        (classification::ClassificationResult *)inputs_[1]->getParam(this);
    // 遍历每个分类结果
    for (int i = 0; i < result->labels_.size(); i++) {
      auto label = result->labels_[i];

      // 将分类结果和置信度转为字符串
      std::string text = "class: " + std::to_string(label.label_ids_) +
                         " score: " + std::to_string(label.scores_);

      // 在图像左上角绘制文本
      cv::putText(*input_mat, text, cv::Point(30, 30 + i * 30),
                  cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 255, 0), 2);
    }
    // cv::imwrite("draw_label_node.jpg", *input_mat);
    outputs_[0]->set(input_mat, inputs_[0]->getIndex(this), true);
    return base::kStatusCodeOk;
  }
};

class classificationDemo : public dag::Graph {
 public:
  classificationDemo(const std::string &name) : dag::Graph(name) {}
  virtual ~classificationDemo() {}

  base::Status make(base::InferenceType inference_type,
                    base::CodecFlag codec_flag) {
    base::Status status = base::kStatusCodeOk;
    // 创建分类图
    decode_node_ = (codec::OpenCvImageDecodeNode *)this
                       ->createNode<codec::OpenCvImageDecodeNode>(
                           "decode_node_", codec_flag);
    graph_ =
        (classification::ClassificationResnetGraph *)this
            ->createNode<classification::ClassificationResnetGraph>("resnet");
    graph_->make(inference_type);
    draw_node_ = (DrawLableNode *)this->createNode<DrawLableNode>(
        "draw_node", std::vector<dag::Edge *>(), std::vector<dag::Edge *>());
    encode_node_ = (codec::OpenCvImageEncodeNode *)this
                       ->createNode<codec::OpenCvImageEncodeNode>(
                           "encode_node_", codec_flag);
    return status;
  }

  base::Status setInferParam(base::DeviceType device_type,
                             base::ModelType model_type, bool is_path,
                             std::vector<std::string> &model_value) {
    graph_->setInferParam(device_type, model_type, is_path, model_value);
    return base::kStatusCodeOk;
  }

  base::Status setInputPath(const std::string &input_path) {
    decode_node_->setPath(input_path);
    return base::kStatusCodeOk;
  }

  base::Status setOutputPath(const std::string &output_path) {
    encode_node_->setPath(output_path);
    return base::kStatusCodeOk;
  }

  base::Status setRefPath(const std::string &ref_path) {
    encode_node_->setRefPath(ref_path);
    return base::kStatusCodeOk;
  }

  virtual std::vector<dag::Edge *> forward(std::vector<dag::Edge *> inputs) {
    std::vector<dag::Edge *> decode_node_outputs = (*decode_node_)(inputs);

    std::vector<dag::Edge *> graph_outputs = (*graph_)(decode_node_outputs);

    std::vector<dag::Edge *> draw_node_inputs = {decode_node_outputs[0],
                                                 graph_outputs[0]};
    std::vector<dag::Edge *> draw_node_outputs =
        (*draw_node_)(draw_node_inputs);
    std::vector<dag::Edge *> encode_node_outputs =
        (*encode_node_)(draw_node_outputs);
    return graph_outputs;
  }

 public:
  codec::OpenCvImageDecodeNode *decode_node_;
  codec::OpenCvImageEncodeNode *encode_node_;
  DrawLableNode *draw_node_;
  classification::ClassificationResnetGraph *graph_;
};

int main(int argc, char *argv[]) {
  gflags::ParseCommandLineNonHelpFlags(&argc, &argv, true);
  if (demo::FLAGS_usage) {
    demo::showUsage();
    return -1;
  }

  // 检测模型的有向无环图graph名称，例如:nndeploy::classification::ClassificationResnetGraph
  std::string name = demo::getName();
  // 推理后端类型，例如:
  // kInferenceTypeOpenVino / kInferenceTypeTensorRt /
  base::InferenceType inference_type = demo::getInferenceType();
  // 推理设备类型，例如:
  // kDeviceTypeCodeX86:0/kDeviceTypeCodeCuda:0/...
  base::DeviceType device_type = demo::getDeviceType();
  // 模型类型，例如:
  // kModelTypeOnnx/kModelTypeMnn/...
  base::ModelType model_type = demo::getModelType();
  // 模型是否是路径
  bool is_path = demo::isPath();
  // 模型路径或者模型字符串
  std::vector<std::string> model_value = demo::getModelValue();
  // input path
  std::string input_path = demo::getInputPath();
  base::CodecFlag codec_flag = demo::getCodecFlag();
  // output path
  std::string ouput_path = demo::getOutputPath();
  // base::kParallelTypePipeline / base::kParallelTypeSequential
  base::ParallelType pt = demo::getParallelType();

  classificationDemo graph_demo("resnet_demo");
  graph_demo.setTimeProfileFlag(true);
  graph_demo.make(inference_type, codec_flag);

  graph_demo.setInferParam(device_type, model_type, is_path, model_value);

  std::vector<dag::Edge *> inputs;

  graph_demo.setInputPath(input_path);
  graph_demo.setOutputPath(ouput_path);
  graph_demo.setRefPath(input_path);

  for (int i = 0; i < 100; i++) {
    NNDEPLOY_TIME_POINT_START("graph_demo(inputs)");
    graph_demo(inputs);
    NNDEPLOY_TIME_POINT_END("graph_demo(inputs)");
  }

  // just for dump
  graph_demo.init();
  graph_demo.dump();
  graph_demo.graph_->dump();
  graph_demo.deinit();

  NNDEPLOY_TIME_PROFILER_PRINT("demo");
  NNDEPLOY_TIME_PROFILER_PRINT_REMOVE_WARMUP("demo", 10);

  return 0;
}