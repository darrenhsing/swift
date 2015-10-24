//===--- GlobalLoopARCSequenceDataflow.h ----------------------------------===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2015 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See http://swift.org/LICENSE.txt for license information
// See http://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

#ifndef SWIFT_SILANALYSIS_GLOBALLOOPARCSEQUENCEDATAFLOW_H
#define SWIFT_SILANALYSIS_GLOBALLOOPARCSEQUENCEDATAFLOW_H

#include "RefCountState.h"
#include "swift/SILAnalysis/LoopRegionAnalysis.h"
#include "swift/Basic/BlotMapVector.h"
#include "swift/Basic/NullablePtr.h"
#include "llvm/ADT/MapVector.h"
#include "llvm/ADT/Optional.h"

namespace swift {

class SILFunction;
class AliasAnalysis;

} // end swift namespace

namespace swift {

// Forward declaration of private classes that are opaque in this header.
class ARCRegionState;

/// A class that implements the ARC sequence data flow.
class LoopARCSequenceDataflowEvaluator {
  /// The bump ptr allocator that is used to allocate memory in the allocator.
  llvm::BumpPtrAllocator Allocator;

  /// The SILFunction that we are applying the dataflow to.
  SILFunction &F;

  /// The alias analysis that we are using for alias queries.
  AliasAnalysis *AA;

  /// Loop region information that we use to perform dataflow up and down the
  /// loop nest.
  LoopRegionFunctionInfo *LRFI;

  /// The loop info we use for convenience to seed our traversals.
  SILLoopInfo *SLI;

  /// An analysis which computes the identity root of a SILValue(), i.e. the
  /// dominating origin SILValue of the reference count that by retaining or
  /// releasing this value one is affecting.
  RCIdentityFunctionInfo *RCFI;

  /// The map from dataflow terminating decrements -> increment dataflow state.
  BlotMapVector<SILInstruction *, TopDownRefCountState> &DecToIncStateMap;

  /// The map from dataflow terminating increment -> decrement dataflow state.
  BlotMapVector<SILInstruction *, BottomUpRefCountState> &IncToDecStateMap;

  /// Stashed information for each region.
  llvm::DenseMap<const LoopRegion *, ARCRegionState *> RegionStateInfo;

  /// This is meant to find releases matched to consumed arguments. This is
  /// really less interesting than keeping a stash of all of the reference
  /// counted values that we have seen and computing their last post dominating
  /// consumed argument. But I am going to leave this in for now.
  ConsumedArgToEpilogueReleaseMatcher ConsumedArgToReleaseMap;

public:
  LoopARCSequenceDataflowEvaluator(
      SILFunction &F, AliasAnalysis *AA, LoopRegionFunctionInfo *LRFI,
      SILLoopInfo *SLI, RCIdentityFunctionInfo *RCIA,
      BlotMapVector<SILInstruction *, TopDownRefCountState> &DecToIncStateMap,
      BlotMapVector<SILInstruction *, BottomUpRefCountState> &IncToDecStateMap);
  ~LoopARCSequenceDataflowEvaluator();

  SILFunction *getFunction() const { return &F; }

  /// Clear all of the state associated with the given loop.
  void clearLoopState(const LoopRegion *R);

  /// Perform the sequence dataflow, bottom up and top down on the loop region
  /// \p R.
  bool runOnLoop(const LoopRegion *R, bool FreezeOwnedArgEpilogueReleases);

private:
  /// Merge in the BottomUp state of any successors of DataHandle.getBB() into
  /// DataHandle.getState().
  void mergeSuccessors(const LoopRegion *R, ARCRegionState &State);

  /// Merge in the TopDown state of any predecessors of DataHandle.getBB() into
  /// DataHandle.getState().
  void mergePredecessors(const LoopRegion *R, ARCRegionState &State);

  void computePostDominatingConsumedArgMap();

  ARCRegionState &getARCState(const LoopRegion *L) {
    auto Iter = RegionStateInfo.find(L);
    assert(Iter != RegionStateInfo.end() && "Should have created state for "
                                            "each region");
    return *Iter->second;
  }

  bool processLoopTopDown(const LoopRegion *R);
  bool processLoopBottomUp(const LoopRegion *R,
                           bool FreezeOwnedArgEpilogueReleases);
};

} // end swift namespace

#endif
