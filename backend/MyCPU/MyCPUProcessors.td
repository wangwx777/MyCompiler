//===-- MyCPUProcessors.td - MyCPU Processor Definitions ----*- tablegen -*-===//
//
// Processor model definitions for the MyCPU target.
//
//===----------------------------------------------------------------------===//

def : ProcessorModel<"generic-mycpu", NoSchedModel, [FeatureBase]>;
