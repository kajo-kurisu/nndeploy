#include "flag.h"
#include "nndeploy/base/glic_stl_include.h"
#include "nndeploy/base/shape.h"
#include "nndeploy/base/time_profiler.h"
#include "nndeploy/framework.h"
#include "nndeploy/ir/default_interpret.h"
#include "nndeploy/ir/interpret.h"
#include "nndeploy/ir/ir.h"
#include "nndeploy/net/net.h"
#include "nndeploy/op/expr.h"
#include "nndeploy/op/op.h"

using namespace nndeploy;

class ExprDemo : public ir::ModelDesc {
 public:
  ExprDemo(){};
  ~ExprDemo(){};
  void init() {
    auto input =
        op::makeInput(this, "input", base::dataTypeOf<float>(), {1, 3, 640, 640});
    auto pool_param = std::make_shared<ir::MaxPoolParam>();
    pool_param->kernel_shape_ = {2, 2};
    pool_param->strides_ = {2, 2};
    auto pool1 = op::makeMaxPool(this, input, pool_param);
    auto relu1 = op::makeRelu(this, pool1);
    auto softmax_0 =
        op::makeSoftmax(this, relu1, std::make_shared<ir::SoftmaxParam>());
    auto softmax_1 =
        op::makeSoftmax(this, relu1, std::make_shared<ir::SoftmaxParam>());

    auto add = op::makeAdd(this, softmax_0, softmax_1);

    op::makeOutput(this, add);
  }
};

void printHelloWorld() { std::cout << "hello world!" << std::endl; }

int main(int argc, char const *argv[]) {
  int ret = nndeployFrameworkInit();
  if (ret != 0) {
    NNDEPLOY_LOGE("nndeployFrameworkInit failed. ERROR: %d\n", ret);
    return ret;
  }
  printHelloWorld();
  ret = nndeployFrameworkDeinit();
  if (ret != 0) {
    NNDEPLOY_LOGE("nndeployFrameworkInit failed. ERROR: %d\n", ret);
    return ret;
  }
  return 0;
}
