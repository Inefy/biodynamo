// -----------------------------------------------------------------------------
//
// Copyright (C) The BioDynaMo Project.
// All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
//
// See the LICENSE file distributed with this work for details.
// See the NOTICE file distributed with this work for additional information
// regarding copyright ownership.
//
// -----------------------------------------------------------------------------

#ifndef EXECUTION_CONTEXT_APPROXIMATE_EXEC_CTXT_H_
#define EXECUTION_CONTEXT_APPROXIMATE_EXEC_CTXT_H_

#include <vector>
#include <memory>
#include <mutex>

#include "resource_manager.h"
#include "fixed_size_vector.h"

namespace bdm {

// TODO rename to InPlaceExecCtxt ?
// TODO documentation + test
template <typename TCTParam = CompileTimeParam<>>
class ApproximateExecCtxt {
public:
  using Backend = typename TCTParam::SimulationBackend;
  using Types = typename TCTParam::SimObjectTypes;

  ApproximateExecCtxt() {
    // FIXME this doesn't work: must hold all new elements for all sim_objects processed by this thread.
    // reserve enough memory to hold all new objects during one iteration of
    // one sim object. If more objects would be created (using `New`),
    // references would become invalid.
    // Alternative: use container that doesn't migrate objects.
    new_sim_objects_.Reserve(10);
  }

  template <typename TSimulation = Simulation<>>
  void SetupIteration() {
    EnableNeighborGuards(); // FIXME remove

    // first iteration might have uncommited changes
    TearDownIteration();
  }

  /// This function is not thread-safe
  template <typename TSimulation = Simulation<>>
  void TearDownIteration() {
    // new sim objects
    auto* rm = TSimulation::GetActive()->GetResourceManager();
    new_sim_objects_.ApplyOnAllElements([&](auto&& sim_object, SoHandle){
      rm->push_back(sim_object);
    });
    new_sim_objects_.Clear();

    // removed sim objects
    // remove them after adding new ones (maybe one has been removed
    // that was in new_sim_objects_)
    for(auto& uid : remove_) {
      rm->Remove(uid);
    }
    remove_.clear();
  }

  /// Execute a series of operations on a simulation object in the order given
  /// in the argument
  template <typename TSo, typename TFirstOp, typename... TOps>
  void Execute(TSo&& so, TFirstOp first_op, TOps... other_ops) {
    auto* grid = Simulation<TCTParam>::GetActive()->GetGrid();
    auto nb_mutex_builder = grid->GetNeighborMutexBuilder();
    if (nb_mutex_builder != nullptr) {
      auto mutex = nb_mutex_builder->GetMutex(so.GetBoxIdx());
      std::lock_guard<decltype(mutex)> guard(mutex);
      ExecuteInternal(so, first_op, other_ops...);
    } else {
      ExecuteInternal(so, first_op, other_ops...);
    }
  }

  /// Create a new simulation object and return a reference to it.
  /// @tparam TScalarSo simulation object type with scalar backend
  /// @param args arguments which will be forwarded to the TScalarSo constructor
  /// @remarks Note that this function is not thread safe.
  template <typename TScalarSo, typename... Args, typename TBackend = Backend>
  typename std::enable_if<std::is_same<TBackend, Soa>::value,
                          typename TScalarSo::template Self<SoaRef>>::type
  New(Args... args) {
    TScalarSo so(std::forward<Args>(args)...);
    auto uid = so.GetUid();
    new_sim_objects_.push_back(so);
    return new_sim_objects_.template GetSimObject<TScalarSo>(uid);
  }

  template <typename TScalarSo, typename... Args, typename TBackend = Backend>
  typename std::enable_if<std::is_same<TBackend, Scalar>::value,
                          TScalarSo&>::type
  New(Args... args) {
    TScalarSo so(std::forward<Args>(args)...);
    auto uid = so.GetUid();
    new_sim_objects_.push_back(so);
    return new_sim_objects_.template GetSimObject<TScalarSo>(uid);
  }

  /// Forwards the call to `Grid::ForEachNeighborWithinRadius`
  /// Could be used to cache the results.
  template <typename TLambda, typename TSo, typename TSimulation = Simulation<>>
  void ForEachNeighborWithinRadius(const TLambda& lambda, const TSo& query,
                                   double squared_radius) {
     auto* grid = TSimulation::GetActive()->GetGrid();
     return grid->template ForEachNeighborWithinRadius(lambda, query, squared_radius);
  }

  template <typename TSo, typename TSimBackend = Backend, typename TSimulation = Simulation<>>
  auto&& GetSimObject(SoUid uid, typename std::enable_if<std::is_same<TSimBackend, Scalar>::value>::type* ptr = 0) {
    // check if the uid corresponds to a new object not yet in the Rm
    if (new_sim_objects_.Contains(uid)) {
      return new_sim_objects_.template GetSimObject<TSo>(uid);
    } else {
      auto* rm = TSimulation::GetActive()->GetResourceManager();
      return rm->template GetSimObject<TSo>(uid);
    }
  }

  template <typename TSo, typename TSimBackend = Backend, typename TSimulation = Simulation<>>
  auto GetSimObject(SoUid uid, typename std::enable_if<std::is_same<TSimBackend, Soa>::value>::type* ptr = 0) {
    if (new_sim_objects_.Contains(uid)) {
      return new_sim_objects_.template GetSimObject<TSo>(uid);
    } else {
      auto* rm = TSimulation::GetActive()->GetResourceManager();
      return rm->template GetSimObject<TSo>(uid);
    }
  }

  template <typename TSo, typename TSimBackend = Backend, typename TSimulation = Simulation<>>
  const auto&& GetConstSimObject(SoUid uid, typename std::enable_if<std::is_same<TSimBackend, Scalar>::value>::type* ptr = 0) {
    return GetSimObject<TSo>(uid);
  }

  template <typename TSo, typename TSimBackend = Backend, typename TSimulation = Simulation<>>
  const auto GetConstSimObject(SoUid uid, typename std::enable_if<std::is_same<TSimBackend, Soa>::value>::type* ptr = 0) {
    return GetSimObject<TSo>(uid);
  }

  template <typename TSimulation = Simulation<>>
  void RemoveFromSimulation(SoUid uid) {
    remove_.push_back(uid);
  }

  // TODO make opt-out
  /// If a sim objects modifies other simulation objects while it is updated,
  /// race conditions can occur using this exection context. This function turns
  /// on the protection mechanism. The protection mechanism is turned off by
  /// default to avoid unnecessary overhead for simulations that do not require
  /// this feature.
  void EnableNeighborGuards() {
    Simulation<TCTParam>::GetActive()->GetGrid()->EnableNeighborMutexes();
  }

private:
  std::vector<SoUid> remove_;

  /// Use seperate ResourceManager to store new objects, before they are added
  /// to the main ResourceManager. Using a ResourceManager adds
  /// some memory overhead, but avoids code duplication.
  ResourceManager<TCTParam> new_sim_objects_;

  /// Execute a single operation on a simulation object
  template <typename TSo, typename TFirstOp>
  void ExecuteInternal(TSo&& so, TFirstOp first_op) {
    first_op(so);
  }

  /// Execute a series of operations on a simulation object in the order given
  /// in the argument
  template <typename TSo, typename TFirstOp, typename... TOps>
  void ExecuteInternal(TSo&& so, TFirstOp first_op, TOps... other_ops) {
    first_op(so);
    ExecuteInternal(so, other_ops...);
  }
};

}  // namespace bdm

#endif  // EXECUTION_CONTEXT_APPROXIMATE_EXECUTION_CTXT_H_