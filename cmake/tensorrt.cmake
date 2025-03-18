
include(ExternalProject)

# TensorRT头文件路径
#set(TENSORRT_INCLUDE_DIR_USER "/usr/include" CACHE PATH "Path to TensorRT include directory")
set(TensorRT_DIR "/home/sy/my_lib/TensorRT-8.6.1.6_cu11")
set(TENSORRT_INCLUDE_DIR_USER "/home/sy/my_lib/TensorRT-8.6.1.6_cu11/include" CACHE PATH "Path to TensorRT include directory")

#find_package(TensorRT REQUIRED)
include_directories(${TensorRT_DIR}/include)
link_directories(${TensorRT_DIR}/lib)

if (ENABLE_NNDEPLOY_INFERENCE_TENSORRT STREQUAL "OFF")
elseif (ENABLE_NNDEPLOY_INFERENCE_TENSORRT STREQUAL "ON")
  find_path(
    TENSORRT_INCLUDE_DIR NvInfer.h
    HINTS ${TENSORRT_INCLUDE_DIR_USER} ${CUDA_TOOLKIT_ROOT_DIR})
  message(STATUS "Found TensorRT headers at ${TENSORRT_INCLUDE_DIR}")
  
  include_directories(${TENSORRT_INCLUDE_DIR})
  
  
  set(NNDEPLOY_THIRD_PARTY_LIBRARY ${NNDEPLOY_THIRD_PARTY_LIBRARY} libnvinfer.so)
  set(NNDEPLOY_THIRD_PARTY_LIBRARY ${NNDEPLOY_THIRD_PARTY_LIBRARY} libnvinfer_plugin.so)
  set(NNDEPLOY_THIRD_PARTY_LIBRARY ${NNDEPLOY_THIRD_PARTY_LIBRARY} libnvparsers.so)
  set(NNDEPLOY_THIRD_PARTY_LIBRARY ${NNDEPLOY_THIRD_PARTY_LIBRARY} libnvonnxparser.so)
else()
  include_directories(${ENABLE_NNDEPLOY_INFERENCE_TENSORRT}/include)
  set(LIB_PATH ${ENABLE_NNDEPLOY_INFERENCE_TENSORRT}/${NNDEPLOY_THIRD_PARTY_LIBRARY_PATH_SUFFIX})
  set(LIBS "nvinfer" "nvinfer_plugin" "nvparsers" "nvonnxparser")
  foreach(LIB ${LIBS})
    set(LIB_NAME ${NNDEPLOY_LIB_PREFIX}${LIB}${NNDEPLOY_LIB_SUFFIX})
    set(FULL_LIB_NAME ${LIB_PATH}/${LIB_NAME})
    set(NNDEPLOY_THIRD_PARTY_LIBRARY ${NNDEPLOY_THIRD_PARTY_LIBRARY} ${FULL_LIB_NAME})    
  endforeach()
  file(GLOB_RECURSE INSTALL_LIBS "${LIB_PATH}/*")
  foreach(INSTALL_LIB ${INSTALL_LIBS})
    install(FILES ${INSTALL_LIB} DESTINATION ${NNDEPLOY_INSTALL_LIB_PATH})
  endforeach()
  if(SYSTEM.Windows)
    set(BIN_PATH ${ENABLE_NNDEPLOY_INFERENCE_TENSORRT}/bin)
    file(GLOB_RECURSE INSTALL_BINS "${BIN_PATH}/*.dll")
    foreach(INSTALL_BIN ${INSTALL_BINS})
      install(FILES ${INSTALL_BIN} DESTINATION ${NNDEPLOY_INSTALL_BIN_PATH})
      file(COPY ${INSTALL_BIN} DESTINATION ${EXECUTABLE_OUTPUT_PATH})
    endforeach()
  endif()
endif()
