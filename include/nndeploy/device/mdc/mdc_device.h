

#ifndef _NNDEPLOY_DEVICE_MDC_DEVICE_H_
#define _NNDEPLOY_DEVICE_MDC_DEVICE_H_

#include "nndeploy/device/device.h"
#include "nndeploy/device/mdc/mdc_include.h"

namespace nndeploy {
namespace device {

class MdcArchitecture : public Architecture {
 public:
  explicit MdcArchitecture(base::DeviceTypeCode device_type_code);

  virtual ~MdcArchitecture();

  virtual base::Status checkDevice(int device_id = 0,
                                   void *command_queue = nullptr,
                                   std::string library_path = "") override;

  virtual base::Status enableDevice(int device_id = 0,
                                    void *command_queue = nullptr,
                                    std::string library_path = "") override;

  virtual Device *getDevice(int device_id) override;

  virtual std::vector<DeviceInfo> getDeviceInfo(
      std::string library_path = "") override;
};

/**
 * @brief
 *
 */
class NNDEPLOY_CC_API MdcDevice : public Device {
  friend class MdcArchitecture;

 public:
  virtual BufferDesc toBufferDesc(const MatDesc &desc,
                                  const base::IntVector &config);

  virtual BufferDesc toBufferDesc(const TensorDesc &desc,
                                  const base::IntVector &config);

  virtual Buffer *allocate(size_t size);
  virtual Buffer *allocate(const BufferDesc &desc);
  virtual void deallocate(Buffer *buffer);

  virtual base::Status copy(Buffer *src, Buffer *dst);
  virtual base::Status download(Buffer *src, Buffer *dst);
  virtual base::Status upload(Buffer *src, Buffer *dst);

  virtual base::Status synchronize();

  virtual void *getCommandQueue();

 protected:
  MdcDevice(base::DeviceType device_type, void *command_queue = nullptr,
            std::string library_path = "")
      : Device(device_type), external_command_queue_(command_queue){};
  virtual ~MdcDevice(){};

  virtual base::Status init();
  virtual base::Status deinit();

 private:
  void *external_command_queue_ = nullptr;
  aclrtStream stream_ = nullptr;
  aclrtContext context_ = nullptr;
};

}  // namespace device
}  // namespace nndeploy

#endif
