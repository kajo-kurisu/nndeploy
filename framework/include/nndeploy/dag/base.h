#ifndef _NNDEPLOY_DAG_BASE_H_
#define _NNDEPLOY_DAG_BASE_H_

#include "nndeploy/base/common.h"
#include "nndeploy/base/glic_stl_include.h"
#include "nndeploy/base/log.h"
#include "nndeploy/base/macro.h"
#include "nndeploy/base/object.h"
#include "nndeploy/base/opencv_include.h"
#include "nndeploy/base/param.h"
#include "nndeploy/base/status.h"
#include "nndeploy/device/buffer.h"
#include "nndeploy/device/device.h"
#include "nndeploy/device/memory_pool.h"
#include "nndeploy/device/tensor.h"

namespace nndeploy {
namespace dag {

enum class EdgeTypeFlag {
  kBuffer = 1,
  kCvMat = 2,
  kTensor = 4,
  kParam = 8,
  kAny = 1 << 30,
  kNone = 1 << 31,
};

/**
 * @brief 输入输出类型信息
 * @note 用于描述输入输出类型信息
 */
class NNDEPLOY_CC_API EdgeTypeInfo {
 public:
  EdgeTypeInfo() : type_(EdgeTypeFlag::kBuffer), type_name_("") {}
  ~EdgeTypeInfo() = default;

  template <typename T>
  void setType() {
    typedef typename std::decay<T>::type DT;
    if constexpr (std::is_same<DT, device::Buffer>::value) {
      type_ = EdgeTypeFlag::kBuffer;
      type_name_ = "Buffer";
    } else if constexpr (std::is_same<DT, cv::Mat>::value) {
      type_ = EdgeTypeFlag::kCvMat;
      type_name_ = "CvMat";
    } else if constexpr (std::is_same<DT, device::Tensor>::value) {
      type_ = EdgeTypeFlag::kTensor;
      type_name_ = "Tensor";
    } else if constexpr (std::is_base_of<base::Param, DT>::value) {
      type_ = EdgeTypeFlag::kParam;
      type_name_ = "Param";
    } else {
      type_ = EdgeTypeFlag::kAny;
      type_name_ = getTypeNameRemovePrefix<DT>();
    }
    // type_name_ = getTypeNameRemovePrefix<DT>();
    type_ptr_ = &typeid(DT);
    type_holder_ = std::make_shared<TypeHolder<DT>>();
  }

  EdgeTypeFlag getType() const { return type_; }

  std::string getTypeName() const { return type_name_; }

  const std::type_info* getTypePtr() const { return type_ptr_; }

  template <typename T>
  bool isType() const {
    return (type_ptr_ != nullptr) && (*type_ptr_ == typeid(T));
  }

  template <typename T, typename... Args>
  T* createType(Args&&... args) {
    if (!isType<T>()) {
      NNDEPLOY_LOGE("Type mismatch in createType\n");
      NNDEPLOY_LOGE(" stored=%s\n", type_ptr_->name());
      NNDEPLOY_LOGE(" requested=%s\n", typeid(T).name());
      return nullptr;
    }
    return new T(std::forward<Args>(args)...);
  }

  // 获取类型名称的辅助函数
  template <typename T>
  static std::string getTypeNameRemovePrefix() {
    std::string name = typeid(T).name();
    // 移除命名空间
    size_t pos = name.find_last_of("::");
    if (pos != std::string::npos) {
      name = name.substr(pos + 1);
    }
    // 移除编译器生成的前缀数字
    while (!name.empty() && std::isdigit(name[0])) {
      name = name.substr(1);
    }
    // 移除常见的类型修饰符
    const std::string modifiers[] = {"class ", "struct ", "enum ", "union "};
    for (const auto& mod : modifiers) {
      if (name.substr(0, mod.length()) == mod) {
        name = name.substr(mod.length());
      }
    }
    // 移除空格
    name.erase(std::remove_if(name.begin(), name.end(), ::isspace), name.end());
    return name;
  }

  template <typename T>
  bool checkType() const {
    if (type_ptr_ == nullptr) {
      NNDEPLOY_LOGE("The type info is empty\n");
      NNDEPLOY_LOGE(" requested=%s\n", typeid(T).name());
      return false;
    }
    if (*type_ptr_ != typeid(T)) {
      NNDEPLOY_LOGE("The stored type mismatch\n");
      NNDEPLOY_LOGE(" stored=%s\n", type_ptr_->name());
      NNDEPLOY_LOGE(" requested=%s\n", typeid(T).name());
      return false;
    }
    return true;
  }

 public:
  // Type holder base class
  struct TypeHolderBase {
    virtual ~TypeHolderBase() = default;
  };

  // Type holder for specific type
  template <typename T>
  struct TypeHolder : TypeHolderBase {
    using Type = T;
  };

  EdgeTypeFlag type_;
  std::string type_name_;
  const std::type_info* type_ptr_{nullptr};
  std::shared_ptr<TypeHolderBase> type_holder_;
};

}  // namespace dag
}  // namespace nndeploy

#endif /* _NNDEPLOY_DAG_TYPE_H_ */
