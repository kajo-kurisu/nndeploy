#include "flag.h"
#include "nndeploy/base/glic_stl_include.h"
#include "nndeploy/base/time_profiler.h"
#include "nndeploy/classification/classification.h"
#include "nndeploy/classification/result.h"
#include "nndeploy/codec/codec.h"
#include "nndeploy/dag/node.h"
#include "nndeploy/device/device.h"
#include "nndeploy/framework.h"
#include "nndeploy/thread_pool/thread_pool.h"

using namespace nndeploy;

class DrawLableNode : public dag::Node {
 public:
  DrawLableNode(const std::string &name,
                std::initializer_list<dag::Edge *> inputs,
                std::initializer_list<dag::Edge *> outputs)
      : Node(name, inputs, outputs) {}
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

    outputs_[0]->set(input_mat, inputs_[0]->getIndex(this), true);
    return base::kStatusCodeOk;
  }
};

#if 0

int main(int argc, char *argv[]) {
  gflags::ParseCommandLineNonHelpFlags(&argc, &argv, true);
  if (demo::FLAGS_usage) {
    demo::showUsage();
    return -1;
  }

  int ret = nndeployFrameworkInit();
  if (ret != 0) {
    NNDEPLOY_LOGE("nndeployFrameworkInit failed. ERROR: %d\n", ret);
    return ret;
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

  // 有向无环图graph的输入边packert
  dag::Edge classification_in("classification_in");
  // 有向无环图graph的输出边packert
  dag::Edge classification_out("classification_out");
  // graph
  dag::Graph *graph = new dag::Graph("demo", {}, {&classification_out});
  if (graph == nullptr) {
    NNDEPLOY_LOGE("graph is nullptr");
    return -1;
  }
  // 创建检测模型有向无环图graph
  classification::ClassificationResnetGraph *classification_graph =
      new classification::ClassificationResnetGraph(name, {&classification_in},
                                                    {&classification_out});
  dag::NodeDesc pre_desc("preprocess", {"classification_in"}, {"data"});
  dag::NodeDesc infer_desc("infer", {"data"}, {"resnetv17_dense0_fwd"});
  dag::NodeDesc post_desc("postprocess", {"resnetv17_dense0_fwd"},
                          {"classification_out"});
  classification_graph->make(pre_desc, infer_desc, inference_type, post_desc);
  classification_graph->setInferParam(device_type, model_type, is_path,
                                      model_value);
  // classification_graph->setTimeProfileFlag(true);
  graph->addNode(classification_graph);

  // 解码节点
  codec::DecodeNode *decode_node = codec::createDecodeNode(
      base::kCodecTypeOpenCV, codec_flag, "decode_node", &classification_in);
  graph->addNode(decode_node);

  // draw box
  dag::Edge *draw_output = graph->createEdge("draw_output");
  dag::Node *draw_label_node = graph->createNode<DrawLableNode>(
      "DrawLableNode", {&classification_in, &classification_out},
      {draw_output});

  // 编码节点
  codec::EncodeNode *encode_node = codec::createEncodeNode(
      base::kCodecTypeOpenCV, codec_flag, "encode_node", draw_output);
  graph->addNode(encode_node);

  // 设置pipeline并行
  base::Status status = graph->setParallelType(pt);
  if (status != base::kStatusCodeOk) {
    NNDEPLOY_LOGE("graph setParallelType failed");
    return -1;
  }

  graph->setTimeProfileFlag(true);

  // 初始化有向无环图graph
  NNDEPLOY_TIME_POINT_START("graph->init()");
  status = graph->init();
  if (status != base::kStatusCodeOk) {
    NNDEPLOY_LOGE("graph init failed");
    return -1;
  }
  NNDEPLOY_TIME_POINT_END("graph->init()");

  status = graph->dump();
  status = classification_graph->dump();

  NNDEPLOY_TIME_POINT_START("graph->run");
  decode_node->setPath(input_path);
  encode_node->setRefPath(input_path);
  encode_node->setPath(ouput_path);
  int size = decode_node->getSize();
  size = 100;
  decode_node->setSize(size);
  for (int i = 0; i < size; ++i) {
    status = graph->run();
    if (status != base::kStatusCodeOk) {
      NNDEPLOY_LOGE("graph run failed");
      return -1;
    }

    if (pt != base::kParallelTypePipeline) {
      classification::ClassificationResult *result =
          (classification::ClassificationResult *)
              classification_out.getGraphOutputParam();
      if (result == nullptr) {
        NNDEPLOY_LOGE("result is nullptr");
        return -1;
      }
    }
  }

  if (pt == base::kParallelTypePipeline) {
    // NNDEPLOY_LOGE("size = %d.\n", size);
    for (int i = 0; i < size; ++i) {
      classification::ClassificationResult *result =
          (classification::ClassificationResult *)
              classification_out.getGraphOutputParam();
      // NNDEPLOY_LOGE("%p.\n", result);
      if (result == nullptr) {
        NNDEPLOY_LOGE("result is nullptr");
        return -1;
      }
    }
  }
  NNDEPLOY_TIME_POINT_END("graph->run");

  // 有向无环图graph反初始化
  status = graph->deinit();
  if (status != base::kStatusCodeOk) {
    NNDEPLOY_LOGE("graph deinit failed");
    return -1;
  }

  NNDEPLOY_TIME_PROFILER_PRINT("demo");
  // NNDEPLOY_TIME_PROFILER_PRINT_INDEX("demo", 0);
  // NNDEPLOY_TIME_PROFILER_PRINT_INDEX("demo", 1);
  // NNDEPLOY_TIME_PROFILER_PRINT_INDEX("demo", 2);
  // NNDEPLOY_TIME_PROFILER_PRINT_INDEX("demo", 50);
  // NNDEPLOY_TIME_PROFILER_PRINT_INDEX("demo", 99);
  NNDEPLOY_TIME_PROFILER_PRINT_REMOVE_WARMUP("demo", 10);

  // 有向无环图graph销毁
  delete encode_node;
  delete decode_node;
  delete classification_graph;
  delete graph;

  ret = nndeployFrameworkDeinit();
  if (ret != 0) {
    NNDEPLOY_LOGE("nndeployFrameworkInit failed. ERROR: %d\n", ret);
    return ret;
  }
  return 0;
}

#else

int main(int argc, char *argv[]) {
  gflags::ParseCommandLineNonHelpFlags(&argc, &argv, true);
  if (demo::FLAGS_usage) {
    demo::showUsage();
    return -1;
  }

  int ret = nndeployFrameworkInit();
  if (ret != 0) {
    NNDEPLOY_LOGE("nndeployFrameworkInit failed. ERROR: %d\n", ret);
    return ret;
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

  // 创建分类图
  classification::ClassificationResnetGraph graph("resnet");
  graph.make(inference_type);
  graph.setInferParam(device_type, model_type, is_path, model_value);

  // 调用forward函数进行推理
  for (int i = 0; i < 1; i++) {
    std::shared_ptr<dag::Edge> input_image =
        std::make_shared<dag::Edge>("input_image");
    cv::Mat image = cv::imread(input_path);
    input_image->set(image, i);
    // NNDEPLOY_LOGE("input_image: %p.\n", input_image.get());
    std::vector<std::shared_ptr<dag::Edge>> outputs =
        graph.forward({input_image}, {"classification_result"}, nullptr);
    // NNDEPLOY_LOGE("outputs: %p.\n", outputs[0].get());
    classification::ClassificationResult *result =
        (classification::ClassificationResult *)outputs[0]
            ->getGraphOutputParam();
    // NNDEPLOY_LOGE("result: %p.\n", result);
    for (int i = 0; i < result->labels_.size(); i++) {
      auto label = result->labels_[i];
      // 将分类结果和置信度转为字符串
      std::string text = "class: " + std::to_string(label.label_ids_) +
                         " score: " + std::to_string(label.scores_);

      // 在图像左上角绘制文本
      cv::putText(image, text, cv::Point(30, 30 + i * 30),
                  cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 255, 0), 2);
    }
    cv::imwrite(ouput_path, image);
  }

  graph.init();

  graph.dump();

  return 0;
}

#endif