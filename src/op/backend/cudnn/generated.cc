#include <cudnn.h>

#include <mnm/op.h>

#include "../../../common/arg_utils.h"
#include "../../../common/cuda.h"
#include "../../../common/shape_utils.h"
#include "./util.h"

#include "../../attrs/conv.h"
#include "../../attrs/dropout.h"
#include "../../attrs/norm.h"
#include "../../attrs/pool.h"
#include "../../attrs/softmax.h"
namespace mnm {
namespace op {
namespace backend {
namespace cudnn {
namespace generated {

using common::arg_utils::AsVector;
using common::arg_utils::DeduceCtx;
using common::arg_utils::DeduceDLType;
using common::shape_utils::BytesCompactTensor;
using common::shape_utils::MakeShape;
using dmlc::BeginPtr;
using ir::Array;
using ir::Attrs;
using ir::Downcast;
using ir::make_node;
using mnm::op::OpEnv;
using value::OpaqueValue;
using value::TensorValue;
using value::TupleValue;
using value::Value;

class TensorFormatEnum : public EnumBase<TensorFormatEnum, 3, int32_t, cudnnTensorFormat_t> {
 public:
  ENUM_DEF_HEADER(TensorFormatEnum, 0, plain);
  ENUM_DEF_ENTRY_WITH_NAME(TensorFormatEnum, 0, TensorNchw, CUDNN_TENSOR_NCHW, "CUDNN_TENSOR_NCHW");
  ENUM_DEF_ENTRY_WITH_NAME(TensorFormatEnum, 1, TensorNhwc, CUDNN_TENSOR_NHWC, "CUDNN_TENSOR_NHWC");
  ENUM_DEF_ENTRY_WITH_NAME(TensorFormatEnum, 2, TensorNchwVectC, CUDNN_TENSOR_NCHW_VECT_C,
                           "CUDNN_TENSOR_NCHW_VECT_C");
};

class PoolingModeEnum : public EnumBase<PoolingModeEnum, 4, int32_t, cudnnPoolingMode_t> {
 public:
  ENUM_DEF_HEADER(PoolingModeEnum, 0, plain);
  ENUM_DEF_ENTRY_WITH_NAME(PoolingModeEnum, 0, PoolingMax, CUDNN_POOLING_MAX, "CUDNN_POOLING_MAX");
  ENUM_DEF_ENTRY_WITH_NAME(PoolingModeEnum, 1, PoolingAverageCountIncludePadding,
                           CUDNN_POOLING_AVERAGE_COUNT_INCLUDE_PADDING,
                           "CUDNN_POOLING_AVERAGE_COUNT_INCLUDE_PADDING");
  ENUM_DEF_ENTRY_WITH_NAME(PoolingModeEnum, 2, PoolingAverageCountExcludePadding,
                           CUDNN_POOLING_AVERAGE_COUNT_EXCLUDE_PADDING,
                           "CUDNN_POOLING_AVERAGE_COUNT_EXCLUDE_PADDING");
  ENUM_DEF_ENTRY_WITH_NAME(PoolingModeEnum, 3, PoolingMaxDeterministic,
                           CUDNN_POOLING_MAX_DETERMINISTIC, "CUDNN_POOLING_MAX_DETERMINISTIC");
};

class BatchNormModeEnum : public EnumBase<BatchNormModeEnum, 3, int32_t, cudnnBatchNormMode_t> {
 public:
  ENUM_DEF_HEADER(BatchNormModeEnum, 0, plain);
  ENUM_DEF_ENTRY_WITH_NAME(BatchNormModeEnum, 0, BatchnormPerActivation,
                           CUDNN_BATCHNORM_PER_ACTIVATION, "CUDNN_BATCHNORM_PER_ACTIVATION");
  ENUM_DEF_ENTRY_WITH_NAME(BatchNormModeEnum, 1, BatchnormSpatial, CUDNN_BATCHNORM_SPATIAL,
                           "CUDNN_BATCHNORM_SPATIAL");
  ENUM_DEF_ENTRY_WITH_NAME(BatchNormModeEnum, 2, BatchnormSpatialPersistent,
                           CUDNN_BATCHNORM_SPATIAL_PERSISTENT,
                           "CUDNN_BATCHNORM_SPATIAL_PERSISTENT");
};

class ConvolutionBwdFilterAlgoEnum
    : public EnumBase<ConvolutionBwdFilterAlgoEnum, 8, int32_t, cudnnConvolutionBwdFilterAlgo_t> {
 public:
  ENUM_DEF_HEADER(ConvolutionBwdFilterAlgoEnum, 0, plain);
  ENUM_DEF_ENTRY_WITH_NAME(ConvolutionBwdFilterAlgoEnum, 0, ConvolutionBwdFilterAlgo0,
                           CUDNN_CONVOLUTION_BWD_FILTER_ALGO_0,
                           "CUDNN_CONVOLUTION_BWD_FILTER_ALGO_0");
  ENUM_DEF_ENTRY_WITH_NAME(ConvolutionBwdFilterAlgoEnum, 1, ConvolutionBwdFilterAlgo1,
                           CUDNN_CONVOLUTION_BWD_FILTER_ALGO_1,
                           "CUDNN_CONVOLUTION_BWD_FILTER_ALGO_1");
  ENUM_DEF_ENTRY_WITH_NAME(ConvolutionBwdFilterAlgoEnum, 2, ConvolutionBwdFilterAlgoFft,
                           CUDNN_CONVOLUTION_BWD_FILTER_ALGO_FFT,
                           "CUDNN_CONVOLUTION_BWD_FILTER_ALGO_FFT");
  ENUM_DEF_ENTRY_WITH_NAME(ConvolutionBwdFilterAlgoEnum, 3, ConvolutionBwdFilterAlgo3,
                           CUDNN_CONVOLUTION_BWD_FILTER_ALGO_3,
                           "CUDNN_CONVOLUTION_BWD_FILTER_ALGO_3");
  ENUM_DEF_ENTRY_WITH_NAME(ConvolutionBwdFilterAlgoEnum, 4, ConvolutionBwdFilterAlgoWinograd,
                           CUDNN_CONVOLUTION_BWD_FILTER_ALGO_WINOGRAD,
                           "CUDNN_CONVOLUTION_BWD_FILTER_ALGO_WINOGRAD");
  ENUM_DEF_ENTRY_WITH_NAME(ConvolutionBwdFilterAlgoEnum, 5,
                           ConvolutionBwdFilterAlgoWinogradNonfused,
                           CUDNN_CONVOLUTION_BWD_FILTER_ALGO_WINOGRAD_NONFUSED,
                           "CUDNN_CONVOLUTION_BWD_FILTER_ALGO_WINOGRAD_NONFUSED");
  ENUM_DEF_ENTRY_WITH_NAME(ConvolutionBwdFilterAlgoEnum, 6, ConvolutionBwdFilterAlgoFftTiling,
                           CUDNN_CONVOLUTION_BWD_FILTER_ALGO_FFT_TILING,
                           "CUDNN_CONVOLUTION_BWD_FILTER_ALGO_FFT_TILING");
  ENUM_DEF_ENTRY_WITH_NAME(ConvolutionBwdFilterAlgoEnum, 7, ConvolutionBwdFilterAlgoCount,
                           CUDNN_CONVOLUTION_BWD_FILTER_ALGO_COUNT,
                           "CUDNN_CONVOLUTION_BWD_FILTER_ALGO_COUNT");
};

class ActivationModeEnum : public EnumBase<ActivationModeEnum, 6, int32_t, cudnnActivationMode_t> {
 public:
  ENUM_DEF_HEADER(ActivationModeEnum, 0, plain);
  ENUM_DEF_ENTRY_WITH_NAME(ActivationModeEnum, 0, ActivationSigmoid, CUDNN_ACTIVATION_SIGMOID,
                           "CUDNN_ACTIVATION_SIGMOID");
  ENUM_DEF_ENTRY_WITH_NAME(ActivationModeEnum, 1, ActivationRelu, CUDNN_ACTIVATION_RELU,
                           "CUDNN_ACTIVATION_RELU");
  ENUM_DEF_ENTRY_WITH_NAME(ActivationModeEnum, 2, ActivationTanh, CUDNN_ACTIVATION_TANH,
                           "CUDNN_ACTIVATION_TANH");
  ENUM_DEF_ENTRY_WITH_NAME(ActivationModeEnum, 3, ActivationClippedRelu,
                           CUDNN_ACTIVATION_CLIPPED_RELU, "CUDNN_ACTIVATION_CLIPPED_RELU");
  ENUM_DEF_ENTRY_WITH_NAME(ActivationModeEnum, 4, ActivationElu, CUDNN_ACTIVATION_ELU,
                           "CUDNN_ACTIVATION_ELU");
  ENUM_DEF_ENTRY_WITH_NAME(ActivationModeEnum, 5, ActivationIdentity, CUDNN_ACTIVATION_IDENTITY,
                           "CUDNN_ACTIVATION_IDENTITY");
};

class NanPropagationEnum : public EnumBase<NanPropagationEnum, 2, int32_t, cudnnNanPropagation_t> {
 public:
  ENUM_DEF_HEADER(NanPropagationEnum, 0, plain);
  ENUM_DEF_ENTRY_WITH_NAME(NanPropagationEnum, 0, NotPropagateNan, CUDNN_NOT_PROPAGATE_NAN,
                           "CUDNN_NOT_PROPAGATE_NAN");
  ENUM_DEF_ENTRY_WITH_NAME(NanPropagationEnum, 1, PropagateNan, CUDNN_PROPAGATE_NAN,
                           "CUDNN_PROPAGATE_NAN");
};

class SoftmaxModeEnum : public EnumBase<SoftmaxModeEnum, 2, int32_t, cudnnSoftmaxMode_t> {
 public:
  ENUM_DEF_HEADER(SoftmaxModeEnum, 0, plain);
  ENUM_DEF_ENTRY_WITH_NAME(SoftmaxModeEnum, 0, SoftmaxModeInstance, CUDNN_SOFTMAX_MODE_INSTANCE,
                           "CUDNN_SOFTMAX_MODE_INSTANCE");
  ENUM_DEF_ENTRY_WITH_NAME(SoftmaxModeEnum, 1, SoftmaxModeChannel, CUDNN_SOFTMAX_MODE_CHANNEL,
                           "CUDNN_SOFTMAX_MODE_CHANNEL");
};

class ConvolutionFwdAlgoEnum
    : public EnumBase<ConvolutionFwdAlgoEnum, 9, int32_t, cudnnConvolutionFwdAlgo_t> {
 public:
  ENUM_DEF_HEADER(ConvolutionFwdAlgoEnum, 0, plain);
  ENUM_DEF_ENTRY_WITH_NAME(ConvolutionFwdAlgoEnum, 0, ConvolutionFwdAlgoImplicitGemm,
                           CUDNN_CONVOLUTION_FWD_ALGO_IMPLICIT_GEMM,
                           "CUDNN_CONVOLUTION_FWD_ALGO_IMPLICIT_GEMM");
  ENUM_DEF_ENTRY_WITH_NAME(ConvolutionFwdAlgoEnum, 1, ConvolutionFwdAlgoImplicitPrecompGemm,
                           CUDNN_CONVOLUTION_FWD_ALGO_IMPLICIT_PRECOMP_GEMM,
                           "CUDNN_CONVOLUTION_FWD_ALGO_IMPLICIT_PRECOMP_GEMM");
  ENUM_DEF_ENTRY_WITH_NAME(ConvolutionFwdAlgoEnum, 2, ConvolutionFwdAlgoGemm,
                           CUDNN_CONVOLUTION_FWD_ALGO_GEMM, "CUDNN_CONVOLUTION_FWD_ALGO_GEMM");
  ENUM_DEF_ENTRY_WITH_NAME(ConvolutionFwdAlgoEnum, 3, ConvolutionFwdAlgoDirect,
                           CUDNN_CONVOLUTION_FWD_ALGO_DIRECT, "CUDNN_CONVOLUTION_FWD_ALGO_DIRECT");
  ENUM_DEF_ENTRY_WITH_NAME(ConvolutionFwdAlgoEnum, 4, ConvolutionFwdAlgoFft,
                           CUDNN_CONVOLUTION_FWD_ALGO_FFT, "CUDNN_CONVOLUTION_FWD_ALGO_FFT");
  ENUM_DEF_ENTRY_WITH_NAME(ConvolutionFwdAlgoEnum, 5, ConvolutionFwdAlgoFftTiling,
                           CUDNN_CONVOLUTION_FWD_ALGO_FFT_TILING,
                           "CUDNN_CONVOLUTION_FWD_ALGO_FFT_TILING");
  ENUM_DEF_ENTRY_WITH_NAME(ConvolutionFwdAlgoEnum, 6, ConvolutionFwdAlgoWinograd,
                           CUDNN_CONVOLUTION_FWD_ALGO_WINOGRAD,
                           "CUDNN_CONVOLUTION_FWD_ALGO_WINOGRAD");
  ENUM_DEF_ENTRY_WITH_NAME(ConvolutionFwdAlgoEnum, 7, ConvolutionFwdAlgoWinogradNonfused,
                           CUDNN_CONVOLUTION_FWD_ALGO_WINOGRAD_NONFUSED,
                           "CUDNN_CONVOLUTION_FWD_ALGO_WINOGRAD_NONFUSED");
  ENUM_DEF_ENTRY_WITH_NAME(ConvolutionFwdAlgoEnum, 8, ConvolutionFwdAlgoCount,
                           CUDNN_CONVOLUTION_FWD_ALGO_COUNT, "CUDNN_CONVOLUTION_FWD_ALGO_COUNT");
};

class ConvolutionBwdDataAlgoEnum
    : public EnumBase<ConvolutionBwdDataAlgoEnum, 7, int32_t, cudnnConvolutionBwdDataAlgo_t> {
 public:
  ENUM_DEF_HEADER(ConvolutionBwdDataAlgoEnum, 0, plain);
  ENUM_DEF_ENTRY_WITH_NAME(ConvolutionBwdDataAlgoEnum, 0, ConvolutionBwdDataAlgo0,
                           CUDNN_CONVOLUTION_BWD_DATA_ALGO_0, "CUDNN_CONVOLUTION_BWD_DATA_ALGO_0");
  ENUM_DEF_ENTRY_WITH_NAME(ConvolutionBwdDataAlgoEnum, 1, ConvolutionBwdDataAlgo1,
                           CUDNN_CONVOLUTION_BWD_DATA_ALGO_1, "CUDNN_CONVOLUTION_BWD_DATA_ALGO_1");
  ENUM_DEF_ENTRY_WITH_NAME(ConvolutionBwdDataAlgoEnum, 2, ConvolutionBwdDataAlgoFft,
                           CUDNN_CONVOLUTION_BWD_DATA_ALGO_FFT,
                           "CUDNN_CONVOLUTION_BWD_DATA_ALGO_FFT");
  ENUM_DEF_ENTRY_WITH_NAME(ConvolutionBwdDataAlgoEnum, 3, ConvolutionBwdDataAlgoFftTiling,
                           CUDNN_CONVOLUTION_BWD_DATA_ALGO_FFT_TILING,
                           "CUDNN_CONVOLUTION_BWD_DATA_ALGO_FFT_TILING");
  ENUM_DEF_ENTRY_WITH_NAME(ConvolutionBwdDataAlgoEnum, 4, ConvolutionBwdDataAlgoWinograd,
                           CUDNN_CONVOLUTION_BWD_DATA_ALGO_WINOGRAD,
                           "CUDNN_CONVOLUTION_BWD_DATA_ALGO_WINOGRAD");
  ENUM_DEF_ENTRY_WITH_NAME(ConvolutionBwdDataAlgoEnum, 5, ConvolutionBwdDataAlgoWinogradNonfused,
                           CUDNN_CONVOLUTION_BWD_DATA_ALGO_WINOGRAD_NONFUSED,
                           "CUDNN_CONVOLUTION_BWD_DATA_ALGO_WINOGRAD_NONFUSED");
  ENUM_DEF_ENTRY_WITH_NAME(ConvolutionBwdDataAlgoEnum, 6, ConvolutionBwdDataAlgoCount,
                           CUDNN_CONVOLUTION_BWD_DATA_ALGO_COUNT,
                           "CUDNN_CONVOLUTION_BWD_DATA_ALGO_COUNT");
};

class ConvolutionModeEnum
    : public EnumBase<ConvolutionModeEnum, 2, int32_t, cudnnConvolutionMode_t> {
 public:
  ENUM_DEF_HEADER(ConvolutionModeEnum, 0, plain);
  ENUM_DEF_ENTRY_WITH_NAME(ConvolutionModeEnum, 0, Convolution, CUDNN_CONVOLUTION,
                           "CUDNN_CONVOLUTION");
  ENUM_DEF_ENTRY_WITH_NAME(ConvolutionModeEnum, 1, CrossCorrelation, CUDNN_CROSS_CORRELATION,
                           "CUDNN_CROSS_CORRELATION");
};

class SoftmaxAlgorithmEnum
    : public EnumBase<SoftmaxAlgorithmEnum, 3, int32_t, cudnnSoftmaxAlgorithm_t> {
 public:
  ENUM_DEF_HEADER(SoftmaxAlgorithmEnum, 0, plain);
  ENUM_DEF_ENTRY_WITH_NAME(SoftmaxAlgorithmEnum, 0, SoftmaxFast, CUDNN_SOFTMAX_FAST,
                           "CUDNN_SOFTMAX_FAST");
  ENUM_DEF_ENTRY_WITH_NAME(SoftmaxAlgorithmEnum, 1, SoftmaxAccurate, CUDNN_SOFTMAX_ACCURATE,
                           "CUDNN_SOFTMAX_ACCURATE");
  ENUM_DEF_ENTRY_WITH_NAME(SoftmaxAlgorithmEnum, 2, SoftmaxLog, CUDNN_SOFTMAX_LOG,
                           "CUDNN_SOFTMAX_LOG");
};

AlgorithmCache<cudnnConvolutionFwdAlgo_t> _cache_cudnnConvolutionFwdAlgo_t;
cudnnConvolutionFwdAlgo_t FindConvolutionForwardAlgorithm(
    const std::vector<int64_t>& key, const cudnnTensorDescriptor_t xDesc,
    const cudnnFilterDescriptor_t wDesc, const cudnnConvolutionDescriptor_t convDesc,
    const cudnnTensorDescriptor_t yDesc) {
  if (_cache_cudnnConvolutionFwdAlgo_t.has(key)) {
    return _cache_cudnnConvolutionFwdAlgo_t.get(key);
  }
  int cnt;
  cudnnConvolutionFwdAlgoPerf_t res;
  CUDNN_CALL(cudnnFindConvolutionForwardAlgorithm(CUDNNThreadEntry::ThreadLocal()->handle, xDesc,
                                                  wDesc, convDesc, yDesc, 1, &cnt, &res));
  if (res.status != CUDNN_STATUS_SUCCESS) {
    LOG(FATAL) << "ValueError: Cannot find a proper algorithm!\n";
    throw;
  }
  _cache_cudnnConvolutionFwdAlgo_t.set(key, res.algo);
  return res.algo;
}

int64_t GetConvolutionForwardWorkspaceSize(const cudnnTensorDescriptor_t xDesc,
                                           const cudnnFilterDescriptor_t wDesc,
                                           const cudnnConvolutionDescriptor_t convDesc,
                                           const cudnnTensorDescriptor_t yDesc,
                                           cudnnConvolutionFwdAlgo_t algo) {
  size_t sizeInBytes;
  CUDNN_CALL(cudnnGetConvolutionForwardWorkspaceSize(CUDNNThreadEntry::ThreadLocal()->handle, xDesc,
                                                     wDesc, convDesc, yDesc, algo, &sizeInBytes));

  return sizeInBytes;
}

AlgorithmCache<cudnnConvolutionBwdDataAlgo_t> _cache_cudnnConvolutionBwdDataAlgo_t;
cudnnConvolutionBwdDataAlgo_t FindConvolutionBackwardDataAlgorithm(
    const std::vector<int64_t>& key, const cudnnFilterDescriptor_t wDesc,
    const cudnnTensorDescriptor_t dyDesc, const cudnnConvolutionDescriptor_t convDesc,
    const cudnnTensorDescriptor_t dxDesc) {
  if (_cache_cudnnConvolutionBwdDataAlgo_t.has(key)) {
    return _cache_cudnnConvolutionBwdDataAlgo_t.get(key);
  }
  int cnt;
  cudnnConvolutionBwdDataAlgoPerf_t res;
  CUDNN_CALL(cudnnFindConvolutionBackwardDataAlgorithm(
      CUDNNThreadEntry::ThreadLocal()->handle, wDesc, dyDesc, convDesc, dxDesc, 1, &cnt, &res));
  if (res.status != CUDNN_STATUS_SUCCESS) {
    LOG(FATAL) << "ValueError: Cannot find a proper algorithm!\n";
    throw;
  }
  _cache_cudnnConvolutionBwdDataAlgo_t.set(key, res.algo);
  return res.algo;
}

int64_t GetConvolutionBackwardDataWorkspaceSize(const cudnnFilterDescriptor_t wDesc,
                                                const cudnnTensorDescriptor_t dyDesc,
                                                const cudnnConvolutionDescriptor_t convDesc,
                                                const cudnnTensorDescriptor_t dxDesc,
                                                cudnnConvolutionBwdDataAlgo_t algo) {
  size_t sizeInBytes;
  CUDNN_CALL(cudnnGetConvolutionBackwardDataWorkspaceSize(CUDNNThreadEntry::ThreadLocal()->handle,
                                                          wDesc, dyDesc, convDesc, dxDesc, algo,
                                                          &sizeInBytes));

  return sizeInBytes;
}

AlgorithmCache<cudnnConvolutionBwdFilterAlgo_t> _cache_cudnnConvolutionBwdFilterAlgo_t;
cudnnConvolutionBwdFilterAlgo_t FindConvolutionBackwardFilterAlgorithm(
    const std::vector<int64_t>& key, const cudnnTensorDescriptor_t xDesc,
    const cudnnTensorDescriptor_t dyDesc, const cudnnConvolutionDescriptor_t convDesc,
    const cudnnFilterDescriptor_t dwDesc) {
  if (_cache_cudnnConvolutionBwdFilterAlgo_t.has(key)) {
    return _cache_cudnnConvolutionBwdFilterAlgo_t.get(key);
  }
  int cnt;
  cudnnConvolutionBwdFilterAlgoPerf_t res;
  CUDNN_CALL(cudnnFindConvolutionBackwardFilterAlgorithm(
      CUDNNThreadEntry::ThreadLocal()->handle, xDesc, dyDesc, convDesc, dwDesc, 1, &cnt, &res));
  if (res.status != CUDNN_STATUS_SUCCESS) {
    LOG(FATAL) << "ValueError: Cannot find a proper algorithm!\n";
    throw;
  }
  _cache_cudnnConvolutionBwdFilterAlgo_t.set(key, res.algo);
  return res.algo;
}

int64_t GetConvolutionBackwardFilterWorkspaceSize(const cudnnTensorDescriptor_t xDesc,
                                                  const cudnnTensorDescriptor_t dyDesc,
                                                  const cudnnConvolutionDescriptor_t convDesc,
                                                  const cudnnFilterDescriptor_t gradDesc,
                                                  cudnnConvolutionBwdFilterAlgo_t algo) {
  size_t sizeInBytes;
  CUDNN_CALL(cudnnGetConvolutionBackwardFilterWorkspaceSize(CUDNNThreadEntry::ThreadLocal()->handle,
                                                            xDesc, dyDesc, convDesc, gradDesc, algo,
                                                            &sizeInBytes));

  return sizeInBytes;
}

int64_t DropoutGetStatesSize() {
  size_t sizeInBytes;
  CUDNN_CALL(cudnnDropoutGetStatesSize(CUDNNThreadEntry::ThreadLocal()->handle, &sizeInBytes));

  return sizeInBytes;
}

int64_t DropoutGetReserveSpaceSize(cudnnTensorDescriptor_t xdesc) {
  size_t sizeInBytes;
  CUDNN_CALL(cudnnDropoutGetReserveSpaceSize(xdesc, &sizeInBytes));

  return sizeInBytes;
}

// Frontend operator "mnm.op.add" dispatch to
// cudnnAddTensor(cudnnHandle_t handle,
//                const void *alpha,
//                const cudnnTensorDescriptor_t aDesc,
//                const void *A,
//                const void *beta,
//                const cudnnTensorDescriptor_t cDesc,
//                void *C)
class AddTensor_for_op_add : public OpEnv {
 public:
  DType dtype;
  cudnnTensorDescriptor_t aDesc;
  cudnnTensorDescriptor_t cDesc;

  AddTensor_for_op_add(Array<Value> args, const OpInfo& info, Attrs attrs) {
    args.push_back(info->output);
    std::vector<const DLTensor*> dlts = AsVector(args);
    dtype = DeduceDLType(dlts);
    Context ctx = info->ctx;
    (void)ctx;

    FORM_SHAPE(shape_0, dlts[0]);
    FORM_STRIDE(stride_0, shape_0);
    CUDNN_CALL(cudnnCreateTensorDescriptor(&aDesc));
    CUDNN_CALL(cudnnSetTensorNdDescriptor(aDesc, CUDNNDType(dtype), shape_0.size(),
                                          BeginPtr(shape_0), BeginPtr(stride_0)));
    cDesc = aDesc;
    RequestMemory(const_cast<void**>(&dlts[1]->data), ctx, BytesCompactTensor(*dlts[1]));
  }

  ~AddTensor_for_op_add() {
    CUDNN_CALL(cudnnDestroyTensorDescriptor(aDesc));
    CUDNN_CALL(cudnnDestroyTensorDescriptor(cDesc));
  }

  void Execute(Array<Value> args, const OpInfo& info, Attrs attrs) override final {
    args.push_back(info->output);
    std::vector<const DLTensor*> dlts = AsVector(args);
    CUDNN_CALL(cudnnAddTensor(CUDNNThreadEntry::ThreadLocal()->handle,
                              CUDNNDType(dtype).const_addr<1>(), aDesc, dlts[0]->data,
                              CUDNNDType(dtype).const_addr<0>(), cDesc, dlts[1]->data));
    CUDA_CALL(cudaDeviceSynchronize());
  }

  static OpEnv* make(Array<Value> args, const OpInfo& info, Attrs attrs) {
    auto res = std::make_unique<AddTensor_for_op_add>(args, info, attrs);
    return res.release();
  }
};

MNM_REGISTER_OP_DISPATCH("mnm.op.add", DevType::kCUDA(), "generated_cudnn",
                         AddTensor_for_op_add::make);

// Frontend operator "mnm.op.conv2d" dispatch to
// cudnnConvolutionForward(cudnnHandle_t handle,
//                         const void *alpha,
//                         const cudnnTensorDescriptor_t xDesc,
//                         const void *x,
//                         const cudnnFilterDescriptor_t wDesc,
//                         const void *w,
//                         const cudnnConvolutionDescriptor_t convDesc,
//                         cudnnConvolutionFwdAlgo_t algo,
//                         void *workSpace,
//                         size_t workSpaceSizeInBytes,
//                         const void *beta,
//                         const cudnnTensorDescriptor_t yDesc,
//                         void *y)
class ConvolutionForward_for_op_conv2d : public OpEnv {
 public:
  DType dtype;
  cudnnTensorDescriptor_t xDesc;
  cudnnFilterDescriptor_t wDesc;
  cudnnConvolutionDescriptor_t convDesc;
  cudnnConvolutionFwdAlgo_t algo;
  cudnnTensorDescriptor_t yDesc;
  BufferValue workspace;

  ConvolutionForward_for_op_conv2d(Array<Value> args, const OpInfo& info, Attrs attrs) {
    args.push_back(info->output);
    std::vector<const DLTensor*> dlts = AsVector(args);
    dtype = DeduceDLType(dlts);
    Context ctx = info->ctx;
    (void)ctx;
    auto casted_ptr = attrs.as<attrs::ConvAttrs>();
    (void)casted_ptr;

    FORM_SHAPE(shape_0, dlts[0]);
    FORM_STRIDE(stride_0, shape_0);
    CUDNN_CALL(cudnnCreateTensorDescriptor(&xDesc));
    CUDNN_CALL(cudnnSetTensorNdDescriptor(xDesc, CUDNNDType(dtype), shape_0.size(),
                                          BeginPtr(shape_0), BeginPtr(stride_0)));
    FORM_SHAPE(shape_1, dlts[1]);
    FORM_STRIDE(stride_1, shape_1);
    CUDNN_CALL(cudnnCreateFilterDescriptor(&wDesc));
    CUDNN_CALL(cudnnSetFilterNdDescriptor(wDesc, CUDNNDType(dtype),
                                          TensorFormatEnum(TensorFormatEnum::TensorNchw()),
                                          shape_1.size(), BeginPtr(shape_1)));
    CUDNN_CALL(cudnnCreateConvolutionDescriptor(&convDesc));
    CUDNN_CALL(cudnnSetConvolutionNdDescriptor(
        convDesc, casted_ptr->stride.size(), BeginPtr(MakeShape<int>(casted_ptr->padding)),
        BeginPtr(MakeShape<int>(casted_ptr->stride)),
        BeginPtr(MakeShape<int>(casted_ptr->dilation)),
        ConvolutionModeEnum(ConvolutionModeEnum::CrossCorrelation()), CUDNNDType(dtype)));
    CUDNN_CALL(cudnnSetConvolutionGroupCount(convDesc, casted_ptr->groups));
    FORM_SHAPE(shape_2, dlts[2]);
    FORM_STRIDE(stride_2, shape_2);
    CUDNN_CALL(cudnnCreateTensorDescriptor(&yDesc));
    CUDNN_CALL(cudnnSetTensorNdDescriptor(yDesc, CUDNNDType(dtype), shape_2.size(),
                                          BeginPtr(shape_2), BeginPtr(stride_2)));
    std::vector<int64_t> key;
    VecAppend(key, shape_0);
    VecAppend(key, shape_1);
    VecAppend(key, shape_2);
    VecAppend(key, casted_ptr->padding);
    VecAppend(key, casted_ptr->stride);
    VecAppend(key, casted_ptr->dilation);
    algo = FindConvolutionForwardAlgorithm(key, xDesc, wDesc, convDesc, yDesc);
    workspace = BufferValue(make_node<BufferNode>());
    workspace->size_in_bytes =
        GetConvolutionForwardWorkspaceSize(xDesc, wDesc, convDesc, yDesc, algo);
    RequestWorkspace(const_cast<void**>(&workspace->data), ctx, workspace->size_in_bytes);
    RequestMemory(const_cast<void**>(&dlts[2]->data), ctx, BytesCompactTensor(*dlts[2]));
  }

  ~ConvolutionForward_for_op_conv2d() {
    CUDNN_CALL(cudnnDestroyTensorDescriptor(xDesc));
    CUDNN_CALL(cudnnDestroyFilterDescriptor(wDesc));
    CUDNN_CALL(cudnnDestroyConvolutionDescriptor(convDesc));
    CUDNN_CALL(cudnnDestroyTensorDescriptor(yDesc));
  }

  void Execute(Array<Value> args, const OpInfo& info, Attrs attrs) override final {
    args.push_back(info->output);
    std::vector<const DLTensor*> dlts = AsVector(args);
    auto casted_ptr = attrs.as<attrs::ConvAttrs>();
    (void)casted_ptr;
    CUDNN_CALL(cudnnConvolutionForward(
        CUDNNThreadEntry::ThreadLocal()->handle, CUDNNDType(dtype).const_addr<1>(), xDesc,
        dlts[0]->data, wDesc, dlts[1]->data, convDesc, algo, workspace->data,
        workspace->size_in_bytes, CUDNNDType(dtype).const_addr<0>(), yDesc, dlts[2]->data));
    CUDA_CALL(cudaDeviceSynchronize());
  }

  static OpEnv* make(Array<Value> args, const OpInfo& info, Attrs attrs) {
    auto res = std::make_unique<ConvolutionForward_for_op_conv2d>(args, info, attrs);
    return res.release();
  }
};

MNM_REGISTER_OP_DISPATCH("mnm.op.conv2d", DevType::kCUDA(), "generated_cudnn",
                         ConvolutionForward_for_op_conv2d::make);

// Frontend operator "mnm.op.grad.conv2d_data" dispatch to
// cudnnConvolutionBackwardData(cudnnHandle_t handle,
//                              const void *alpha,
//                              const cudnnFilterDescriptor_t wDesc,
//                              const void *w,
//                              const cudnnTensorDescriptor_t dyDesc,
//                              const void *dy,
//                              const cudnnConvolutionDescriptor_t convDesc,
//                              cudnnConvolutionBwdDataAlgo_t algo,
//                              void *workSpace,
//                              size_t workSpaceSizeInBytes,
//                              const void *beta,
//                              const cudnnTensorDescriptor_t dxDesc,
//                              void *dx)
class ConvolutionBackwardData_for_op_grad_conv2d_data : public OpEnv {
 public:
  DType dtype;
  cudnnFilterDescriptor_t wDesc;
  cudnnTensorDescriptor_t dyDesc;
  cudnnConvolutionDescriptor_t convDesc;
  cudnnConvolutionBwdDataAlgo_t algo;
  cudnnTensorDescriptor_t dxDesc;
  BufferValue workspace;

  ConvolutionBackwardData_for_op_grad_conv2d_data(Array<Value> args, const OpInfo& info,
                                                  Attrs attrs) {
    args.push_back(info->output);
    std::vector<const DLTensor*> dlts = AsVector(args);
    dtype = DeduceDLType(dlts);
    Context ctx = info->ctx;
    (void)ctx;
    auto casted_ptr = attrs.as<attrs::ConvBackAttrs>();
    (void)casted_ptr;

    FORM_SHAPE(shape_0, dlts[0]);
    FORM_STRIDE(stride_0, shape_0);
    CUDNN_CALL(cudnnCreateFilterDescriptor(&wDesc));
    CUDNN_CALL(cudnnSetFilterNdDescriptor(wDesc, CUDNNDType(dtype),
                                          TensorFormatEnum(TensorFormatEnum::TensorNchw()),
                                          shape_0.size(), BeginPtr(shape_0)));
    FORM_SHAPE(shape_1, dlts[1]);
    FORM_STRIDE(stride_1, shape_1);
    CUDNN_CALL(cudnnCreateTensorDescriptor(&dyDesc));
    CUDNN_CALL(cudnnSetTensorNdDescriptor(dyDesc, CUDNNDType(dtype), shape_1.size(),
                                          BeginPtr(shape_1), BeginPtr(stride_1)));
    CUDNN_CALL(cudnnCreateConvolutionDescriptor(&convDesc));
    CUDNN_CALL(cudnnSetConvolutionNdDescriptor(
        convDesc, casted_ptr->stride.size(), BeginPtr(MakeShape<int>(casted_ptr->padding)),
        BeginPtr(MakeShape<int>(casted_ptr->stride)),
        BeginPtr(MakeShape<int>(casted_ptr->dilation)),
        ConvolutionModeEnum(ConvolutionModeEnum::CrossCorrelation()), CUDNNDType(dtype)));
    CUDNN_CALL(cudnnSetConvolutionGroupCount(convDesc, casted_ptr->groups));
    FORM_SHAPE(shape_2, dlts[2]);
    FORM_STRIDE(stride_2, shape_2);
    CUDNN_CALL(cudnnCreateTensorDescriptor(&dxDesc));
    CUDNN_CALL(cudnnSetTensorNdDescriptor(dxDesc, CUDNNDType(dtype), shape_2.size(),
                                          BeginPtr(shape_2), BeginPtr(stride_2)));
    std::vector<int64_t> key;
    VecAppend(key, shape_0);
    VecAppend(key, shape_1);
    VecAppend(key, shape_2);
    VecAppend(key, casted_ptr->padding);
    VecAppend(key, casted_ptr->stride);
    VecAppend(key, casted_ptr->dilation);
    algo = FindConvolutionBackwardDataAlgorithm(key, wDesc, dyDesc, convDesc, dxDesc);
    workspace = BufferValue(make_node<BufferNode>());
    workspace->size_in_bytes =
        GetConvolutionBackwardDataWorkspaceSize(wDesc, dyDesc, convDesc, dxDesc, algo);
    RequestWorkspace(const_cast<void**>(&workspace->data), ctx, workspace->size_in_bytes);
    RequestMemory(const_cast<void**>(&dlts[2]->data), ctx, BytesCompactTensor(*dlts[2]));
  }

  ~ConvolutionBackwardData_for_op_grad_conv2d_data() {
    CUDNN_CALL(cudnnDestroyFilterDescriptor(wDesc));
    CUDNN_CALL(cudnnDestroyTensorDescriptor(dyDesc));
    CUDNN_CALL(cudnnDestroyConvolutionDescriptor(convDesc));
    CUDNN_CALL(cudnnDestroyTensorDescriptor(dxDesc));
  }

  void Execute(Array<Value> args, const OpInfo& info, Attrs attrs) override final {
    args.push_back(info->output);
    std::vector<const DLTensor*> dlts = AsVector(args);
    auto casted_ptr = attrs.as<attrs::ConvBackAttrs>();
    (void)casted_ptr;
    CUDNN_CALL(cudnnConvolutionBackwardData(
        CUDNNThreadEntry::ThreadLocal()->handle, CUDNNDType(dtype).const_addr<1>(), wDesc,
        dlts[0]->data, dyDesc, dlts[1]->data, convDesc, algo, workspace->data,
        workspace->size_in_bytes, CUDNNDType(dtype).const_addr<0>(), dxDesc, dlts[2]->data));
    CUDA_CALL(cudaDeviceSynchronize());
  }

  static OpEnv* make(Array<Value> args, const OpInfo& info, Attrs attrs) {
    auto res = std::make_unique<ConvolutionBackwardData_for_op_grad_conv2d_data>(args, info, attrs);
    return res.release();
  }
};

MNM_REGISTER_OP_DISPATCH("mnm.op.grad.conv2d_data", DevType::kCUDA(), "generated_cudnn",
                         ConvolutionBackwardData_for_op_grad_conv2d_data::make);

// Frontend operator "mnm.op.grad.conv2d_filter" dispatch to
// cudnnConvolutionBackwardFilter(cudnnHandle_t handle,
//                                const void *alpha,
//                                const cudnnTensorDescriptor_t xDesc,
//                                const void *x,
//                                const cudnnTensorDescriptor_t dyDesc,
//                                const void *dy,
//                                const cudnnConvolutionDescriptor_t convDesc,
//                                cudnnConvolutionBwdFilterAlgo_t algo,
//                                void *workSpace,
//                                size_t workSpaceSizeInBytes,
//                                const void *beta,
//                                const cudnnFilterDescriptor_t dwDesc,
//                                void *dw)
class ConvolutionBackwardFilter_for_op_grad_conv2d_filter : public OpEnv {
 public:
  DType dtype;
  cudnnTensorDescriptor_t xDesc;
  cudnnTensorDescriptor_t dyDesc;
  cudnnConvolutionDescriptor_t convDesc;
  cudnnConvolutionBwdFilterAlgo_t algo;
  cudnnFilterDescriptor_t dwDesc;
  BufferValue workspace;

  ConvolutionBackwardFilter_for_op_grad_conv2d_filter(Array<Value> args, const OpInfo& info,
                                                      Attrs attrs) {
    args.push_back(info->output);
    std::vector<const DLTensor*> dlts = AsVector(args);
    dtype = DeduceDLType(dlts);
    Context ctx = info->ctx;
    (void)ctx;
    auto casted_ptr = attrs.as<attrs::ConvAttrs>();
    (void)casted_ptr;

    FORM_SHAPE(shape_0, dlts[0]);
    FORM_STRIDE(stride_0, shape_0);
    CUDNN_CALL(cudnnCreateTensorDescriptor(&xDesc));
    CUDNN_CALL(cudnnSetTensorNdDescriptor(xDesc, CUDNNDType(dtype), shape_0.size(),
                                          BeginPtr(shape_0), BeginPtr(stride_0)));
    FORM_SHAPE(shape_1, dlts[1]);
    FORM_STRIDE(stride_1, shape_1);
    CUDNN_CALL(cudnnCreateTensorDescriptor(&dyDesc));
    CUDNN_CALL(cudnnSetTensorNdDescriptor(dyDesc, CUDNNDType(dtype), shape_1.size(),
                                          BeginPtr(shape_1), BeginPtr(stride_1)));
    CUDNN_CALL(cudnnCreateConvolutionDescriptor(&convDesc));
    CUDNN_CALL(cudnnSetConvolutionNdDescriptor(
        convDesc, casted_ptr->stride.size(), BeginPtr(MakeShape<int>(casted_ptr->padding)),
        BeginPtr(MakeShape<int>(casted_ptr->stride)),
        BeginPtr(MakeShape<int>(casted_ptr->dilation)),
        ConvolutionModeEnum(ConvolutionModeEnum::CrossCorrelation()), CUDNNDType(dtype)));
    CUDNN_CALL(cudnnSetConvolutionGroupCount(convDesc, casted_ptr->groups));
    FORM_SHAPE(shape_2, dlts[2]);
    FORM_STRIDE(stride_2, shape_2);
    CUDNN_CALL(cudnnCreateFilterDescriptor(&dwDesc));
    CUDNN_CALL(cudnnSetFilterNdDescriptor(dwDesc, CUDNNDType(dtype),
                                          TensorFormatEnum(TensorFormatEnum::TensorNchw()),
                                          shape_2.size(), BeginPtr(shape_2)));
    std::vector<int64_t> key;
    VecAppend(key, shape_0);
    VecAppend(key, shape_1);
    VecAppend(key, shape_2);
    VecAppend(key, casted_ptr->padding);
    VecAppend(key, casted_ptr->stride);
    VecAppend(key, casted_ptr->dilation);
    algo = FindConvolutionBackwardFilterAlgorithm(key, xDesc, dyDesc, convDesc, dwDesc);
    workspace = BufferValue(make_node<BufferNode>());
    workspace->size_in_bytes =
        GetConvolutionBackwardFilterWorkspaceSize(xDesc, dyDesc, convDesc, dwDesc, algo);
    RequestWorkspace(const_cast<void**>(&workspace->data), ctx, workspace->size_in_bytes);
    RequestMemory(const_cast<void**>(&dlts[2]->data), ctx, BytesCompactTensor(*dlts[2]));
  }

  ~ConvolutionBackwardFilter_for_op_grad_conv2d_filter() {
    CUDNN_CALL(cudnnDestroyTensorDescriptor(xDesc));
    CUDNN_CALL(cudnnDestroyTensorDescriptor(dyDesc));
    CUDNN_CALL(cudnnDestroyConvolutionDescriptor(convDesc));
    CUDNN_CALL(cudnnDestroyFilterDescriptor(dwDesc));
  }

  void Execute(Array<Value> args, const OpInfo& info, Attrs attrs) override final {
    args.push_back(info->output);
    std::vector<const DLTensor*> dlts = AsVector(args);
    auto casted_ptr = attrs.as<attrs::ConvAttrs>();
    (void)casted_ptr;
    CUDNN_CALL(cudnnConvolutionBackwardFilter(
        CUDNNThreadEntry::ThreadLocal()->handle, CUDNNDType(dtype).const_addr<1>(), xDesc,
        dlts[0]->data, dyDesc, dlts[1]->data, convDesc, algo, workspace->data,
        workspace->size_in_bytes, CUDNNDType(dtype).const_addr<0>(), dwDesc, dlts[2]->data));
    CUDA_CALL(cudaDeviceSynchronize());
  }

  static OpEnv* make(Array<Value> args, const OpInfo& info, Attrs attrs) {
    auto res =
        std::make_unique<ConvolutionBackwardFilter_for_op_grad_conv2d_filter>(args, info, attrs);
    return res.release();
  }
};

MNM_REGISTER_OP_DISPATCH("mnm.op.grad.conv2d_filter", DevType::kCUDA(), "generated_cudnn",
                         ConvolutionBackwardFilter_for_op_grad_conv2d_filter::make);

// Frontend operator "mnm.op.relu" dispatch to
// cudnnActivationForward(cudnnHandle_t handle,
//                        cudnnActivationDescriptor_t activationDesc,
//                        const void *alpha,
//                        const cudnnTensorDescriptor_t xDesc,
//                        const void *x,
//                        const void *beta,
//                        const cudnnTensorDescriptor_t yDesc,
//                        void *y)
class ActivationForward_for_op_relu : public OpEnv {
 public:
  DType dtype;
  cudnnActivationDescriptor_t activationDesc;
  cudnnTensorDescriptor_t xDesc;
  cudnnTensorDescriptor_t yDesc;

  ActivationForward_for_op_relu(Array<Value> args, const OpInfo& info, Attrs attrs) {
    args.push_back(info->output);
    std::vector<const DLTensor*> dlts = AsVector(args);
    dtype = DeduceDLType(dlts);
    Context ctx = info->ctx;
    (void)ctx;

    CUDNN_CALL(cudnnCreateActivationDescriptor(&activationDesc));
    CUDNN_CALL(cudnnSetActivationDescriptor(
        activationDesc, ActivationModeEnum(ActivationModeEnum::ActivationRelu()),
        NanPropagationEnum(NanPropagationEnum::PropagateNan()), 0.0));
    FORM_SHAPE(shape_0, dlts[0]);
    FORM_STRIDE(stride_0, shape_0);
    CUDNN_CALL(cudnnCreateTensorDescriptor(&xDesc));
    CUDNN_CALL(cudnnSetTensorNdDescriptor(xDesc, CUDNNDType(dtype), shape_0.size(),
                                          BeginPtr(shape_0), BeginPtr(stride_0)));
    FORM_SHAPE(shape_1, dlts[1]);
    FORM_STRIDE(stride_1, shape_1);
    CUDNN_CALL(cudnnCreateTensorDescriptor(&yDesc));
    CUDNN_CALL(cudnnSetTensorNdDescriptor(yDesc, CUDNNDType(dtype), shape_1.size(),
                                          BeginPtr(shape_1), BeginPtr(stride_1)));
    RequestMemory(const_cast<void**>(&dlts[1]->data), ctx, BytesCompactTensor(*dlts[1]));
  }

  ~ActivationForward_for_op_relu() {
    CUDNN_CALL(cudnnDestroyActivationDescriptor(activationDesc));
    CUDNN_CALL(cudnnDestroyTensorDescriptor(xDesc));
    CUDNN_CALL(cudnnDestroyTensorDescriptor(yDesc));
  }

  void Execute(Array<Value> args, const OpInfo& info, Attrs attrs) override final {
    args.push_back(info->output);
    std::vector<const DLTensor*> dlts = AsVector(args);
    CUDNN_CALL(cudnnActivationForward(CUDNNThreadEntry::ThreadLocal()->handle, activationDesc,
                                      CUDNNDType(dtype).const_addr<1>(), xDesc, dlts[0]->data,
                                      CUDNNDType(dtype).const_addr<0>(), yDesc, dlts[1]->data));
    CUDA_CALL(cudaDeviceSynchronize());
  }

  static OpEnv* make(Array<Value> args, const OpInfo& info, Attrs attrs) {
    auto res = std::make_unique<ActivationForward_for_op_relu>(args, info, attrs);
    return res.release();
  }
};

MNM_REGISTER_OP_DISPATCH("mnm.op.relu", DevType::kCUDA(), "generated_cudnn",
                         ActivationForward_for_op_relu::make);

// Frontend operator "mnm.op.grad.relu" dispatch to
// cudnnActivationBackward(cudnnHandle_t handle,
//                         cudnnActivationDescriptor_t activationDesc,
//                         const void *alpha,
//                         const cudnnTensorDescriptor_t yDesc,
//                         const void *y,
//                         const cudnnTensorDescriptor_t dyDesc,
//                         const void *dy,
//                         const cudnnTensorDescriptor_t xDesc,
//                         const void *x,
//                         const void *beta,
//                         const cudnnTensorDescriptor_t dxDesc,
//                         void *dx)
class ActivationBackward_for_op_grad_relu : public OpEnv {
 public:
  DType dtype;
  cudnnActivationDescriptor_t activationDesc;
  cudnnTensorDescriptor_t yDesc;
  cudnnTensorDescriptor_t dyDesc;
  cudnnTensorDescriptor_t xDesc;
  cudnnTensorDescriptor_t dxDesc;

  ActivationBackward_for_op_grad_relu(Array<Value> args, const OpInfo& info, Attrs attrs) {
    args.push_back(info->output);
    std::vector<const DLTensor*> dlts = AsVector(args);
    dtype = DeduceDLType(dlts);
    Context ctx = info->ctx;
    (void)ctx;

    CUDNN_CALL(cudnnCreateActivationDescriptor(&activationDesc));
    CUDNN_CALL(cudnnSetActivationDescriptor(
        activationDesc, ActivationModeEnum(ActivationModeEnum::ActivationRelu()),
        NanPropagationEnum(NanPropagationEnum::PropagateNan()), 0.0));
    FORM_SHAPE(shape_0, dlts[0]);
    FORM_STRIDE(stride_0, shape_0);
    CUDNN_CALL(cudnnCreateTensorDescriptor(&yDesc));
    CUDNN_CALL(cudnnSetTensorNdDescriptor(yDesc, CUDNNDType(dtype), shape_0.size(),
                                          BeginPtr(shape_0), BeginPtr(stride_0)));
    FORM_SHAPE(shape_1, dlts[1]);
    FORM_STRIDE(stride_1, shape_1);
    CUDNN_CALL(cudnnCreateTensorDescriptor(&dyDesc));
    CUDNN_CALL(cudnnSetTensorNdDescriptor(dyDesc, CUDNNDType(dtype), shape_1.size(),
                                          BeginPtr(shape_1), BeginPtr(stride_1)));
    FORM_SHAPE(shape_2, dlts[2]);
    FORM_STRIDE(stride_2, shape_2);
    CUDNN_CALL(cudnnCreateTensorDescriptor(&xDesc));
    CUDNN_CALL(cudnnSetTensorNdDescriptor(xDesc, CUDNNDType(dtype), shape_2.size(),
                                          BeginPtr(shape_2), BeginPtr(stride_2)));
    FORM_SHAPE(shape_3, dlts[3]);
    FORM_STRIDE(stride_3, shape_3);
    CUDNN_CALL(cudnnCreateTensorDescriptor(&dxDesc));
    CUDNN_CALL(cudnnSetTensorNdDescriptor(dxDesc, CUDNNDType(dtype), shape_3.size(),
                                          BeginPtr(shape_3), BeginPtr(stride_3)));
    RequestMemory(const_cast<void**>(&dlts[3]->data), ctx, BytesCompactTensor(*dlts[3]));
  }

  ~ActivationBackward_for_op_grad_relu() {
    CUDNN_CALL(cudnnDestroyActivationDescriptor(activationDesc));
    CUDNN_CALL(cudnnDestroyTensorDescriptor(yDesc));
    CUDNN_CALL(cudnnDestroyTensorDescriptor(dyDesc));
    CUDNN_CALL(cudnnDestroyTensorDescriptor(xDesc));
    CUDNN_CALL(cudnnDestroyTensorDescriptor(dxDesc));
  }

  void Execute(Array<Value> args, const OpInfo& info, Attrs attrs) override final {
    args.push_back(info->output);
    std::vector<const DLTensor*> dlts = AsVector(args);
    CUDNN_CALL(cudnnActivationBackward(CUDNNThreadEntry::ThreadLocal()->handle, activationDesc,
                                       CUDNNDType(dtype).const_addr<1>(), yDesc, dlts[0]->data,
                                       dyDesc, dlts[1]->data, xDesc, dlts[2]->data,
                                       CUDNNDType(dtype).const_addr<0>(), dxDesc, dlts[3]->data));
    CUDA_CALL(cudaDeviceSynchronize());
  }

  static OpEnv* make(Array<Value> args, const OpInfo& info, Attrs attrs) {
    auto res = std::make_unique<ActivationBackward_for_op_grad_relu>(args, info, attrs);
    return res.release();
  }
};

MNM_REGISTER_OP_DISPATCH("mnm.op.grad.relu", DevType::kCUDA(), "generated_cudnn",
                         ActivationBackward_for_op_grad_relu::make);

// Frontend operator "mnm.op.tanh" dispatch to
// cudnnActivationForward(cudnnHandle_t handle,
//                        cudnnActivationDescriptor_t activationDesc,
//                        const void *alpha,
//                        const cudnnTensorDescriptor_t xDesc,
//                        const void *x,
//                        const void *beta,
//                        const cudnnTensorDescriptor_t yDesc,
//                        void *y)
class ActivationForward_for_op_tanh : public OpEnv {
 public:
  DType dtype;
  cudnnActivationDescriptor_t activationDesc;
  cudnnTensorDescriptor_t xDesc;
  cudnnTensorDescriptor_t yDesc;

  ActivationForward_for_op_tanh(Array<Value> args, const OpInfo& info, Attrs attrs) {
    args.push_back(info->output);
    std::vector<const DLTensor*> dlts = AsVector(args);
    dtype = DeduceDLType(dlts);
    Context ctx = info->ctx;
    (void)ctx;

    CUDNN_CALL(cudnnCreateActivationDescriptor(&activationDesc));
    CUDNN_CALL(cudnnSetActivationDescriptor(
        activationDesc, ActivationModeEnum(ActivationModeEnum::ActivationTanh()),
        NanPropagationEnum(NanPropagationEnum::PropagateNan()), 0.0));
    FORM_SHAPE(shape_0, dlts[0]);
    FORM_STRIDE(stride_0, shape_0);
    CUDNN_CALL(cudnnCreateTensorDescriptor(&xDesc));
    CUDNN_CALL(cudnnSetTensorNdDescriptor(xDesc, CUDNNDType(dtype), shape_0.size(),
                                          BeginPtr(shape_0), BeginPtr(stride_0)));
    FORM_SHAPE(shape_1, dlts[1]);
    FORM_STRIDE(stride_1, shape_1);
    CUDNN_CALL(cudnnCreateTensorDescriptor(&yDesc));
    CUDNN_CALL(cudnnSetTensorNdDescriptor(yDesc, CUDNNDType(dtype), shape_1.size(),
                                          BeginPtr(shape_1), BeginPtr(stride_1)));
    RequestMemory(const_cast<void**>(&dlts[1]->data), ctx, BytesCompactTensor(*dlts[1]));
  }

  ~ActivationForward_for_op_tanh() {
    CUDNN_CALL(cudnnDestroyActivationDescriptor(activationDesc));
    CUDNN_CALL(cudnnDestroyTensorDescriptor(xDesc));
    CUDNN_CALL(cudnnDestroyTensorDescriptor(yDesc));
  }

  void Execute(Array<Value> args, const OpInfo& info, Attrs attrs) override final {
    args.push_back(info->output);
    std::vector<const DLTensor*> dlts = AsVector(args);
    CUDNN_CALL(cudnnActivationForward(CUDNNThreadEntry::ThreadLocal()->handle, activationDesc,
                                      CUDNNDType(dtype).const_addr<1>(), xDesc, dlts[0]->data,
                                      CUDNNDType(dtype).const_addr<0>(), yDesc, dlts[1]->data));
    CUDA_CALL(cudaDeviceSynchronize());
  }

  static OpEnv* make(Array<Value> args, const OpInfo& info, Attrs attrs) {
    auto res = std::make_unique<ActivationForward_for_op_tanh>(args, info, attrs);
    return res.release();
  }
};

MNM_REGISTER_OP_DISPATCH("mnm.op.tanh", DevType::kCUDA(), "generated_cudnn",
                         ActivationForward_for_op_tanh::make);

// Frontend operator "mnm.op.grad.tanh" dispatch to
// cudnnActivationBackward(cudnnHandle_t handle,
//                         cudnnActivationDescriptor_t activationDesc,
//                         const void *alpha,
//                         const cudnnTensorDescriptor_t yDesc,
//                         const void *y,
//                         const cudnnTensorDescriptor_t dyDesc,
//                         const void *dy,
//                         const cudnnTensorDescriptor_t xDesc,
//                         const void *x,
//                         const void *beta,
//                         const cudnnTensorDescriptor_t dxDesc,
//                         void *dx)
class ActivationBackward_for_op_grad_tanh : public OpEnv {
 public:
  DType dtype;
  cudnnActivationDescriptor_t activationDesc;
  cudnnTensorDescriptor_t yDesc;
  cudnnTensorDescriptor_t dyDesc;
  cudnnTensorDescriptor_t xDesc;
  cudnnTensorDescriptor_t dxDesc;

  ActivationBackward_for_op_grad_tanh(Array<Value> args, const OpInfo& info, Attrs attrs) {
    args.push_back(info->output);
    std::vector<const DLTensor*> dlts = AsVector(args);
    dtype = DeduceDLType(dlts);
    Context ctx = info->ctx;
    (void)ctx;

    CUDNN_CALL(cudnnCreateActivationDescriptor(&activationDesc));
    CUDNN_CALL(cudnnSetActivationDescriptor(
        activationDesc, ActivationModeEnum(ActivationModeEnum::ActivationTanh()),
        NanPropagationEnum(NanPropagationEnum::PropagateNan()), 0.0));
    FORM_SHAPE(shape_0, dlts[0]);
    FORM_STRIDE(stride_0, shape_0);
    CUDNN_CALL(cudnnCreateTensorDescriptor(&yDesc));
    CUDNN_CALL(cudnnSetTensorNdDescriptor(yDesc, CUDNNDType(dtype), shape_0.size(),
                                          BeginPtr(shape_0), BeginPtr(stride_0)));
    FORM_SHAPE(shape_1, dlts[1]);
    FORM_STRIDE(stride_1, shape_1);
    CUDNN_CALL(cudnnCreateTensorDescriptor(&dyDesc));
    CUDNN_CALL(cudnnSetTensorNdDescriptor(dyDesc, CUDNNDType(dtype), shape_1.size(),
                                          BeginPtr(shape_1), BeginPtr(stride_1)));
    FORM_SHAPE(shape_2, dlts[2]);
    FORM_STRIDE(stride_2, shape_2);
    CUDNN_CALL(cudnnCreateTensorDescriptor(&xDesc));
    CUDNN_CALL(cudnnSetTensorNdDescriptor(xDesc, CUDNNDType(dtype), shape_2.size(),
                                          BeginPtr(shape_2), BeginPtr(stride_2)));
    FORM_SHAPE(shape_3, dlts[3]);
    FORM_STRIDE(stride_3, shape_3);
    CUDNN_CALL(cudnnCreateTensorDescriptor(&dxDesc));
    CUDNN_CALL(cudnnSetTensorNdDescriptor(dxDesc, CUDNNDType(dtype), shape_3.size(),
                                          BeginPtr(shape_3), BeginPtr(stride_3)));
    RequestMemory(const_cast<void**>(&dlts[3]->data), ctx, BytesCompactTensor(*dlts[3]));
  }

  ~ActivationBackward_for_op_grad_tanh() {
    CUDNN_CALL(cudnnDestroyActivationDescriptor(activationDesc));
    CUDNN_CALL(cudnnDestroyTensorDescriptor(yDesc));
    CUDNN_CALL(cudnnDestroyTensorDescriptor(dyDesc));
    CUDNN_CALL(cudnnDestroyTensorDescriptor(xDesc));
    CUDNN_CALL(cudnnDestroyTensorDescriptor(dxDesc));
  }

  void Execute(Array<Value> args, const OpInfo& info, Attrs attrs) override final {
    args.push_back(info->output);
    std::vector<const DLTensor*> dlts = AsVector(args);
    CUDNN_CALL(cudnnActivationBackward(CUDNNThreadEntry::ThreadLocal()->handle, activationDesc,
                                       CUDNNDType(dtype).const_addr<1>(), yDesc, dlts[0]->data,
                                       dyDesc, dlts[1]->data, xDesc, dlts[2]->data,
                                       CUDNNDType(dtype).const_addr<0>(), dxDesc, dlts[3]->data));
    CUDA_CALL(cudaDeviceSynchronize());
  }

  static OpEnv* make(Array<Value> args, const OpInfo& info, Attrs attrs) {
    auto res = std::make_unique<ActivationBackward_for_op_grad_tanh>(args, info, attrs);
    return res.release();
  }
};

MNM_REGISTER_OP_DISPATCH("mnm.op.grad.tanh", DevType::kCUDA(), "generated_cudnn",
                         ActivationBackward_for_op_grad_tanh::make);

// Frontend operator "mnm.op.sigmoid" dispatch to
// cudnnActivationForward(cudnnHandle_t handle,
//                        cudnnActivationDescriptor_t activationDesc,
//                        const void *alpha,
//                        const cudnnTensorDescriptor_t xDesc,
//                        const void *x,
//                        const void *beta,
//                        const cudnnTensorDescriptor_t yDesc,
//                        void *y)
class ActivationForward_for_op_sigmoid : public OpEnv {
 public:
  DType dtype;
  cudnnActivationDescriptor_t activationDesc;
  cudnnTensorDescriptor_t xDesc;
  cudnnTensorDescriptor_t yDesc;

  ActivationForward_for_op_sigmoid(Array<Value> args, const OpInfo& info, Attrs attrs) {
    args.push_back(info->output);
    std::vector<const DLTensor*> dlts = AsVector(args);
    dtype = DeduceDLType(dlts);
    Context ctx = info->ctx;
    (void)ctx;

    CUDNN_CALL(cudnnCreateActivationDescriptor(&activationDesc));
    CUDNN_CALL(cudnnSetActivationDescriptor(
        activationDesc, ActivationModeEnum(ActivationModeEnum::ActivationSigmoid()),
        NanPropagationEnum(NanPropagationEnum::PropagateNan()), 0.0));
    FORM_SHAPE(shape_0, dlts[0]);
    FORM_STRIDE(stride_0, shape_0);
    CUDNN_CALL(cudnnCreateTensorDescriptor(&xDesc));
    CUDNN_CALL(cudnnSetTensorNdDescriptor(xDesc, CUDNNDType(dtype), shape_0.size(),
                                          BeginPtr(shape_0), BeginPtr(stride_0)));
    FORM_SHAPE(shape_1, dlts[1]);
    FORM_STRIDE(stride_1, shape_1);
    CUDNN_CALL(cudnnCreateTensorDescriptor(&yDesc));
    CUDNN_CALL(cudnnSetTensorNdDescriptor(yDesc, CUDNNDType(dtype), shape_1.size(),
                                          BeginPtr(shape_1), BeginPtr(stride_1)));
    RequestMemory(const_cast<void**>(&dlts[1]->data), ctx, BytesCompactTensor(*dlts[1]));
  }

  ~ActivationForward_for_op_sigmoid() {
    CUDNN_CALL(cudnnDestroyActivationDescriptor(activationDesc));
    CUDNN_CALL(cudnnDestroyTensorDescriptor(xDesc));
    CUDNN_CALL(cudnnDestroyTensorDescriptor(yDesc));
  }

  void Execute(Array<Value> args, const OpInfo& info, Attrs attrs) override final {
    args.push_back(info->output);
    std::vector<const DLTensor*> dlts = AsVector(args);
    CUDNN_CALL(cudnnActivationForward(CUDNNThreadEntry::ThreadLocal()->handle, activationDesc,
                                      CUDNNDType(dtype).const_addr<1>(), xDesc, dlts[0]->data,
                                      CUDNNDType(dtype).const_addr<0>(), yDesc, dlts[1]->data));
    CUDA_CALL(cudaDeviceSynchronize());
  }

  static OpEnv* make(Array<Value> args, const OpInfo& info, Attrs attrs) {
    auto res = std::make_unique<ActivationForward_for_op_sigmoid>(args, info, attrs);
    return res.release();
  }
};

MNM_REGISTER_OP_DISPATCH("mnm.op.sigmoid", DevType::kCUDA(), "generated_cudnn",
                         ActivationForward_for_op_sigmoid::make);

// Frontend operator "mnm.op.grad.sigmoid" dispatch to
// cudnnActivationBackward(cudnnHandle_t handle,
//                         cudnnActivationDescriptor_t activationDesc,
//                         const void *alpha,
//                         const cudnnTensorDescriptor_t yDesc,
//                         const void *y,
//                         const cudnnTensorDescriptor_t dyDesc,
//                         const void *dy,
//                         const cudnnTensorDescriptor_t xDesc,
//                         const void *x,
//                         const void *beta,
//                         const cudnnTensorDescriptor_t dxDesc,
//                         void *dx)
class ActivationBackward_for_op_grad_sigmoid : public OpEnv {
 public:
  DType dtype;
  cudnnActivationDescriptor_t activationDesc;
  cudnnTensorDescriptor_t yDesc;
  cudnnTensorDescriptor_t dyDesc;
  cudnnTensorDescriptor_t xDesc;
  cudnnTensorDescriptor_t dxDesc;

  ActivationBackward_for_op_grad_sigmoid(Array<Value> args, const OpInfo& info, Attrs attrs) {
    args.push_back(info->output);
    std::vector<const DLTensor*> dlts = AsVector(args);
    dtype = DeduceDLType(dlts);
    Context ctx = info->ctx;
    (void)ctx;

    CUDNN_CALL(cudnnCreateActivationDescriptor(&activationDesc));
    CUDNN_CALL(cudnnSetActivationDescriptor(
        activationDesc, ActivationModeEnum(ActivationModeEnum::ActivationSigmoid()),
        NanPropagationEnum(NanPropagationEnum::PropagateNan()), 0.0));
    FORM_SHAPE(shape_0, dlts[0]);
    FORM_STRIDE(stride_0, shape_0);
    CUDNN_CALL(cudnnCreateTensorDescriptor(&yDesc));
    CUDNN_CALL(cudnnSetTensorNdDescriptor(yDesc, CUDNNDType(dtype), shape_0.size(),
                                          BeginPtr(shape_0), BeginPtr(stride_0)));
    FORM_SHAPE(shape_1, dlts[1]);
    FORM_STRIDE(stride_1, shape_1);
    CUDNN_CALL(cudnnCreateTensorDescriptor(&dyDesc));
    CUDNN_CALL(cudnnSetTensorNdDescriptor(dyDesc, CUDNNDType(dtype), shape_1.size(),
                                          BeginPtr(shape_1), BeginPtr(stride_1)));
    FORM_SHAPE(shape_2, dlts[2]);
    FORM_STRIDE(stride_2, shape_2);
    CUDNN_CALL(cudnnCreateTensorDescriptor(&xDesc));
    CUDNN_CALL(cudnnSetTensorNdDescriptor(xDesc, CUDNNDType(dtype), shape_2.size(),
                                          BeginPtr(shape_2), BeginPtr(stride_2)));
    FORM_SHAPE(shape_3, dlts[3]);
    FORM_STRIDE(stride_3, shape_3);
    CUDNN_CALL(cudnnCreateTensorDescriptor(&dxDesc));
    CUDNN_CALL(cudnnSetTensorNdDescriptor(dxDesc, CUDNNDType(dtype), shape_3.size(),
                                          BeginPtr(shape_3), BeginPtr(stride_3)));
    RequestMemory(const_cast<void**>(&dlts[3]->data), ctx, BytesCompactTensor(*dlts[3]));
  }

  ~ActivationBackward_for_op_grad_sigmoid() {
    CUDNN_CALL(cudnnDestroyActivationDescriptor(activationDesc));
    CUDNN_CALL(cudnnDestroyTensorDescriptor(yDesc));
    CUDNN_CALL(cudnnDestroyTensorDescriptor(dyDesc));
    CUDNN_CALL(cudnnDestroyTensorDescriptor(xDesc));
    CUDNN_CALL(cudnnDestroyTensorDescriptor(dxDesc));
  }

  void Execute(Array<Value> args, const OpInfo& info, Attrs attrs) override final {
    args.push_back(info->output);
    std::vector<const DLTensor*> dlts = AsVector(args);
    CUDNN_CALL(cudnnActivationBackward(CUDNNThreadEntry::ThreadLocal()->handle, activationDesc,
                                       CUDNNDType(dtype).const_addr<1>(), yDesc, dlts[0]->data,
                                       dyDesc, dlts[1]->data, xDesc, dlts[2]->data,
                                       CUDNNDType(dtype).const_addr<0>(), dxDesc, dlts[3]->data));
    CUDA_CALL(cudaDeviceSynchronize());
  }

  static OpEnv* make(Array<Value> args, const OpInfo& info, Attrs attrs) {
    auto res = std::make_unique<ActivationBackward_for_op_grad_sigmoid>(args, info, attrs);
    return res.release();
  }
};

MNM_REGISTER_OP_DISPATCH("mnm.op.grad.sigmoid", DevType::kCUDA(), "generated_cudnn",
                         ActivationBackward_for_op_grad_sigmoid::make);

// Frontend operator "mnm.op.softmax" dispatch to
// cudnnSoftmaxForward(cudnnHandle_t handle,
//                     cudnnSoftmaxAlgorithm_t algo,
//                     cudnnSoftmaxMode_t mode,
//                     const void *alpha,
//                     const cudnnTensorDescriptor_t xDesc,
//                     const void *x,
//                     const void *beta,
//                     const cudnnTensorDescriptor_t yDesc,
//                     void *y)
class SoftmaxForward_for_op_softmax : public OpEnv {
 public:
  DType dtype;
  cudnnSoftmaxAlgorithm_t algo;
  cudnnSoftmaxMode_t mode;
  cudnnTensorDescriptor_t xDesc;
  cudnnTensorDescriptor_t yDesc;

  SoftmaxForward_for_op_softmax(Array<Value> args, const OpInfo& info, Attrs attrs) {
    args.push_back(info->output);
    std::vector<const DLTensor*> dlts = AsVector(args);
    dtype = DeduceDLType(dlts);
    Context ctx = info->ctx;
    (void)ctx;
    auto casted_ptr = attrs.as<attrs::SoftmaxAttrs>();
    (void)casted_ptr;

    int axis = casted_ptr->axis;
    int left = 1, center = dlts[0]->shape[axis], right = 1;
    for (int i = 0; i < axis; ++i) {
      left *= dlts[0]->shape[i];
    }
    for (int i = axis + 1; i < dlts[0]->ndim; ++i) {
      right *= dlts[0]->shape[i];
    }

    algo = SoftmaxAlgorithmEnum(SoftmaxAlgorithmEnum::SoftmaxAccurate());
    mode = center == 1 && right == 1 ? SoftmaxModeEnum(SoftmaxModeEnum::SoftmaxModeInstance())
                                     : SoftmaxModeEnum(SoftmaxModeEnum::SoftmaxModeChannel());
    std::vector<int> shape_0{left, center, right, 1};
    FORM_STRIDE(stride_0, shape_0);
    CUDNN_CALL(cudnnCreateTensorDescriptor(&xDesc));
    CUDNN_CALL(cudnnSetTensorNdDescriptor(xDesc, CUDNNDType(dtype), shape_0.size(),
                                          BeginPtr(shape_0), BeginPtr(stride_0)));
    std::vector<int> shape_1{left, center, right, 1};
    FORM_STRIDE(stride_1, shape_1);
    CUDNN_CALL(cudnnCreateTensorDescriptor(&yDesc));
    CUDNN_CALL(cudnnSetTensorNdDescriptor(yDesc, CUDNNDType(dtype), shape_1.size(),
                                          BeginPtr(shape_1), BeginPtr(stride_1)));
    RequestMemory(const_cast<void**>(&dlts[1]->data), ctx, BytesCompactTensor(*dlts[1]));
  }

  ~SoftmaxForward_for_op_softmax() {
    CUDNN_CALL(cudnnDestroyTensorDescriptor(xDesc));
    CUDNN_CALL(cudnnDestroyTensorDescriptor(yDesc));
  }

  void Execute(Array<Value> args, const OpInfo& info, Attrs attrs) override final {
    args.push_back(info->output);
    std::vector<const DLTensor*> dlts = AsVector(args);
    auto casted_ptr = attrs.as<attrs::SoftmaxAttrs>();
    (void)casted_ptr;
    CUDNN_CALL(cudnnSoftmaxForward(CUDNNThreadEntry::ThreadLocal()->handle, algo, mode,
                                   CUDNNDType(dtype).const_addr<1>(), xDesc, dlts[0]->data,
                                   CUDNNDType(dtype).const_addr<0>(), yDesc, dlts[1]->data));
    CUDA_CALL(cudaDeviceSynchronize());
  }

  static OpEnv* make(Array<Value> args, const OpInfo& info, Attrs attrs) {
    auto res = std::make_unique<SoftmaxForward_for_op_softmax>(args, info, attrs);
    return res.release();
  }
};

MNM_REGISTER_OP_DISPATCH("mnm.op.softmax", DevType::kCUDA(), "generated_cudnn",
                         SoftmaxForward_for_op_softmax::make);

// Frontend operator "mnm.op.log_softmax" dispatch to
// cudnnSoftmaxForward(cudnnHandle_t handle,
//                     cudnnSoftmaxAlgorithm_t algo,
//                     cudnnSoftmaxMode_t mode,
//                     const void *alpha,
//                     const cudnnTensorDescriptor_t xDesc,
//                     const void *x,
//                     const void *beta,
//                     const cudnnTensorDescriptor_t yDesc,
//                     void *y)
class SoftmaxForward_for_op_log_softmax : public OpEnv {
 public:
  DType dtype;
  cudnnSoftmaxAlgorithm_t algo;
  cudnnSoftmaxMode_t mode;
  cudnnTensorDescriptor_t xDesc;
  cudnnTensorDescriptor_t yDesc;

  SoftmaxForward_for_op_log_softmax(Array<Value> args, const OpInfo& info, Attrs attrs) {
    args.push_back(info->output);
    std::vector<const DLTensor*> dlts = AsVector(args);
    dtype = DeduceDLType(dlts);
    Context ctx = info->ctx;
    (void)ctx;
    auto casted_ptr = attrs.as<attrs::SoftmaxAttrs>();
    (void)casted_ptr;

    int axis = casted_ptr->axis;
    int left = 1, center = dlts[0]->shape[axis], right = 1;
    for (int i = 0; i < axis; ++i) {
      left *= dlts[0]->shape[i];
    }
    for (int i = axis + 1; i < dlts[0]->ndim; ++i) {
      right *= dlts[0]->shape[i];
    }

    algo = SoftmaxAlgorithmEnum(SoftmaxAlgorithmEnum::SoftmaxLog());
    mode = center == 1 && right == 1 ? SoftmaxModeEnum(SoftmaxModeEnum::SoftmaxModeInstance())
                                     : SoftmaxModeEnum(SoftmaxModeEnum::SoftmaxModeChannel());
    std::vector<int> shape_0{left, center, right, 1};
    FORM_STRIDE(stride_0, shape_0);
    CUDNN_CALL(cudnnCreateTensorDescriptor(&xDesc));
    CUDNN_CALL(cudnnSetTensorNdDescriptor(xDesc, CUDNNDType(dtype), shape_0.size(),
                                          BeginPtr(shape_0), BeginPtr(stride_0)));
    std::vector<int> shape_1{left, center, right, 1};
    FORM_STRIDE(stride_1, shape_1);
    CUDNN_CALL(cudnnCreateTensorDescriptor(&yDesc));
    CUDNN_CALL(cudnnSetTensorNdDescriptor(yDesc, CUDNNDType(dtype), shape_1.size(),
                                          BeginPtr(shape_1), BeginPtr(stride_1)));
    RequestMemory(const_cast<void**>(&dlts[1]->data), ctx, BytesCompactTensor(*dlts[1]));
  }

  ~SoftmaxForward_for_op_log_softmax() {
    CUDNN_CALL(cudnnDestroyTensorDescriptor(xDesc));
    CUDNN_CALL(cudnnDestroyTensorDescriptor(yDesc));
  }

  void Execute(Array<Value> args, const OpInfo& info, Attrs attrs) override final {
    args.push_back(info->output);
    std::vector<const DLTensor*> dlts = AsVector(args);
    auto casted_ptr = attrs.as<attrs::SoftmaxAttrs>();
    (void)casted_ptr;
    CUDNN_CALL(cudnnSoftmaxForward(CUDNNThreadEntry::ThreadLocal()->handle, algo, mode,
                                   CUDNNDType(dtype).const_addr<1>(), xDesc, dlts[0]->data,
                                   CUDNNDType(dtype).const_addr<0>(), yDesc, dlts[1]->data));
    CUDA_CALL(cudaDeviceSynchronize());
  }

  static OpEnv* make(Array<Value> args, const OpInfo& info, Attrs attrs) {
    auto res = std::make_unique<SoftmaxForward_for_op_log_softmax>(args, info, attrs);
    return res.release();
  }
};

MNM_REGISTER_OP_DISPATCH("mnm.op.log_softmax", DevType::kCUDA(), "generated_cudnn",
                         SoftmaxForward_for_op_log_softmax::make);

// Frontend operator "mnm.op.grad.softmax" dispatch to
// cudnnSoftmaxBackward(cudnnHandle_t handle,
//                      cudnnSoftmaxAlgorithm_t algo,
//                      cudnnSoftmaxMode_t mode,
//                      const void *alpha,
//                      const cudnnTensorDescriptor_t yDesc,
//                      const void *y,
//                      const cudnnTensorDescriptor_t dyDesc,
//                      const void *dy,
//                      const void *beta,
//                      const cudnnTensorDescriptor_t dxDesc,
//                      void *dx)
class SoftmaxBackward_for_op_grad_softmax : public OpEnv {
 public:
  DType dtype;
  cudnnSoftmaxAlgorithm_t algo;
  cudnnSoftmaxMode_t mode;
  cudnnTensorDescriptor_t yDesc;
  cudnnTensorDescriptor_t dyDesc;
  cudnnTensorDescriptor_t dxDesc;

  SoftmaxBackward_for_op_grad_softmax(Array<Value> args, const OpInfo& info, Attrs attrs) {
    args.push_back(info->output);
    std::vector<const DLTensor*> dlts = AsVector(args);
    dtype = DeduceDLType(dlts);
    Context ctx = info->ctx;
    (void)ctx;
    auto casted_ptr = attrs.as<attrs::SoftmaxAttrs>();
    (void)casted_ptr;

    algo = SoftmaxAlgorithmEnum(SoftmaxAlgorithmEnum::SoftmaxAccurate());
    mode = casted_ptr->axis == 0 ? SoftmaxModeEnum(SoftmaxModeEnum::SoftmaxModeInstance())
                                 : SoftmaxModeEnum(SoftmaxModeEnum::SoftmaxModeChannel());
    FORM_SHAPE(shape_0, dlts[0]);
    FORM_STRIDE(stride_0, shape_0);
    CUDNN_CALL(cudnnCreateTensorDescriptor(&yDesc));
    CUDNN_CALL(cudnnSetTensorNdDescriptor(yDesc, CUDNNDType(dtype), shape_0.size(),
                                          BeginPtr(shape_0), BeginPtr(stride_0)));
    FORM_SHAPE(shape_1, dlts[1]);
    FORM_STRIDE(stride_1, shape_1);
    CUDNN_CALL(cudnnCreateTensorDescriptor(&dyDesc));
    CUDNN_CALL(cudnnSetTensorNdDescriptor(dyDesc, CUDNNDType(dtype), shape_1.size(),
                                          BeginPtr(shape_1), BeginPtr(stride_1)));
    FORM_SHAPE(shape_2, dlts[2]);
    FORM_STRIDE(stride_2, shape_2);
    CUDNN_CALL(cudnnCreateTensorDescriptor(&dxDesc));
    CUDNN_CALL(cudnnSetTensorNdDescriptor(dxDesc, CUDNNDType(dtype), shape_2.size(),
                                          BeginPtr(shape_2), BeginPtr(stride_2)));
    RequestMemory(const_cast<void**>(&dlts[2]->data), ctx, BytesCompactTensor(*dlts[2]));
  }

  ~SoftmaxBackward_for_op_grad_softmax() {
    CUDNN_CALL(cudnnDestroyTensorDescriptor(yDesc));
    CUDNN_CALL(cudnnDestroyTensorDescriptor(dyDesc));
    CUDNN_CALL(cudnnDestroyTensorDescriptor(dxDesc));
  }

  void Execute(Array<Value> args, const OpInfo& info, Attrs attrs) override final {
    args.push_back(info->output);
    std::vector<const DLTensor*> dlts = AsVector(args);
    auto casted_ptr = attrs.as<attrs::SoftmaxAttrs>();
    (void)casted_ptr;
    CUDNN_CALL(cudnnSoftmaxBackward(CUDNNThreadEntry::ThreadLocal()->handle, algo, mode,
                                    CUDNNDType(dtype).const_addr<1>(), yDesc, dlts[0]->data, dyDesc,
                                    dlts[1]->data, CUDNNDType(dtype).const_addr<0>(), dxDesc,
                                    dlts[2]->data));
    CUDA_CALL(cudaDeviceSynchronize());
  }

  static OpEnv* make(Array<Value> args, const OpInfo& info, Attrs attrs) {
    auto res = std::make_unique<SoftmaxBackward_for_op_grad_softmax>(args, info, attrs);
    return res.release();
  }
};

MNM_REGISTER_OP_DISPATCH("mnm.op.grad.softmax", DevType::kCUDA(), "generated_cudnn",
                         SoftmaxBackward_for_op_grad_softmax::make);

// Frontend operator "mnm.op.max_pool2d" dispatch to
// cudnnPoolingForward(cudnnHandle_t handle,
//                     const cudnnPoolingDescriptor_t poolingDesc,
//                     const void *alpha,
//                     const cudnnTensorDescriptor_t xDesc,
//                     const void *x,
//                     const void *beta,
//                     const cudnnTensorDescriptor_t yDesc,
//                     void *y)
class PoolingForward_for_op_max_pool2d : public OpEnv {
 public:
  DType dtype;
  cudnnPoolingDescriptor_t poolingDesc;
  cudnnTensorDescriptor_t xDesc;
  cudnnTensorDescriptor_t yDesc;

  PoolingForward_for_op_max_pool2d(Array<Value> args, const OpInfo& info, Attrs attrs) {
    args.push_back(info->output);
    std::vector<const DLTensor*> dlts = AsVector(args);
    dtype = DeduceDLType(dlts);
    Context ctx = info->ctx;
    (void)ctx;
    auto casted_ptr = attrs.as<attrs::MaxPoolAttrs>();
    (void)casted_ptr;

    CUDNN_CALL(cudnnCreatePoolingDescriptor(&poolingDesc));
    CUDNN_CALL(cudnnSetPoolingNdDescriptor(
        poolingDesc, PoolingModeEnum(PoolingModeEnum::PoolingMax()),
        NanPropagationEnum(NanPropagationEnum::PropagateNan()), casted_ptr->kernel.size(),
        BeginPtr(MakeShape<int>(casted_ptr->kernel)), BeginPtr(MakeShape<int>(casted_ptr->padding)),
        BeginPtr(MakeShape<int>(casted_ptr->stride))));
    FORM_SHAPE(shape_0, dlts[0]);
    FORM_STRIDE(stride_0, shape_0);
    CUDNN_CALL(cudnnCreateTensorDescriptor(&xDesc));
    CUDNN_CALL(cudnnSetTensorNdDescriptor(xDesc, CUDNNDType(dtype), shape_0.size(),
                                          BeginPtr(shape_0), BeginPtr(stride_0)));
    FORM_SHAPE(shape_1, dlts[1]);
    FORM_STRIDE(stride_1, shape_1);
    CUDNN_CALL(cudnnCreateTensorDescriptor(&yDesc));
    CUDNN_CALL(cudnnSetTensorNdDescriptor(yDesc, CUDNNDType(dtype), shape_1.size(),
                                          BeginPtr(shape_1), BeginPtr(stride_1)));
    RequestMemory(const_cast<void**>(&dlts[1]->data), ctx, BytesCompactTensor(*dlts[1]));
  }

  ~PoolingForward_for_op_max_pool2d() {
    CUDNN_CALL(cudnnDestroyPoolingDescriptor(poolingDesc));
    CUDNN_CALL(cudnnDestroyTensorDescriptor(xDesc));
    CUDNN_CALL(cudnnDestroyTensorDescriptor(yDesc));
  }

  void Execute(Array<Value> args, const OpInfo& info, Attrs attrs) override final {
    args.push_back(info->output);
    std::vector<const DLTensor*> dlts = AsVector(args);
    auto casted_ptr = attrs.as<attrs::MaxPoolAttrs>();
    (void)casted_ptr;
    CUDNN_CALL(cudnnPoolingForward(CUDNNThreadEntry::ThreadLocal()->handle, poolingDesc,
                                   CUDNNDType(dtype).const_addr<1>(), xDesc, dlts[0]->data,
                                   CUDNNDType(dtype).const_addr<0>(), yDesc, dlts[1]->data));
    CUDA_CALL(cudaDeviceSynchronize());
  }

  static OpEnv* make(Array<Value> args, const OpInfo& info, Attrs attrs) {
    auto res = std::make_unique<PoolingForward_for_op_max_pool2d>(args, info, attrs);
    return res.release();
  }
};

MNM_REGISTER_OP_DISPATCH("mnm.op.max_pool2d", DevType::kCUDA(), "generated_cudnn",
                         PoolingForward_for_op_max_pool2d::make);

// Frontend operator "mnm.op.grad.max_pool2d" dispatch to
// cudnnPoolingBackward(cudnnHandle_t handle,
//                      const cudnnPoolingDescriptor_t poolingDesc,
//                      const void *alpha,
//                      const cudnnTensorDescriptor_t yDesc,
//                      const void *y,
//                      const cudnnTensorDescriptor_t dyDesc,
//                      const void *dy,
//                      const cudnnTensorDescriptor_t xDesc,
//                      const void *x,
//                      const void *beta,
//                      const cudnnTensorDescriptor_t dxDesc,
//                      void *dx)
class PoolingBackward_for_op_grad_max_pool2d : public OpEnv {
 public:
  DType dtype;
  cudnnPoolingDescriptor_t poolingDesc;
  cudnnTensorDescriptor_t yDesc;
  cudnnTensorDescriptor_t dyDesc;
  cudnnTensorDescriptor_t xDesc;
  cudnnTensorDescriptor_t dxDesc;

  PoolingBackward_for_op_grad_max_pool2d(Array<Value> args, const OpInfo& info, Attrs attrs) {
    args.push_back(info->output);
    std::vector<const DLTensor*> dlts = AsVector(args);
    dtype = DeduceDLType(dlts);
    Context ctx = info->ctx;
    (void)ctx;
    auto casted_ptr = attrs.as<attrs::MaxPoolAttrs>();
    (void)casted_ptr;

    CUDNN_CALL(cudnnCreatePoolingDescriptor(&poolingDesc));
    CUDNN_CALL(cudnnSetPoolingNdDescriptor(
        poolingDesc, PoolingModeEnum(PoolingModeEnum::PoolingMax()),
        NanPropagationEnum(NanPropagationEnum::PropagateNan()), casted_ptr->kernel.size(),
        BeginPtr(MakeShape<int>(casted_ptr->kernel)), BeginPtr(MakeShape<int>(casted_ptr->padding)),
        BeginPtr(MakeShape<int>(casted_ptr->stride))));
    FORM_SHAPE(shape_0, dlts[0]);
    FORM_STRIDE(stride_0, shape_0);
    CUDNN_CALL(cudnnCreateTensorDescriptor(&yDesc));
    CUDNN_CALL(cudnnSetTensorNdDescriptor(yDesc, CUDNNDType(dtype), shape_0.size(),
                                          BeginPtr(shape_0), BeginPtr(stride_0)));
    FORM_SHAPE(shape_1, dlts[1]);
    FORM_STRIDE(stride_1, shape_1);
    CUDNN_CALL(cudnnCreateTensorDescriptor(&dyDesc));
    CUDNN_CALL(cudnnSetTensorNdDescriptor(dyDesc, CUDNNDType(dtype), shape_1.size(),
                                          BeginPtr(shape_1), BeginPtr(stride_1)));
    FORM_SHAPE(shape_2, dlts[2]);
    FORM_STRIDE(stride_2, shape_2);
    CUDNN_CALL(cudnnCreateTensorDescriptor(&xDesc));
    CUDNN_CALL(cudnnSetTensorNdDescriptor(xDesc, CUDNNDType(dtype), shape_2.size(),
                                          BeginPtr(shape_2), BeginPtr(stride_2)));
    FORM_SHAPE(shape_3, dlts[3]);
    FORM_STRIDE(stride_3, shape_3);
    CUDNN_CALL(cudnnCreateTensorDescriptor(&dxDesc));
    CUDNN_CALL(cudnnSetTensorNdDescriptor(dxDesc, CUDNNDType(dtype), shape_3.size(),
                                          BeginPtr(shape_3), BeginPtr(stride_3)));
    RequestMemory(const_cast<void**>(&dlts[3]->data), ctx, BytesCompactTensor(*dlts[3]));
  }

  ~PoolingBackward_for_op_grad_max_pool2d() {
    CUDNN_CALL(cudnnDestroyPoolingDescriptor(poolingDesc));
    CUDNN_CALL(cudnnDestroyTensorDescriptor(yDesc));
    CUDNN_CALL(cudnnDestroyTensorDescriptor(dyDesc));
    CUDNN_CALL(cudnnDestroyTensorDescriptor(xDesc));
    CUDNN_CALL(cudnnDestroyTensorDescriptor(dxDesc));
  }

  void Execute(Array<Value> args, const OpInfo& info, Attrs attrs) override final {
    args.push_back(info->output);
    std::vector<const DLTensor*> dlts = AsVector(args);
    auto casted_ptr = attrs.as<attrs::MaxPoolAttrs>();
    (void)casted_ptr;
    CUDNN_CALL(cudnnPoolingBackward(CUDNNThreadEntry::ThreadLocal()->handle, poolingDesc,
                                    CUDNNDType(dtype).const_addr<1>(), yDesc, dlts[0]->data, dyDesc,
                                    dlts[1]->data, xDesc, dlts[2]->data,
                                    CUDNNDType(dtype).const_addr<0>(), dxDesc, dlts[3]->data));
    CUDA_CALL(cudaDeviceSynchronize());
  }

  static OpEnv* make(Array<Value> args, const OpInfo& info, Attrs attrs) {
    auto res = std::make_unique<PoolingBackward_for_op_grad_max_pool2d>(args, info, attrs);
    return res.release();
  }
};

MNM_REGISTER_OP_DISPATCH("mnm.op.grad.max_pool2d", DevType::kCUDA(), "generated_cudnn",
                         PoolingBackward_for_op_grad_max_pool2d::make);

// Frontend operator "mnm.op.avg_pool2d" dispatch to
// cudnnPoolingForward(cudnnHandle_t handle,
//                     const cudnnPoolingDescriptor_t poolingDesc,
//                     const void *alpha,
//                     const cudnnTensorDescriptor_t xDesc,
//                     const void *x,
//                     const void *beta,
//                     const cudnnTensorDescriptor_t yDesc,
//                     void *y)
class PoolingForward_for_op_avg_pool2d : public OpEnv {
 public:
  DType dtype;
  cudnnPoolingDescriptor_t poolingDesc;
  cudnnTensorDescriptor_t xDesc;
  cudnnTensorDescriptor_t yDesc;

  PoolingForward_for_op_avg_pool2d(Array<Value> args, const OpInfo& info, Attrs attrs) {
    args.push_back(info->output);
    std::vector<const DLTensor*> dlts = AsVector(args);
    dtype = DeduceDLType(dlts);
    Context ctx = info->ctx;
    (void)ctx;
    auto casted_ptr = attrs.as<attrs::AvgPoolAttrs>();
    (void)casted_ptr;

    CUDNN_CALL(cudnnCreatePoolingDescriptor(&poolingDesc));
    CUDNN_CALL(cudnnSetPoolingNdDescriptor(
        poolingDesc,
        casted_ptr->include_pad
            ? PoolingModeEnum(PoolingModeEnum::PoolingAverageCountIncludePadding())
            : PoolingModeEnum(PoolingModeEnum::PoolingAverageCountExcludePadding()),
        NanPropagationEnum(NanPropagationEnum::PropagateNan()), casted_ptr->kernel.size(),
        BeginPtr(MakeShape<int>(casted_ptr->kernel)), BeginPtr(MakeShape<int>(casted_ptr->padding)),
        BeginPtr(MakeShape<int>(casted_ptr->stride))));
    FORM_SHAPE(shape_0, dlts[0]);
    FORM_STRIDE(stride_0, shape_0);
    CUDNN_CALL(cudnnCreateTensorDescriptor(&xDesc));
    CUDNN_CALL(cudnnSetTensorNdDescriptor(xDesc, CUDNNDType(dtype), shape_0.size(),
                                          BeginPtr(shape_0), BeginPtr(stride_0)));
    FORM_SHAPE(shape_1, dlts[1]);
    FORM_STRIDE(stride_1, shape_1);
    CUDNN_CALL(cudnnCreateTensorDescriptor(&yDesc));
    CUDNN_CALL(cudnnSetTensorNdDescriptor(yDesc, CUDNNDType(dtype), shape_1.size(),
                                          BeginPtr(shape_1), BeginPtr(stride_1)));
    RequestMemory(const_cast<void**>(&dlts[1]->data), ctx, BytesCompactTensor(*dlts[1]));
  }

  ~PoolingForward_for_op_avg_pool2d() {
    CUDNN_CALL(cudnnDestroyPoolingDescriptor(poolingDesc));
    CUDNN_CALL(cudnnDestroyTensorDescriptor(xDesc));
    CUDNN_CALL(cudnnDestroyTensorDescriptor(yDesc));
  }

  void Execute(Array<Value> args, const OpInfo& info, Attrs attrs) override final {
    args.push_back(info->output);
    std::vector<const DLTensor*> dlts = AsVector(args);
    auto casted_ptr = attrs.as<attrs::AvgPoolAttrs>();
    (void)casted_ptr;
    CUDNN_CALL(cudnnPoolingForward(CUDNNThreadEntry::ThreadLocal()->handle, poolingDesc,
                                   CUDNNDType(dtype).const_addr<1>(), xDesc, dlts[0]->data,
                                   CUDNNDType(dtype).const_addr<0>(), yDesc, dlts[1]->data));
    CUDA_CALL(cudaDeviceSynchronize());
  }

  static OpEnv* make(Array<Value> args, const OpInfo& info, Attrs attrs) {
    auto res = std::make_unique<PoolingForward_for_op_avg_pool2d>(args, info, attrs);
    return res.release();
  }
};

MNM_REGISTER_OP_DISPATCH("mnm.op.avg_pool2d", DevType::kCUDA(), "generated_cudnn",
                         PoolingForward_for_op_avg_pool2d::make);

// Frontend operator "mnm.op.grad.avg_pool2d" dispatch to
// cudnnPoolingBackward(cudnnHandle_t handle,
//                      const cudnnPoolingDescriptor_t poolingDesc,
//                      const void *alpha,
//                      const cudnnTensorDescriptor_t yDesc,
//                      const void *y,
//                      const cudnnTensorDescriptor_t dyDesc,
//                      const void *dy,
//                      const cudnnTensorDescriptor_t xDesc,
//                      const void *x,
//                      const void *beta,
//                      const cudnnTensorDescriptor_t dxDesc,
//                      void *dx)
class PoolingBackward_for_op_grad_avg_pool2d : public OpEnv {
 public:
  DType dtype;
  cudnnPoolingDescriptor_t poolingDesc;
  cudnnTensorDescriptor_t yDesc;
  cudnnTensorDescriptor_t dyDesc;
  cudnnTensorDescriptor_t xDesc;
  cudnnTensorDescriptor_t dxDesc;

  PoolingBackward_for_op_grad_avg_pool2d(Array<Value> args, const OpInfo& info, Attrs attrs) {
    args.push_back(info->output);
    std::vector<const DLTensor*> dlts = AsVector(args);
    dtype = DeduceDLType(dlts);
    Context ctx = info->ctx;
    (void)ctx;
    auto casted_ptr = attrs.as<attrs::AvgPoolAttrs>();
    (void)casted_ptr;

    CUDNN_CALL(cudnnCreatePoolingDescriptor(&poolingDesc));
    CUDNN_CALL(cudnnSetPoolingNdDescriptor(
        poolingDesc,
        casted_ptr->include_pad
            ? PoolingModeEnum(PoolingModeEnum::PoolingAverageCountIncludePadding())
            : PoolingModeEnum(PoolingModeEnum::PoolingAverageCountExcludePadding()),
        NanPropagationEnum(NanPropagationEnum::PropagateNan()), casted_ptr->kernel.size(),
        BeginPtr(MakeShape<int>(casted_ptr->kernel)), BeginPtr(MakeShape<int>(casted_ptr->padding)),
        BeginPtr(MakeShape<int>(casted_ptr->stride))));
    FORM_SHAPE(shape_0, dlts[0]);
    FORM_STRIDE(stride_0, shape_0);
    CUDNN_CALL(cudnnCreateTensorDescriptor(&yDesc));
    CUDNN_CALL(cudnnSetTensorNdDescriptor(yDesc, CUDNNDType(dtype), shape_0.size(),
                                          BeginPtr(shape_0), BeginPtr(stride_0)));
    FORM_SHAPE(shape_1, dlts[1]);
    FORM_STRIDE(stride_1, shape_1);
    CUDNN_CALL(cudnnCreateTensorDescriptor(&dyDesc));
    CUDNN_CALL(cudnnSetTensorNdDescriptor(dyDesc, CUDNNDType(dtype), shape_1.size(),
                                          BeginPtr(shape_1), BeginPtr(stride_1)));
    FORM_SHAPE(shape_2, dlts[2]);
    FORM_STRIDE(stride_2, shape_2);
    CUDNN_CALL(cudnnCreateTensorDescriptor(&xDesc));
    CUDNN_CALL(cudnnSetTensorNdDescriptor(xDesc, CUDNNDType(dtype), shape_2.size(),
                                          BeginPtr(shape_2), BeginPtr(stride_2)));
    FORM_SHAPE(shape_3, dlts[3]);
    FORM_STRIDE(stride_3, shape_3);
    CUDNN_CALL(cudnnCreateTensorDescriptor(&dxDesc));
    CUDNN_CALL(cudnnSetTensorNdDescriptor(dxDesc, CUDNNDType(dtype), shape_3.size(),
                                          BeginPtr(shape_3), BeginPtr(stride_3)));
    RequestMemory(const_cast<void**>(&dlts[3]->data), ctx, BytesCompactTensor(*dlts[3]));
  }

  ~PoolingBackward_for_op_grad_avg_pool2d() {
    CUDNN_CALL(cudnnDestroyPoolingDescriptor(poolingDesc));
    CUDNN_CALL(cudnnDestroyTensorDescriptor(yDesc));
    CUDNN_CALL(cudnnDestroyTensorDescriptor(dyDesc));
    CUDNN_CALL(cudnnDestroyTensorDescriptor(xDesc));
    CUDNN_CALL(cudnnDestroyTensorDescriptor(dxDesc));
  }

  void Execute(Array<Value> args, const OpInfo& info, Attrs attrs) override final {
    args.push_back(info->output);
    std::vector<const DLTensor*> dlts = AsVector(args);
    auto casted_ptr = attrs.as<attrs::AvgPoolAttrs>();
    (void)casted_ptr;
    CUDNN_CALL(cudnnPoolingBackward(CUDNNThreadEntry::ThreadLocal()->handle, poolingDesc,
                                    CUDNNDType(dtype).const_addr<1>(), yDesc, dlts[0]->data, dyDesc,
                                    dlts[1]->data, xDesc, dlts[2]->data,
                                    CUDNNDType(dtype).const_addr<0>(), dxDesc, dlts[3]->data));
    CUDA_CALL(cudaDeviceSynchronize());
  }

  static OpEnv* make(Array<Value> args, const OpInfo& info, Attrs attrs) {
    auto res = std::make_unique<PoolingBackward_for_op_grad_avg_pool2d>(args, info, attrs);
    return res.release();
  }
};

MNM_REGISTER_OP_DISPATCH("mnm.op.grad.avg_pool2d", DevType::kCUDA(), "generated_cudnn",
                         PoolingBackward_for_op_grad_avg_pool2d::make);

// Frontend operator "mnm.op.dropout" dispatch to
// cudnnDropoutForward(cudnnHandle_t handle,
//                     const cudnnDropoutDescriptor_t dropoutDesc,
//                     const cudnnTensorDescriptor_t xdesc,
//                     const void *x,
//                     const cudnnTensorDescriptor_t ydesc,
//                     void *y,
//                     void *reserveSpace,
//                     size_t reserveSpaceSizeInBytes)
class DropoutForward_for_op_dropout : public OpEnv {
 public:
  DType dtype;
  cudnnDropoutDescriptor_t dropoutDesc;
  cudnnTensorDescriptor_t xdesc;
  cudnnTensorDescriptor_t ydesc;

  DropoutForward_for_op_dropout(Array<Value> args, const OpInfo& info, Attrs attrs) {
    args.push_back(info->output);

    int n = args.size();
    std::vector<const DLTensor*> dlts(n);
    for (int i = 0; i < n - 1; ++i) {
      dlts[i] = args[i];
    }
    value::TupleValue tv = Downcast<TupleValue>(args[n - 1]);
    dlts[n - 1] = tv->fields[0];
    std::vector<OpaqueValue> opqs{Downcast<OpaqueValue>(tv->fields[1]),
                                  Downcast<OpaqueValue>(tv->fields[2])};

    dtype = DeduceDLType(dlts);
    Context ctx = info->ctx;
    (void)ctx;
    auto casted_ptr = attrs.as<attrs::DropoutAttrs>();
    (void)casted_ptr;

    CUDNN_CALL(cudnnCreateDropoutDescriptor(&dropoutDesc));
    opqs[0]->data = BufferValue(make_node<BufferNode>());
    Downcast<BufferValue>(opqs[0]->data)->size_in_bytes = DropoutGetStatesSize();
    RequestMemory(const_cast<void**>(&Downcast<BufferValue>(opqs[0]->data)->data), ctx,
                  Downcast<BufferValue>(opqs[0]->data)->size_in_bytes);
    CUDNN_CALL(cudnnSetDropoutDescriptor(
        dropoutDesc, CUDNNThreadEntry::ThreadLocal()->handle, casted_ptr->dropout,
        Downcast<BufferValue>(opqs[0]->data)->data,
        Downcast<BufferValue>(opqs[0]->data)->size_in_bytes, casted_ptr->seed));
    FORM_SHAPE(shape_0, dlts[0]);
    FORM_STRIDE(stride_0, shape_0);
    CUDNN_CALL(cudnnCreateTensorDescriptor(&xdesc));
    CUDNN_CALL(cudnnSetTensorNdDescriptor(xdesc, CUDNNDType(dtype), shape_0.size(),
                                          BeginPtr(shape_0), BeginPtr(stride_0)));
    FORM_SHAPE(shape_1, dlts[1]);
    FORM_STRIDE(stride_1, shape_1);
    CUDNN_CALL(cudnnCreateTensorDescriptor(&ydesc));
    CUDNN_CALL(cudnnSetTensorNdDescriptor(ydesc, CUDNNDType(dtype), shape_1.size(),
                                          BeginPtr(shape_1), BeginPtr(stride_1)));
    RequestMemory(const_cast<void**>(&dlts[1]->data), ctx, BytesCompactTensor(*dlts[1]));
    opqs[1]->data = BufferValue(make_node<BufferNode>());
    Downcast<BufferValue>(opqs[1]->data)->size_in_bytes = DropoutGetReserveSpaceSize(xdesc);
    RequestMemory(const_cast<void**>(&Downcast<BufferValue>(opqs[1]->data)->data), ctx,
                  Downcast<BufferValue>(opqs[1]->data)->size_in_bytes);
  }

  ~DropoutForward_for_op_dropout() {
    CUDNN_CALL(cudnnDestroyDropoutDescriptor(dropoutDesc));
    CUDNN_CALL(cudnnDestroyTensorDescriptor(xdesc));
    CUDNN_CALL(cudnnDestroyTensorDescriptor(ydesc));
  }

  void Execute(Array<Value> args, const OpInfo& info, Attrs attrs) override final {
    args.push_back(info->output);

    int n = args.size();
    std::vector<const DLTensor*> dlts(n);
    for (int i = 0; i < n - 1; ++i) {
      dlts[i] = args[i];
    }
    value::TupleValue tv = Downcast<TupleValue>(args[n - 1]);
    dlts[n - 1] = tv->fields[0];
    std::vector<OpaqueValue> opqs{Downcast<OpaqueValue>(tv->fields[1]),
                                  Downcast<OpaqueValue>(tv->fields[2])};

    auto casted_ptr = attrs.as<attrs::DropoutAttrs>();
    (void)casted_ptr;
    CUDNN_CALL(cudnnDropoutForward(CUDNNThreadEntry::ThreadLocal()->handle, dropoutDesc, xdesc,
                                   dlts[0]->data, ydesc, dlts[1]->data,
                                   Downcast<BufferValue>(opqs[1]->data)->data,
                                   Downcast<BufferValue>(opqs[1]->data)->size_in_bytes));
    CUDA_CALL(cudaDeviceSynchronize());
  }

  static OpEnv* make(Array<Value> args, const OpInfo& info, Attrs attrs) {
    auto res = std::make_unique<DropoutForward_for_op_dropout>(args, info, attrs);
    return res.release();
  }
};

MNM_REGISTER_OP_DISPATCH("mnm.op.dropout", DevType::kCUDA(), "generated_cudnn",
                         DropoutForward_for_op_dropout::make);

// Frontend operator "mnm.op.grad.dropout" dispatch to
// cudnnDropoutBackward(cudnnHandle_t handle,
//                      const cudnnDropoutDescriptor_t dropoutDesc,
//                      const cudnnTensorDescriptor_t dydesc,
//                      const void *dy,
//                      const cudnnTensorDescriptor_t dxdesc,
//                      void *dx,
//                      void *reserveSpace,
//                      size_t reserveSpaceSizeInBytes)
class DropoutBackward_for_op_grad_dropout : public OpEnv {
 public:
  DType dtype;
  cudnnDropoutDescriptor_t dropoutDesc;
  cudnnTensorDescriptor_t dydesc;
  cudnnTensorDescriptor_t dxdesc;

  DropoutBackward_for_op_grad_dropout(Array<Value> args, const OpInfo& info, Attrs attrs) {
    args.push_back(info->output);

    int n = args.size();
    std::vector<const DLTensor*> dlts(n);
    for (int i = 0; i < n - 1; ++i) {
      dlts[i] = args[i];
    }
    value::TupleValue tv = Downcast<TupleValue>(args[n - 1]);
    dlts[n - 1] = tv->fields[0];
    std::vector<OpaqueValue> opqs{Downcast<OpaqueValue>(tv->fields[1]),
                                  Downcast<OpaqueValue>(tv->fields[2])};

    dtype = DeduceDLType(dlts);
    Context ctx = info->ctx;
    (void)ctx;
    auto casted_ptr = attrs.as<attrs::DropoutAttrs>();
    (void)casted_ptr;

    CUDNN_CALL(cudnnCreateDropoutDescriptor(&dropoutDesc));
    CUDNN_CALL(cudnnRestoreDropoutDescriptor(
        dropoutDesc, CUDNNThreadEntry::ThreadLocal()->handle, casted_ptr->dropout,
        Downcast<BufferValue>(opqs[0])->data, Downcast<BufferValue>(opqs[0])->size_in_bytes,
        casted_ptr->seed));
    FORM_SHAPE(shape_0, dlts[0]);
    FORM_STRIDE(stride_0, shape_0);
    CUDNN_CALL(cudnnCreateTensorDescriptor(&dydesc));
    CUDNN_CALL(cudnnSetTensorNdDescriptor(dydesc, CUDNNDType(dtype), shape_0.size(),
                                          BeginPtr(shape_0), BeginPtr(stride_0)));
    FORM_SHAPE(shape_1, dlts[1]);
    FORM_STRIDE(stride_1, shape_1);
    CUDNN_CALL(cudnnCreateTensorDescriptor(&dxdesc));
    CUDNN_CALL(cudnnSetTensorNdDescriptor(dxdesc, CUDNNDType(dtype), shape_1.size(),
                                          BeginPtr(shape_1), BeginPtr(stride_1)));
    RequestMemory(const_cast<void**>(&dlts[1]->data), ctx, BytesCompactTensor(*dlts[1]));
  }

  ~DropoutBackward_for_op_grad_dropout() {
    CUDNN_CALL(cudnnDestroyDropoutDescriptor(dropoutDesc));
    CUDNN_CALL(cudnnDestroyTensorDescriptor(dydesc));
    CUDNN_CALL(cudnnDestroyTensorDescriptor(dxdesc));
  }

  void Execute(Array<Value> args, const OpInfo& info, Attrs attrs) override final {
    args.push_back(info->output);

    int n = args.size();
    std::vector<const DLTensor*> dlts(n);
    for (int i = 0; i < n - 1; ++i) {
      dlts[i] = args[i];
    }
    value::TupleValue tv = Downcast<TupleValue>(args[n - 1]);
    dlts[n - 1] = tv->fields[0];
    std::vector<OpaqueValue> opqs{Downcast<OpaqueValue>(tv->fields[1]),
                                  Downcast<OpaqueValue>(tv->fields[2])};

    auto casted_ptr = attrs.as<attrs::DropoutAttrs>();
    (void)casted_ptr;
    CUDNN_CALL(cudnnDropoutBackward(CUDNNThreadEntry::ThreadLocal()->handle, dropoutDesc, dydesc,
                                    dlts[0]->data, dxdesc, dlts[1]->data,
                                    Downcast<BufferValue>(opqs[1])->data,
                                    Downcast<BufferValue>(opqs[1])->size_in_bytes));
    CUDA_CALL(cudaDeviceSynchronize());
  }

  static OpEnv* make(Array<Value> args, const OpInfo& info, Attrs attrs) {
    auto res = std::make_unique<DropoutBackward_for_op_grad_dropout>(args, info, attrs);
    return res.release();
  }
};

MNM_REGISTER_OP_DISPATCH("mnm.op.grad.dropout", DevType::kCUDA(), "generated_cudnn",
                         DropoutBackward_for_op_grad_dropout::make);

class Unified_batch_norm2d;
// Frontend operator "mnm.op.batch_norm2d" dispatch to
// cudnnBatchNormalizationForwardTraining(cudnnHandle_t handle,
//                                        cudnnBatchNormMode_t mode,
//                                        const void *alpha,
//                                        const void *beta,
//                                        const cudnnTensorDescriptor_t xDesc,
//                                        const void *x,
//                                        const cudnnTensorDescriptor_t yDesc,
//                                        void *y,
//                                        const cudnnTensorDescriptor_t bnScaleBiasMeanVarDesc,
//                                        const void *bnScale,
//                                        const void *bnBias,
//                                        double exponentialAverageFactor,
//                                        void *resultRunningMean,
//                                        void *resultRunningVariance,
//                                        double epsilon,
//                                        void *resultSaveMean,
//                                        void *resultSaveInvVariance)
class BatchNormalizationForwardTraining_for_op_batch_norm2d : public OpEnv {
 public:
  DType dtype;
  cudnnBatchNormMode_t mode;
  cudnnTensorDescriptor_t xDesc;
  cudnnTensorDescriptor_t yDesc;
  cudnnTensorDescriptor_t bnScaleBiasMeanVarDesc;

  BatchNormalizationForwardTraining_for_op_batch_norm2d(Array<Value> args, const OpInfo& info,
                                                        Attrs attrs) {
    args.push_back(info->output);
    std::vector<const DLTensor*> dlts = AsVector(args);
    dtype = DeduceDLType(dlts);
    Context ctx = info->ctx;
    (void)ctx;
    auto casted_ptr = attrs.as<attrs::BatchNormAttrs>();
    (void)casted_ptr;

    mode = BatchNormModeEnum(BatchNormModeEnum::BatchnormSpatial());
    FORM_SHAPE(shape_0, dlts[0]);
    FORM_STRIDE(stride_0, shape_0);
    CUDNN_CALL(cudnnCreateTensorDescriptor(&xDesc));
    CUDNN_CALL(cudnnSetTensorNdDescriptor(xDesc, CUDNNDType(dtype), shape_0.size(),
                                          BeginPtr(shape_0), BeginPtr(stride_0)));
    FORM_SHAPE(shape_5, dlts[5]);
    FORM_STRIDE(stride_5, shape_5);
    CUDNN_CALL(cudnnCreateTensorDescriptor(&yDesc));
    CUDNN_CALL(cudnnSetTensorNdDescriptor(yDesc, CUDNNDType(dtype), shape_5.size(),
                                          BeginPtr(shape_5), BeginPtr(stride_5)));
    RequestMemory(const_cast<void**>(&dlts[5]->data), ctx, BytesCompactTensor(*dlts[5]));
    std::vector<int> shape_1{1, (int)dlts[1]->shape[0], 1, 1};
    FORM_STRIDE(stride_1, shape_1);
    CUDNN_CALL(cudnnCreateTensorDescriptor(&bnScaleBiasMeanVarDesc));
    CUDNN_CALL(cudnnSetTensorNdDescriptor(bnScaleBiasMeanVarDesc, CUDNNDType(dtype), shape_1.size(),
                                          BeginPtr(shape_1), BeginPtr(stride_1)));
  }

  ~BatchNormalizationForwardTraining_for_op_batch_norm2d() {
    CUDNN_CALL(cudnnDestroyTensorDescriptor(xDesc));
    CUDNN_CALL(cudnnDestroyTensorDescriptor(yDesc));
    CUDNN_CALL(cudnnDestroyTensorDescriptor(bnScaleBiasMeanVarDesc));
  }

  void Execute(Array<Value> args, const OpInfo& info, Attrs attrs) override final {
    args.push_back(info->output);
    std::vector<const DLTensor*> dlts = AsVector(args);
    auto casted_ptr = attrs.as<attrs::BatchNormAttrs>();
    (void)casted_ptr;
    CUDNN_CALL(cudnnBatchNormalizationForwardTraining(
        CUDNNThreadEntry::ThreadLocal()->handle, mode, CUDNNDType(dtype).const_addr<1>(),
        CUDNNDType(dtype).const_addr<0>(), xDesc, dlts[0]->data, yDesc, dlts[5]->data,
        bnScaleBiasMeanVarDesc, dlts[3]->data, dlts[4]->data, casted_ptr->momentum, dlts[1]->data,
        dlts[2]->data, casted_ptr->eps, nullptr, nullptr));
    CUDA_CALL(cudaDeviceSynchronize());
  }

  static OpEnv* make(Array<Value> args, const OpInfo& info, Attrs attrs) {
    auto res =
        std::make_unique<BatchNormalizationForwardTraining_for_op_batch_norm2d>(args, info, attrs);
    return res.release();
  }
};

// Frontend operator "mnm.op.batch_norm2d" dispatch to
// cudnnBatchNormalizationForwardInference(cudnnHandle_t handle,
//                                         cudnnBatchNormMode_t mode,
//                                         const void *alpha,
//                                         const void *beta,
//                                         const cudnnTensorDescriptor_t xDesc,
//                                         const void *x,
//                                         const cudnnTensorDescriptor_t yDesc,
//                                         void *y,
//                                         const cudnnTensorDescriptor_t bnScaleBiasMeanVarDesc,
//                                         const void *bnScale,
//                                         const void *bnBias,
//                                         const void *estimatedMean,
//                                         const void *estimatedVariance,
//                                         double epsilon)
class BatchNormalizationForwardInference_for_op_batch_norm2d : public OpEnv {
 public:
  DType dtype;
  cudnnBatchNormMode_t mode;
  cudnnTensorDescriptor_t xDesc;
  cudnnTensorDescriptor_t yDesc;
  cudnnTensorDescriptor_t bnScaleBiasMeanVarDesc;

  BatchNormalizationForwardInference_for_op_batch_norm2d(Array<Value> args, const OpInfo& info,
                                                         Attrs attrs) {
    args.push_back(info->output);
    std::vector<const DLTensor*> dlts = AsVector(args);
    dtype = DeduceDLType(dlts);
    Context ctx = info->ctx;
    (void)ctx;
    auto casted_ptr = attrs.as<attrs::BatchNormAttrs>();
    (void)casted_ptr;

    mode = BatchNormModeEnum(BatchNormModeEnum::BatchnormSpatial());
    FORM_SHAPE(shape_0, dlts[0]);
    FORM_STRIDE(stride_0, shape_0);
    CUDNN_CALL(cudnnCreateTensorDescriptor(&xDesc));
    CUDNN_CALL(cudnnSetTensorNdDescriptor(xDesc, CUDNNDType(dtype), shape_0.size(),
                                          BeginPtr(shape_0), BeginPtr(stride_0)));
    FORM_SHAPE(shape_5, dlts[5]);
    FORM_STRIDE(stride_5, shape_5);
    CUDNN_CALL(cudnnCreateTensorDescriptor(&yDesc));
    CUDNN_CALL(cudnnSetTensorNdDescriptor(yDesc, CUDNNDType(dtype), shape_5.size(),
                                          BeginPtr(shape_5), BeginPtr(stride_5)));
    RequestMemory(const_cast<void**>(&dlts[5]->data), ctx, BytesCompactTensor(*dlts[5]));
    std::vector<int> shape_1{1, (int)dlts[1]->shape[0], 1, 1};
    FORM_STRIDE(stride_1, shape_1);
    CUDNN_CALL(cudnnCreateTensorDescriptor(&bnScaleBiasMeanVarDesc));
    CUDNN_CALL(cudnnSetTensorNdDescriptor(bnScaleBiasMeanVarDesc, CUDNNDType(dtype), shape_1.size(),
                                          BeginPtr(shape_1), BeginPtr(stride_1)));
  }

  ~BatchNormalizationForwardInference_for_op_batch_norm2d() {
    CUDNN_CALL(cudnnDestroyTensorDescriptor(xDesc));
    CUDNN_CALL(cudnnDestroyTensorDescriptor(yDesc));
    CUDNN_CALL(cudnnDestroyTensorDescriptor(bnScaleBiasMeanVarDesc));
  }

  void Execute(Array<Value> args, const OpInfo& info, Attrs attrs) override final {
    args.push_back(info->output);
    std::vector<const DLTensor*> dlts = AsVector(args);
    auto casted_ptr = attrs.as<attrs::BatchNormAttrs>();
    (void)casted_ptr;
    CUDNN_CALL(cudnnBatchNormalizationForwardInference(
        CUDNNThreadEntry::ThreadLocal()->handle, mode, CUDNNDType(dtype).const_addr<1>(),
        CUDNNDType(dtype).const_addr<0>(), xDesc, dlts[0]->data, yDesc, dlts[5]->data,
        bnScaleBiasMeanVarDesc, dlts[3]->data, dlts[4]->data, dlts[1]->data, dlts[2]->data,
        casted_ptr->eps));
    CUDA_CALL(cudaDeviceSynchronize());
  }

  static OpEnv* make(Array<Value> args, const OpInfo& info, Attrs attrs) {
    auto res =
        std::make_unique<BatchNormalizationForwardInference_for_op_batch_norm2d>(args, info, attrs);
    return res.release();
  }
};

class Unified_batch_norm2d : OpEnv {
 public:
  static OpEnv* make(Array<Value> args, const OpInfo& info, Attrs attrs) {
    auto casted_ptr = attrs.as<attrs::BatchNormAttrs>();
    (void)casted_ptr;
    if (casted_ptr->is_training) {
      auto res = std::make_unique<BatchNormalizationForwardTraining_for_op_batch_norm2d>(args, info,
                                                                                         attrs);
      return res.release();
    } else {
      auto res = std::make_unique<BatchNormalizationForwardInference_for_op_batch_norm2d>(
          args, info, attrs);
      return res.release();
    }
  }
};

MNM_REGISTER_OP_DISPATCH("mnm.op.batch_norm2d", DevType::kCUDA(), "generated_cudnn",
                         Unified_batch_norm2d::make);

}  // namespace generated
}  // namespace cudnn
}  // namespace backend
}  // namespace op
}  // namespace mnm
