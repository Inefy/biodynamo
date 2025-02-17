// -----------------------------------------------------------------------------
//
// Copyright (C) 2021 CERN & University of Surrey for the benefit of the
// BioDynaMo collaboration. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
//
// See the LICENSE file distributed with this work for details.
// See the NOTICE file distributed with this work for additional information
// regarding copyright ownership.
//
// -----------------------------------------------------------------------------

#ifndef CORE_AGENT_CELL_H_
#define CORE_AGENT_CELL_H_

#include <array>
#include <cmath>
#include <complex>
#include <set>
#include <string>
#include <type_traits>
#include <vector>

#include "core/agent/agent.h"
#include "core/agent/cell_division_event.h"
#include "core/agent/new_agent_event.h"
#include "core/container/inline_vector.h"
#include "core/container/math_array.h"
#include "core/execution_context/in_place_exec_ctxt.h"
#include "core/functor.h"
#include "core/interaction_force.h"
#include "core/param/param.h"
#include "core/shape.h"
#include "core/util/math.h"

namespace bdm {

class Cell : public Agent {
  BDM_AGENT_HEADER(Cell, Agent, 1);

 public:
  /// First axis of the local coordinate system.
  static const Real3 kXAxis;
  /// Second axis of the local coordinate system.
  static const Real3 kYAxis;
  /// Third axis of the local coordinate system.
  static const Real3 kZAxis;

  Cell() : diameter_(1.0), density_(1.0) { UpdateVolume(); }

  explicit Cell(real_t diameter) : diameter_(diameter), density_(1.0) {
    UpdateVolume();
  }

  explicit Cell(const Real3& position)
      : position_(position), diameter_(1.0), density_(1.0) {
    UpdateVolume();
  }

  virtual ~Cell() = default;

  /// \brief This method is used to initialise the values of daughter
  /// 2 for a cell division event.
  ///
  /// \see CellDivisionEvent
  void Initialize(const NewAgentEvent& event) override {
    Base::Initialize(event);

    if (event.GetUid() == CellDivisionEvent::kUid) {
      const auto& cdevent = static_cast<const CellDivisionEvent&>(event);
      auto* mother_cell = bdm_static_cast<Cell*>(event.existing_agent);
      auto* daughter = this;  // FIXME
      // A) Defining some values
      // ..................................................................
      // defining the two radii s.t total volume is conserved
      // * radius^3 = r1^3 + r2^3 ;
      // * volume_ratio = r2^3 / r1^3
      real_t radius = mother_cell->GetDiameter() * real_t(0.5);

      // define an axis for division (along which the nuclei will move)
      real_t x_coord = std::cos(cdevent.theta) * std::sin(cdevent.phi);
      real_t y_coord = std::sin(cdevent.theta) * std::sin(cdevent.phi);
      real_t z_coord = std::cos(cdevent.phi);
      Real3 coords = {x_coord, y_coord, z_coord};
      real_t total_length_of_displacement = radius / real_t(4.0);

      const auto& x_axis = mother_cell->kXAxis;
      const auto& y_axis = mother_cell->kYAxis;
      const auto& z_axis = mother_cell->kZAxis;

      Real3 axis_of_division =
          (coords.EntryWiseProduct(x_axis) + coords.EntryWiseProduct(y_axis) +
           coords.EntryWiseProduct(z_axis)) *
          total_length_of_displacement;

      // two equations for the center displacement :
      //  1) d2/d1= v2/v1 = volume_ratio (each sphere is shifted inver.
      //  proportionally to its volume)
      //  2) d1 + d2 = TOTAL_LENGTH_OF_DISPLACEMENT
      real_t d_2 = total_length_of_displacement / (cdevent.volume_ratio + 1);
      real_t d_1 = total_length_of_displacement - d_2;

      real_t mother_volume = mother_cell->GetVolume();
      real_t new_volume = mother_volume / (cdevent.volume_ratio + 1);
      daughter->SetVolume(mother_volume - new_volume);

      // position
      auto mother_pos = mother_cell->GetPosition();
      auto new_position = mother_pos + (axis_of_division * d_2);
      daughter->SetPosition(new_position);

      // E) This sphere becomes the 1st daughter
      // move these cells on opposite direction
      mother_pos -= axis_of_division * d_1;
      // update mother here and not in Update method to avoid recomputation
      mother_cell->SetPosition(mother_pos);
      mother_cell->SetVolume(new_volume);

      daughter->SetAdherence(mother_cell->GetAdherence());
      daughter->SetDensity(mother_cell->GetDensity());
      // G) TODO(lukas) Copy the intracellular and membrane bound Substances
    }
  }

  Shape GetShape() const override { return Shape::kSphere; }

  /// \brief Divide this cell.
  ///
  /// CellDivisionEvent::volume_ratio will be between 0.9 and 1.1\n
  /// The axis of division is random.
  /// \see CellDivisionEvent
  virtual Cell* Divide() {
    auto* random = Simulation::GetActive()->GetRandom();
    return Divide(random->Uniform(real_t(0.9), real_t(1.1)));
  }

  /// \brief Divide this cell.
  ///
  /// The axis of division is random.
  /// \see CellDivisionEvent
  virtual Cell* Divide(real_t volume_ratio) {
    // find random point on sphere (based on :
    // http://mathworld.wolfram.com/SpherePointPicking.html)
    auto* random = Simulation::GetActive()->GetRandom();
    real_t theta = 2 * Math::kPi * random->Uniform(0, 1);
    real_t phi = std::acos(2 * random->Uniform(0, 1) - 1);
    return Divide(volume_ratio, phi, theta);
  }

  /// \brief Divide this cell.
  ///
  /// CellDivisionEvent::volume_ratio will be between 0.9 and 1.1\n
  /// \see CellDivisionEvent
  virtual Cell* Divide(const Real3& axis) {
    auto* random = Simulation::GetActive()->GetRandom();
    auto polarcoord = TransformCoordinatesGlobalToPolar(axis + position_);
    return Divide(random->Uniform(real_t(0.9), real_t(1.1)), polarcoord[1],
                  polarcoord[2]);
  }

  /// \brief Divide this cell.
  ///
  /// \see CellDivisionEvent
  virtual Cell* Divide(real_t volume_ratio, const Real3& axis) {
    auto polarcoord = TransformCoordinatesGlobalToPolar(axis + position_);
    return Divide(volume_ratio, polarcoord[1], polarcoord[2]);
  }

  /// \brief Divide this cell.
  ///
  /// \see CellDivisionEvent
  virtual Cell* Divide(real_t volume_ratio, real_t phi, real_t theta) {
    CellDivisionEvent event(volume_ratio, phi, theta);
    CreateNewAgents(event, {this});
    return bdm_static_cast<Cell*>(event.new_agents[0]);
  }

  real_t GetAdherence() const { return adherence_; }

  real_t GetDiameter() const override { return diameter_; }

  real_t GetMass() const { return density_ * volume_; }

  real_t GetDensity() const { return density_; }

  const Real3& GetPosition() const override { return position_; }

  const Real3& GetTractorForce() const { return tractor_force_; }

  real_t GetVolume() const { return volume_; }

  void SetAdherence(real_t adherence) {
    if (adherence < adherence_) {
      SetStaticnessNextTimestep(false);
    }
    adherence_ = adherence;
  }

  void SetDiameter(real_t diameter) override {
    if (diameter > diameter_) {
      SetPropagateStaticness();
    }
    diameter_ = diameter;
    UpdateVolume();
  }

  void SetVolume(real_t volume) {
    volume_ = volume;
    UpdateDiameter();
  }

  void SetMass(real_t mass) { SetDensity(mass / volume_); }

  void SetDensity(real_t density) {
    if (density > density_) {
      SetPropagateStaticness();
    }
    density_ = density;
  }

  void SetPosition(const Real3& position) override {
    position_ = position;
    SetPropagateStaticness();
  }

  void SetTractorForce(const Real3& tractor_force) {
    tractor_force_ = tractor_force;
  }

  void ChangeVolume(real_t speed) {
    // scaling for integration step
    auto* param = Simulation::GetActive()->GetParam();
    real_t delta = speed * param->simulation_time_step;
    volume_ += delta;
    if (volume_ < real_t(5.2359877E-7)) {
      volume_ = real_t(5.2359877E-7);
    }
    UpdateDiameter();
  }

  void UpdateDiameter() {
    // V = (4/3)*pi*r^3 = (pi/6)*diameter^3
    real_t diameter = std::cbrt(volume_ * 6 / Math::kPi);
    if (diameter > diameter_) {
      Base::SetPropagateStaticness();
    }
    diameter_ = diameter;
  }

  void UpdateVolume() {
    // V = (4/3)*pi*r^3 = (pi/6)*diameter^3
    volume_ = Math::kPi / real_t(6) * std::pow(diameter_, 3);
  }

  void UpdatePosition(const Real3& delta) {
    position_ += delta;
    SetPropagateStaticness();
  }

  Real3 CalculateDisplacement(const InteractionForce* force,
                              real_t squared_radius, real_t dt) override {
    // Basically, the idea is to make the sum of all the forces acting
    // on the Point mass. It is stored in translationForceOnPointMass.
    // There is also a computation of the torque (only applied
    // by the daughter neurites), stored in rotationForce.

    // TODO(roman) : There might be a problem, in the sense that the biology
    // is not applied if the total Force is smaller than adherence.
    // Once, I should look at this more carefully.

    // fixme why? copying
    const auto& tf = GetTractorForce();

    // the 3 types of movement that can occur
    // bool biological_translation = false;
    bool physical_translation = false;
    // bool physical_rotation = false;

    real_t h = dt;
    Real3 movement_at_next_step{0, 0, 0};

    // BIOLOGY :
    // 0) Start with tractor force : What the biology defined as active
    // movement------------
    movement_at_next_step += tf * h;

    // PHYSICS
    // the physics force to move the point mass
    Real3 translation_force_on_point_mass{0, 0, 0};

    // the physics force to rotate the cell
    // Real3 rotation_force { 0, 0, 0 };

    // 1) "artificial force" to maintain the sphere in the ecm simulation
    // boundaries--------
    // 2) Spring force from my neurites (translation and
    // rotation)--------------------------
    // 3) Object avoidance force
    // -----------------------------------------------------------
    //  (We check for every neighbor object if they touch us, i.e. push us
    //  away)

    uint64_t non_zero_neighbor_forces = 0;
    if (!IsStatic()) {
      auto* ctxt = Simulation::GetActive()->GetExecutionContext();
      auto calculate_neighbor_forces =
          L2F([&](Agent* neighbor, real_t squared_distance) {
            auto neighbor_force = force->Calculate(this, neighbor);
            if (neighbor_force[0] != 0 || neighbor_force[1] != 0 ||
                neighbor_force[2] != 0) {
              non_zero_neighbor_forces++;
              translation_force_on_point_mass[0] += neighbor_force[0];
              translation_force_on_point_mass[1] += neighbor_force[1];
              translation_force_on_point_mass[2] += neighbor_force[2];
            }
          });
      ctxt->ForEachNeighbor(calculate_neighbor_forces, *this, squared_radius);

      if (non_zero_neighbor_forces > 1) {
        SetStaticnessNextTimestep(false);
      }
    }

    // 4) PhysicalBonds
    // How the physics influences the next displacement
    real_t norm_of_force = std::sqrt(translation_force_on_point_mass *
                                     translation_force_on_point_mass);

    // is there enough force to :
    //  - make us biologically move (Tractor) :
    //  - break adherence and make us translate ?
    physical_translation = norm_of_force > GetAdherence();

    assert(GetMass() != 0 && "The mass of a cell was found to be zero!");
    real_t mh = h / GetMass();
    // adding the physics translation (scale by weight) if important enough
    if (physical_translation) {
      // We scale the move with mass and time step
      movement_at_next_step += translation_force_on_point_mass * mh;

      // Performing the translation itself :
      // but we want to avoid huge jumps in the simulation, so there are
      // maximum distances possible
      auto* param = Simulation::GetActive()->GetParam();
      if (norm_of_force * mh > param->simulation_max_displacement) {
        movement_at_next_step.Normalize();
        movement_at_next_step *= param->simulation_max_displacement;
      }
    }
    return movement_at_next_step;
  }

  void ApplyDisplacement(const Real3& displacement) override;

  void MovePointMass(const Real3& normalized_dir, real_t speed) {
    tractor_force_ += normalized_dir * speed;
  }

 protected:
  /// Returns the position in the polar coordinate system (cylindrical or
  /// spherical) of a point expressed in global cartesian coordinates
  /// ([1,0,0],[0,1,0],[0,0,1]).
  /// @param coord: position in absolute coordinates - [x,y,z] cartesian values
  /// @return the position in local coordinates
  Real3 TransformCoordinatesGlobalToPolar(const Real3& coord) const;

 private:
  /// NB: Use setter and don't assign values directly
  Real3 position_ = {{0, 0, 0}};
  Real3 tractor_force_ = {{0, 0, 0}};
  /// NB: Use setter and don't assign values directly
  real_t diameter_ = 0;
  real_t volume_ = 0;
  /// NB: Use setter and don't assign values directly
  real_t adherence_ = 0;
  /// NB: Use setter and don't assign values directly
  real_t density_ = 0;
};

}  // namespace bdm

#endif  // CORE_AGENT_CELL_H_
