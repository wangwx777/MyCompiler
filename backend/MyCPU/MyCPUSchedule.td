//===-- MyCPUSchedule.td - MyCPU Scheduling Model ------------*- tablegen -*-===//
//
// ★ MyCPU 最小调度模型
//
// 所有指令使用 NoItinerary（单周期，无调度分析）。
// 当需要实现精确周期级调度时：
//   1. 定义多个 InstrItinClass 子类(如 ALU, Load, Store, Branch)
//   2. 创建 ProcessorItineraries 表，指定各类指令的周期数
//   3. 在指令定义中用 let Itinerary = Xxx 指定调度类
//
//===----------------------------------------------------------------------===//

// ★ 功能单元 — 当前只有 1 个流水线
def FU_Pipe : FuncUnit;

// ★ 调度类别
def MyCPUALU    : InstrItinClass;
def MyCPULoad   : InstrItinClass;
def MyCPUStore  : InstrItinClass;
def MyCPUBranch : InstrItinClass;

// ★ 最小 ProcessorItineraries
// ALU/Branch: 1 周期, Load: 2 周期, Store: 1 周期
def MyCPUInstrItin : ProcessorItineraries<
  [FU_Pipe],                // 功能单元列表
  [],                       // 无 bypass
  [                         // 指令调度数据列表
    InstrItinData<MyCPUALU,    [InstrStage<1, [FU_Pipe]>]>,
    InstrItinData<MyCPULoad,   [InstrStage<2, [FU_Pipe]>]>,
    InstrItinData<MyCPUStore,  [InstrStage<1, [FU_Pipe]>]>,
    InstrItinData<MyCPUBranch, [InstrStage<1, [FU_Pipe]>]>
  ]
>;

// ★ 调度机器模型
def MyCPUModel : SchedMachineModel {
  let IssueWidth         = 1;   // 单发射
  let MicroOpBufferSize  = 0;   // 无微操作缓存
  let LoadLatency        = 2;   // 加载延迟 2 周期
  let CompleteModel      = 1;   // 标记为完整模型
  let PostRAScheduler    = 0;   // 不做 RA 后调度
  let Itineraries        = MyCPUInstrItin;
}
