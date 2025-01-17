//===- MIGraphX.cpp - C Interface for MIGraphX dialect---------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "mlir-c/Dialect/Rock.h"
#include "mlir/CAPI/IR.h"
#include "mlir/CAPI/Pass.h"
#include "mlir/CAPI/Registration.h"
#include "mlir/CAPI/Wrap.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/MHAL/IR/MHAL.h"
#include "mlir/Dialect/Rock/IR/Rock.h"
#include "mlir/Dialect/Rock/Tuning/ConvContext.h"
#include "mlir/Dialect/Rock/Tuning/RockTuning.h"
#include "mlir/Dialect/Rock/utility/fusionUtils.h"
#include "mlir/Support/LogicalResult.h"
#include <cassert>

MLIR_DEFINE_CAPI_DIALECT_REGISTRATION(Rock, rock, mlir::rock::RockDialect)
DEFINE_C_API_PTR_METHODS(MlirRockTuningSpace, mlir::rock::TuningParamSet)
DEFINE_C_API_PTR_METHODS(MlirRockTuningParam, mlir::rock::ParamEntry)
DEFINE_C_API_PTR_METHODS(MlirRockTuningTable, mlir::rock::TuningTable)

using namespace mlir;

MLIR_CAPI_EXPORTED MlirRockTuningSpace
mlirRockTuningSpaceCreate(MlirModule module, RocmlirTuningParamSetKind kind) {
  struct rock::TuningParamSet *newParams;
  rock::TuningParamSetKind ourKind;
  switch (kind) {
  case RocmlirTuningParamSetKindQuick:
    ourKind = rock::TuningParamSetKind::Quick;
    break;
  case RocmlirTuningParamSetKindFull:
    ourKind = rock::TuningParamSetKind::Full;
    break;
  case RocmlirTuningParamSetKindExhaustive:
    ourKind = rock::TuningParamSetKind::Exhaustive;
    break;
  }
  auto mod = unwrap(module);
  newParams = rock::createTunableParamSpace(mod, ourKind);
  return wrap(newParams);
}

MLIR_CAPI_EXPORTED
unsigned mlirRockTuningGetNumParams(MlirRockTuningSpace params) {
  auto *tuningSpace = unwrap(params);
  return tuningSpace->tuningRange.size();
}

MLIR_CAPI_EXPORTED MlirRockTuningParam mlirRockTuningParamCreate() {
  rock::ParamEntry *param = new rock::ParamEntry();
  return wrap(param);
}

MLIR_CAPI_EXPORTED
void mlirRockTuningParamDestroy(MlirRockTuningParam param) {
  delete (unwrap(param));
}

MLIR_CAPI_EXPORTED
void mlirRockTuningSpaceDestroy(MlirRockTuningSpace params) {
  delete (unwrap(params));
}

MLIR_CAPI_EXPORTED
bool mlirRockTuningParamGet(MlirRockTuningSpace params, unsigned pos,
                            MlirRockTuningParam param) {
  auto *tuningSpace = unwrap(params);
  auto *paramEntry = unwrap(param);
  return rock::tuningGetParam(tuningSpace, pos, paramEntry);
}

MLIR_CAPI_EXPORTED
size_t mlirRockTuningParamToString(MlirRockTuningParam param, char *buf,
                                   size_t bufLen) {
  auto *paramEntry = unwrap(param);
  SmallString<ROCMLIR_TUNING_PARAM_STRING_BUFSZ> perfConfig;
  paramEntry->param.getPerfConfigStr(perfConfig);
  strncpy(buf, perfConfig.c_str(), bufLen);
  return perfConfig.size();
}

MLIR_CAPI_EXPORTED
bool mlirRockTuningSetParam(MlirModule module, MlirRockTuningParam param) {
  auto mod = unwrap(module);
  auto *paramEntry = unwrap(param);
  return rock::tuningSetParam(mod, paramEntry);
}

MLIR_CAPI_EXPORTED
bool mlirRockTuningSetFromStr(MlirModule module, MlirStringRef perfStr) {
  auto mod = unwrap(module);
  StringRef perfConfig = unwrap(perfStr);
  return rock::tuningSetStr(mod, perfConfig);
}

MLIR_CAPI_EXPORTED
MlirRockTuningTable mlirRockTuningTableCreate() {
  struct rock::TuningTable *newTable = rock::tuningTableCreate();
  return wrap(newTable);
}

MLIR_CAPI_EXPORTED
void mlirRockTuningTableDestroy(MlirRockTuningTable table) {
  delete unwrap(table);
}

MLIR_CAPI_EXPORTED
bool mlirRockTuningUpdateTable(MlirRockTuningTable perfTable,
                               MlirStringRef problemKey, MlirStringRef perfStr,
                               float time) {
  StringRef problem = unwrap(problemKey);
  StringRef perfConfig = unwrap(perfStr);
  auto *pTable = unwrap(perfTable);
  return rock::tuningTableUpdate(pTable, problem, perfConfig, time);
}

MLIR_CAPI_EXPORTED
bool mlirRockTuningSetFromTable(MlirRockTuningTable perfTable,
                                MlirModule module) {
  auto *pTable = unwrap(perfTable);
  auto mod = unwrap(module);
  SmallString<ROCMLIR_TUNING_PARAM_STRING_BUFSZ> perfConfig;
  if (failed(rock::tuningTableLookup(pTable, mod, perfConfig)))
    return false;
  return rock::tuningSetStr(mod, perfConfig);
}

MLIR_CAPI_EXPORTED size_t mlirRockTuningGetKey(MlirModule module, char *buf,
                                               size_t bufLen) {
  auto mod = unwrap(module);
  SmallString<ROCMLIR_TUNING_KEY_BUFSZ> perfStr;
  if (failed(rock::getTuningProblemStr(mod, perfStr)))
    return (size_t)(-1);
  strncpy(buf, perfStr.c_str(), bufLen);
  return perfStr.size();
}

MLIR_CAPI_EXPORTED
enum RocmlirSplitKSelectionLikelihood
mlirIsSplitKFaster(int64_t gDim, int64_t mDim, int64_t nDim, int64_t kDim,
                   int64_t numCUs, RocmlirTuningParamSetKind tuningLevel) {
  if (tuningLevel ==
      RocmlirTuningParamSetKind::RocmlirTuningParamSetKindQuick) {
    // Note, we return `never` because we don't provide splitK values
    // in the case of `Quick` tuning. If we decide to remove this restriction
    // in the future, we must remove this if-statement
    return RocmlirSplitKSelectionLikelihood::never;
  }
  return rock::isSplitKFaster(gDim, mDim, nDim, kDim, numCUs);
}

MLIR_CAPI_EXPORTED
bool mlirIsModuleFusible(MlirModule module, MlirStringRef perfStr) {
  auto mod = unwrap(module);
  StringRef perfConfig = unwrap(perfStr);
  return rock::isModuleFusible(mod, perfConfig);
}

MLIR_CAPI_EXPORTED
size_t mlirGetNumPrefillArgs(MlirModule module) {
  auto mod = unwrap(module);
  assert(mod.getRegion().getBlocks().size() == 1 &&
         "expected a single block/function in a module");

  std::optional<LLVM::LLVMFuncOp> func = std::nullopt;
  mod.walk([&](LLVM::LLVMFuncOp op) { func = op; });

  if (!func.has_value())
    return 0;
  auto attrs = rock::getStoredPrefillAttributes(func.value());
  return attrs.size();
}

MLIR_CAPI_EXPORTED
void mlirGetPrefillArgsInfo(MlirModule module, size_t *indices,
                            MlirAttribute *initValues, size_t length) {
  auto mod = unwrap(module);
  assert(mod.getRegion().getBlocks().size() == 1 &&
         "expected a single block/function in a module");

  std::optional<LLVM::LLVMFuncOp> func = std::nullopt;
  mod.walk([&](LLVM::LLVMFuncOp op) { func = op; });

  if (!func.has_value())
    return;
  auto attrs = rock::getStoredPrefillAttributes(func.value());

  assert(attrs.size() >= length && "length cannot exceed the attr size");
  for (size_t i = 0; i < length; ++i) {
    indices[i] = attrs[i].getArgIndex();
    initValues[i] = wrap(attrs[i].getInitValue());
  }
}

MLIR_CAPI_EXPORTED
size_t mlirGetNumAuxBuffers(MlirModule module) { return 0; }

MLIR_CAPI_EXPORTED
void mlirGetAuxBuffersInfo(MlirModule module, size_t *sizes,
                           MlirAttribute *initValues, size_t length) {}
