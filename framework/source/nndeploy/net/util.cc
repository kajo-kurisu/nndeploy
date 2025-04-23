
#include "nndeploy/net/util.h"

namespace nndeploy {
namespace net {

device::Tensor *getTensor(std::vector<TensorWrapper *> &tensor_repository,
                          const std::string &tensor_name) {
  for (auto tensor_wrapper : tensor_repository) {
    if (tensor_wrapper->name_ == tensor_name) {
      return tensor_wrapper->tensor_;
    }
  }
  return nullptr;
}
TensorWrapper *findTensorWrapper(
    std::vector<TensorWrapper *> &tensor_repository,
    const std::string &tensor_name) {
  for (auto tensor_wrapper : tensor_repository) {
    if (tensor_wrapper->name_ == tensor_name) {
      return tensor_wrapper;
    }
  }
  return nullptr;
}
TensorWrapper *findTensorWrapper(
    std::vector<TensorWrapper *> &tensor_repository, device::Tensor *tensor) {
  for (auto tensor_wrapper : tensor_repository) {
    if (tensor_wrapper->tensor_ == tensor) {
      return tensor_wrapper;
    }
  }
  return nullptr;
}
std::vector<TensorWrapper *> findStartTensors(
    std::vector<TensorWrapper *> &tensor_repository) {
  std::vector<TensorWrapper *> start_tensor;
  for (auto tensor_wrapper : tensor_repository) {
    if (tensor_wrapper->producers_.empty()) {
      start_tensor.emplace_back(tensor_wrapper);
    }
  }
  return start_tensor;
}
std::vector<TensorWrapper *> findEndTensors(
    std::vector<TensorWrapper *> &tensor_repository) {
  std::vector<TensorWrapper *> end_tensor;
  for (auto tensor_wrapper : tensor_repository) {
    if (tensor_wrapper->consumers_.empty()) {
      end_tensor.emplace_back(tensor_wrapper);
    }
  }
  return end_tensor;
}

op::Op *getOp(std::vector<OpWrapper *> &op_repository,
              const std::string &op_name) {
  for (auto op_wrapper : op_repository) {
    if (op_wrapper->name_ == op_name) {
      return op_wrapper->op_;
    }
  }
  return nullptr;
}
OpWrapper *findOpWrapper(std::vector<OpWrapper *> &op_repository,
                         const std::string &op_name) {
  for (auto op_wrapper : op_repository) {
    if (op_wrapper->name_ == op_name) {
      return op_wrapper;
    }
  }
  return nullptr;
}
OpWrapper *findOpWrapper(std::vector<OpWrapper *> &op_repository, op::Op *op) {
  for (auto op_wrapper : op_repository) {
    if (op_wrapper->op_ == op) {
      return op_wrapper;
    }
  }
  return nullptr;
}
std::vector<OpWrapper *> findStartOps(std::vector<OpWrapper *> &op_repository) {
  std::vector<OpWrapper *> start_ops;
  for (auto op_wrapper : op_repository) {
    if (op_wrapper->predecessors_.empty()) {
      start_ops.emplace_back(op_wrapper);
    }
  }
  return start_ops;
}
// 这个实现是不充分的， 有些边可以既是输出边也是中间节点的输入边
std::vector<OpWrapper *> findEndOps(std::vector<OpWrapper *> &op_repository) {
  std::vector<OpWrapper *> end_ops;
  for (auto op_wrapper : op_repository) {
    if (op_wrapper->successors_.empty()) {
      end_ops.emplace_back(op_wrapper);
    }
  }
  return end_ops;
}

base::Status setColor(std::vector<OpWrapper *> &op_repository,
                      base::NodeColorType color) {
  for (auto op_wrapper : op_repository) {
    op_wrapper->color_ = color;
  }
  return base::kStatusCodeOk;
}

base::Status dumpNet(std::vector<TensorWrapper *> &tensor_repository,
                     std::vector<OpWrapper *> &op_repository,
                     std::vector<device::Tensor *> &graph_inputs,
                     std::vector<device::Tensor *> &graph_outputs,
                     const std::string &name, std::ostream &oss) {
  base::Status status = base::kStatusCodeOk;
  // NNDEPLOY_LOGI("#######################\n");
  // NNDEPLOY_LOGI("op::Op dump Phase!\n");
  // NNDEPLOY_LOGI("#######################\n");

  if (name.empty()) {
    oss << "digraph graphviz {\n";
  } else {
    oss << "digraph " << name << " {\n";
  }
  for (auto input : graph_inputs) {
    if (input->getName().empty()) {
      oss << "p" << (void *)input << "[shape=box, label=\""
          << "input"
          << "\"]\n";
    } else {
      oss << "p" << (void *)input << "[shape=box, label=\"" << input->getName()
          << "\"]\n";
    }
    TensorWrapper *tensor_wrapper = findTensorWrapper(tensor_repository, input);
    for (auto op_wrapper : tensor_wrapper->consumers_) {
      auto op = op_wrapper->op_;
      oss << "p" << (void *)input << "->"
          << "p" << (void *)op;
      if (input->getName().empty()) {
        oss << "\n";
      } else {
        oss << "[label=\"" << input->getName() << ", Shape: ";
        for (auto dim : input->getShape()) {
          oss << dim << " ";
        }
        // oss.seekp(-1, std::ios_base::cur);  // 移除最后一个空格
        oss << "\"]\n";
      }
    }
  }
  for (auto op_wrapper : op_repository) {
    op::Op *op = op_wrapper->op_;
    if (op->getName().empty()) {
      oss << "p" << (void *)op << "[label=\""
          << "op"
          << "\"]\n";
    } else {
      oss << "p" << (void *)op << "[label=\"" << op->getName() << "\"]\n";
    }
    for (auto successor : op_wrapper->successors_) {
      auto outputs = op->getAllOutput();
      auto inputs = successor->op_->getAllInput();
      // 两op::Op间可能有多条Tensor
      for (auto output : outputs) {
        device::Tensor *out_in = nullptr;
        for (auto input : inputs) {
          if (output == input) {
            out_in = output;
          }
        }
        if (out_in != nullptr) {
          oss << "p" << (void *)op << "->"
              << "p" << (void *)(successor->op_);
          if (out_in->getName().empty()) {
            oss << "\n";
          } else {
            oss << "[label=\"" << out_in->getName() << ", Shape: ";
            for (auto dim : out_in->getShape()) {
              oss << dim << " ";
            }
            // oss.seekp(-1, std::ios_base::cur);  // 移除最后一个空格
            oss << "\"]\n";
          }
        }
      }
    }
  }
  for (auto output : graph_outputs) {
    if (output->getName().empty()) {
      oss << "p" << (void *)output << "[shape=box, label=\""
          << "output"
          << "\"]\n";
    } else {
      oss << "p" << (void *)output << "[shape=box, label=\""
          << output->getName() << "\"]\n";
    }
    TensorWrapper *tensor_wrapper =
        findTensorWrapper(tensor_repository, output);
    for (auto op_wrapper : tensor_wrapper->producers_) {
      auto op = op_wrapper->op_;
      oss << "p" << (void *)op << "->"
          << "p" << (void *)output;
      if (output->getName().empty()) {
        oss << "\n";
      } else {
        oss << "[label=\"" << output->getName() << "\"]\n";
      }
    }
    // 打印tensor的形状信息
    oss << "p" << (void *)output << "[label=\"" << output->getName()
        << ", Shape: ";
    for (auto dim : output->getShape()) {
      oss << dim << " ";
    }
    // oss.seekp(-1, std::ios_base::cur);  // 移除最后一个空格
    oss << "\"]\n";
  }
  oss << "}\n";
  return status;
}

std::vector<OpWrapper *> checkUnuseOp(std::vector<OpWrapper *> &op_repository) {
  std::vector<OpWrapper *> unused;
  for (auto op_wrapper : op_repository) {
    if (op_wrapper->color_ == base::kNodeColorWhite) {
      NNDEPLOY_LOGE("Unuse op found in graph, op::Op name: %s.",
                    op_wrapper->name_.c_str());
      unused.emplace_back(op_wrapper);
    }
  }
  return unused;
}
std::vector<TensorWrapper *> checkUnuseTensor(
    std::vector<OpWrapper *> &op_repository,
    std::vector<TensorWrapper *> &tensor_repository) {
  std::vector<TensorWrapper *> unused_tensors;
  std::vector<OpWrapper *> unused_ops = checkUnuseOp(op_repository);
  for (auto iter : unused_ops) {
    for (auto iter_input : iter->op_->getAllInput()) {
      TensorWrapper *input_wrapper =
          findTensorWrapper(tensor_repository, iter_input);
      if (input_wrapper != nullptr) {
        NNDEPLOY_LOGE("Unuse tensor found in graph, tensor name: %s.",
                      input_wrapper->name_.c_str());
        unused_tensors.emplace_back(input_wrapper);
      }
    }
    for (auto iter_output : iter->op_->getAllOutput()) {
      TensorWrapper *output_wrapper =
          findTensorWrapper(tensor_repository, iter_output);
      if (output_wrapper != nullptr) {
        NNDEPLOY_LOGE("Unuse tensor found in graph, tensor name: %s.",
                      output_wrapper->name_.c_str());
        unused_tensors.emplace_back(output_wrapper);
      }
    }
  }
  for (auto tensor_wrapper : tensor_repository) {
    if (tensor_wrapper->producers_.empty() &&
        tensor_wrapper->consumers_.empty()) {
      NNDEPLOY_LOGE("Unuse tensor found in graph, tensor name: %s.",
                    tensor_wrapper->name_.c_str());
      unused_tensors.emplace_back(tensor_wrapper);
    }
  }
  return unused_tensors;
}

base::Status topoSortBFS(std::vector<OpWrapper *> &op_repository,
                         std::vector<OpWrapper *> &topo_sort_op) {
  std::vector<OpWrapper *> start_ops = findStartOps(op_repository);
  if (start_ops.empty()) {
    NNDEPLOY_LOGE("No start op found in graph");
    return base::kStatusCodeErrorInvalidValue;
  }
  std::deque<OpWrapper *> op_deque;
  for (auto op_wrapper : start_ops) {
    op_wrapper->color_ = base::kNodeColorGray;
    op_deque.emplace_back(op_wrapper);
  }
  while (!op_deque.empty()) {
    OpWrapper *op_wrapper = op_deque.front();
    for (auto successor : op_wrapper->successors_) {
      if (successor->color_ == base::kNodeColorWhite) {
        successor->color_ = base::kNodeColorGray;
        op_deque.emplace_back(successor);
      } else if (successor->color_ == base::kNodeColorGray) {
        continue;
      } else {
        NNDEPLOY_LOGE("Cycle detected in graph");
        return base::kStatusCodeErrorInvalidValue;
      }
    }
    op_deque.pop_front();
    op_wrapper->color_ = base::kNodeColorBlack;
    topo_sort_op.emplace_back(op_wrapper);
  }

  checkUnuseOp(op_repository);

  return base::kStatusCodeOk;
}

base::Status TopoSortDFSRecursive(OpWrapper *op_wrapper,
                                  std::stack<OpWrapper *> &dst) {
  base::Status status = base::kStatusCodeOk;
  op_wrapper->color_ = base::kNodeColorGray;
  for (auto successor : op_wrapper->successors_) {
    if (successor->color_ == base::kNodeColorWhite) {
      status = TopoSortDFSRecursive(successor, dst);
      NNDEPLOY_RETURN_ON_NEQ(status, base::kStatusCodeOk,
                             "Cycle detected in graph");
    } else if (successor->color_ == base::kNodeColorGray) {
      NNDEPLOY_LOGE("Cycle detected in graph");
      return base::kStatusCodeErrorInvalidValue;
    } else {
      continue;
    }
  }
  op_wrapper->color_ = base::kNodeColorBlack;
  dst.push(op_wrapper);
  return status;
}

base::Status topoSortDFS(std::vector<OpWrapper *> &op_repository,
                         std::vector<OpWrapper *> &topo_sort_op) {
  base::Status status = base::kStatusCodeOk;
  std::vector<OpWrapper *> start_ops = findStartOps(op_repository);
  if (start_ops.empty()) {
    NNDEPLOY_LOGE("No start op found in graph");
    return base::kStatusCodeErrorInvalidValue;
  }
  std::stack<OpWrapper *> dst;
  for (auto op_wrapper : start_ops) {
    if (op_wrapper->color_ == base::kNodeColorWhite) {
      status = TopoSortDFSRecursive(op_wrapper, dst);
    } else if (op_wrapper->color_ == base::kNodeColorGray) {
      NNDEPLOY_LOGE("Cycle detected in graph");
      status = base::kStatusCodeErrorInvalidValue;
    } else {
      continue;
    }
  }
  while (!dst.empty()) {
    topo_sort_op.emplace_back(dst.top());
    dst.pop();
  }

  checkUnuseOp(op_repository);

  return base::kStatusCodeOk;
}

base::Status topoSort(std::vector<OpWrapper *> &op_repository,
                      base::TopoSortType topo_sort_type,
                      std::vector<OpWrapper *> &topo_sort_op) {
  base::Status status = base::kStatusCodeOk;
  if (topo_sort_type == base::kTopoSortTypeBFS) {
    status = topoSortBFS(op_repository, topo_sort_op);
    if (status != base::kStatusCodeOk) {
      return status;
    }
  } else if (topo_sort_type == base::kTopoSortTypeDFS) {
    status = topoSortDFS(op_repository, topo_sort_op);
    if (status != base::kStatusCodeOk) {
      return status;
    }
  } else {
    NNDEPLOY_LOGE("Invalid topo sort type");
    return base::kStatusCodeErrorInvalidValue;
  }
  return status;
}

bool checkTensor(const std::vector<device::Tensor *> &src_tensors,
                 const std::vector<device::Tensor *> &dst_tensors) {
  for (auto tensor : src_tensors) {
    bool flag = false;
    for (auto check_tensor : dst_tensors) {
      if (tensor == check_tensor) {
        flag = true;
        break;
      }
    }
    if (!flag) {
      return false;
    }
  }
  return true;
}

void printNetInfo(const std::vector<OpWrapper*>& op_repository,
                    const std::vector<TensorWrapper*>& tensor_repository) {
  // 打印每个操作的前驱和后继
  for (const auto& op : op_repository) {
    std::cout << "Operation Name: " << op->name_ << std::endl;
    std::cout << "  Predecessors: ";
    for (const auto& pred : op->predecessors_) {
      std::cout << pred->name_ << ", ";
    }
    std::cout << (op->predecessors_.empty() ? "None" : "") << std::endl;

    std::cout << "  Successors: ";
    for (const auto& succ : op->successors_) {
      std::cout << succ->name_ << ", ";
    }
    std::cout << (op->successors_.empty() ? "None" : "") << std::endl;
    std::cout << "-----------------------------" << std::endl;
  }

  // 打印每个张量的生产者和消费者
  for (const auto& tensor : tensor_repository) {
    std::cout << "Tensor Name: " << tensor->name_ << std::endl;
    std::cout << "  Producers: ";
    for (const auto& producer : tensor->producers_) {
      std::cout << producer->name_ << ", ";
    }
    std::cout << (tensor->producers_.empty() ? "None" : "") << std::endl;

    std::cout << "  Consumers: ";
    for (const auto& consumer : tensor->consumers_) {
      std::cout << consumer->name_ << ", ";
    }
    std::cout << (tensor->consumers_.empty() ? "None" : "") << std::endl;
    std::cout << "-----------------------------" << std::endl;
  }
}

}  // namespace net
}  // namespace nndeploy