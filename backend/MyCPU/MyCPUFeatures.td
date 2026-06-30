//===-- MyCPUFeatures.td - MyCPU Subtarget Features --------*- tablegen -*-===//
//
// Subtarget features for the MyCPU processor.
//
//===----------------------------------------------------------------------===//

// Base feature set: always enabled
def FeatureBase    : SubtargetFeature<"base", "HasBase", "true",
                                      "Base MyCPU instruction set">;

// Multiply/divide extension (future)
def FeatureMulDiv  : SubtargetFeature<"muldiv", "HasMulDiv", "false",
                                      "Hardware multiply/divide support">;

// 64-bit support (future)
def Feature64Bit   : SubtargetFeature<"64bit", "Is64Bit", "false",
                                      "64-bit architecture support">;
