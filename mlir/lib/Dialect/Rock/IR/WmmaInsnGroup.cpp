
#include "mlir/Dialect/Rock/IR/WmmaInsnGroup.h"

#include "mlir/Dialect/AMDGPU/IR/AMDGPUDialect.h"
#include "mlir/Dialect/LLVMIR/ROCDLDialect.h"
#include "mlir/Dialect/Rock/utility/AmdArchDb.h"
#include "mlir/Dialect/Rock/utility/math.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinTypes.h"

#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include <cstdint>

#define DEBUG_TYPE "rock-wmma-insn-group"

using namespace mlir;
using namespace mlir::rock;

static Type getRetType(Type inputType) {
  Builder b(inputType.getContext());
  if (inputType.isInteger(8))
    return b.getI32Type();

  return b.getF32Type();
}

bool WmmaInsn::isCoherentWithK(int64_t kpack, int64_t kPerBlock) {
  if (kPerBlock * kpack < inputLen) {
    LLVM_DEBUG(llvm::dbgs()
               << "kPerBlock*kpack needs to be a multiple of inputLen "
               << inputLen << "\n");
    return false;
  }
  return true;
}

FailureOr<WmmaInsn> WmmaInsn::select(mlir::Type elementTypeA,
                                     mlir::Type elementTypeB, int64_t waveSize,
                                     int64_t mPerWave, int64_t nPerWave) {
  LLVM_DEBUG(llvm::dbgs() << "Invoke Wmma group selection:\n"
                          << "elementTypeA: " << elementTypeA << "\n"
                          << "elementTypeB: " << elementTypeB << "\n"
                          << "mPerWave: " << mPerWave << "\n"
                          << "nPerWave: " << nPerWave << "\n");

  if (elementTypeA != elementTypeB)
    return failure();

  if (waveSize != 32)
    return failure();

  int64_t inputLen = 16;
  int64_t outLen = 8;
  int64_t outStride = 2;

  if (mPerWave % inputLen != 0)
    return failure();
  if (nPerWave % inputLen != 0)
    return failure();

  int64_t mRepeats = mPerWave / inputLen;
  int64_t nRepeats = nPerWave / inputLen;

  VectorType argTypeA = VectorType::get({inputLen}, elementTypeA);
  VectorType argTypeB = VectorType::get({inputLen}, elementTypeB);
  VectorType retType = VectorType::get({outLen}, getRetType(elementTypeA));

  StringRef insn;
  if (elementTypeA.isF16()) {
    insn = ROCDL::wmma_f32_16x16x16_f16::getOperationName();
  } else if (elementTypeA.isBF16()) {
    insn = ROCDL::wmma_f32_16x16x16_bf16::getOperationName();
  } else if (elementTypeA.isInteger(8)) {
    insn = ROCDL::wmma_i32_16x16x16_iu8::getOperationName();
  } else {
    return failure();
  }

  return WmmaInsn{insn,     inputLen, outLen,   outStride, mRepeats,
                  nRepeats, argTypeA, argTypeB, retType};
}
