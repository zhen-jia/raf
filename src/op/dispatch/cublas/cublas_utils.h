/*!
 * Copyright (c) 2019 by Contributors
 * \file src/op/dispatch/cublas/cublas_utils.h
 * \brief Helper functions for cuBLAS
 */
#pragma once
#include <cublas_v2.h>
#include <tvm/relay/transform.h>
#include "mnm/base.h"
#include "mnm/enum_base.h"
#include "mnm/ir.h"
#include "../../../common/cuda_utils.h"

#define CUBLAS_CALL(func)                                                        \
  do {                                                                           \
    cublasStatus_t e = (func);                                                   \
    CHECK_EQ(e, CUBLAS_STATUS_SUCCESS) << "cublas: " << cublasGetErrorString(e); \
  } while (false)

namespace mnm {
namespace op {
namespace cublas {

inline const char* cublasGetErrorString(cublasStatus_t status) {
  switch (status) {
    case CUBLAS_STATUS_SUCCESS:
      return "CUBLAS_STATUS_SUCCESS";
    case CUBLAS_STATUS_NOT_INITIALIZED:
      return "CUBLAS_STATUS_NOT_INITIALIZED";
    case CUBLAS_STATUS_ALLOC_FAILED:
      return "CUBLAS_STATUS_ALLOC_FAILED";
    case CUBLAS_STATUS_INVALID_VALUE:
      return "CUBLAS_STATUS_INVALID_VALUE";
    case CUBLAS_STATUS_ARCH_MISMATCH:
      return "CUBLAS_STATUS_ARCH_MISMATCH";
    case CUBLAS_STATUS_MAPPING_ERROR:
      return "CUBLAS_STATUS_MAPPING_ERROR";
    case CUBLAS_STATUS_EXECUTION_FAILED:
      return "CUBLAS_STATUS_EXECUTION_FAILED";
    case CUBLAS_STATUS_INTERNAL_ERROR:
      return "CUBLAS_STATUS_INTERNAL_ERROR";
    default:
      LOG(FATAL) << "ValueError: Unknown error!\n";
      throw;
  }
  LOG(FATAL) << "ValueError: Unknown error!\n";
  throw;
}

inline void CUBLASTryEnableTensorCore(cublasHandle_t hdl) {
#if CUDA_VERSION >= 11000
  bool allow_tf32 = tvm::relay::transform::PassContext::Current()
                        ->GetConfig<tvm::Bool>("mnm.cublas.allow_tf32", tvm::Bool(true))
                        .value();
  if (allow_tf32) {
    CUBLAS_CALL(cublasSetMathMode(hdl, CUBLAS_TF32_TENSOR_OP_MATH));
  }
#endif
}

class CUBlasThreadEntry {
 public:
  CUBlasThreadEntry();
  static CUBlasThreadEntry* ThreadLocal();

 public:
  cublasHandle_t handle{nullptr};
};

inline void SetStream(cudaStream_t stream) {
  cublasSetStream(CUBlasThreadEntry::ThreadLocal()->handle, stream);
}

}  // namespace cublas
}  // namespace op
}  // namespace mnm
