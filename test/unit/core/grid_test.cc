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

#include "core/environment/environment.h"
#include "core/environment/uniform_grid_environment.h"
#include "core/sim_object/cell.h"
#include "gtest/gtest.h"
#include "unit/test_util/test_util.h"

namespace bdm {

void CellFactory(ResourceManager* rm, size_t cells_per_dim) {
  const double space = 20;
  rm->Reserve(cells_per_dim * cells_per_dim * cells_per_dim);
  for (size_t i = 0; i < cells_per_dim; i++) {
    for (size_t j = 0; j < cells_per_dim; j++) {
      for (size_t k = 0; k < cells_per_dim; k++) {
        Cell* cell = new Cell({k * space, j * space, i * space});
        cell->SetDiameter(30);
        rm->push_back(cell);
      }
    }
  }
}

TEST(GridTest, SetupGrid) {
  Simulation simulation(TEST_NAME);
  auto* rm = simulation.GetResourceManager();
  auto* grid =
      static_cast<UniformGridEnvironment*>(simulation.GetEnvironment());

  CellFactory(rm, 4);

  grid->Update();

  std::unordered_map<SoUid, std::vector<SoUid>> neighbors;
  neighbors.reserve(rm->GetNumSimObjects());

  // Lambda that fills a vector of neighbors for each cell (excluding itself)
  rm->ApplyOnAllElements([&](SimObject* so) {
    auto uid = so->GetUid();
    auto fill_neighbor_list = [&](const SimObject* neighbor) {
      auto nuid = neighbor->GetUid();
      if (uid != nuid) {
        neighbors[uid].push_back(nuid);
      }
    };

    grid->ForEachNeighborWithinRadius(fill_neighbor_list, *so, 1201);
  });

  std::vector<SoUid> expected_0 = {SoUid(1),  SoUid(4),  SoUid(5), SoUid(16),
                                   SoUid(17), SoUid(20), SoUid(21)};
  std::vector<SoUid> expected_4 = {SoUid(0),  SoUid(1),  SoUid(5),  SoUid(8),
                                   SoUid(9),  SoUid(16), SoUid(17), SoUid(20),
                                   SoUid(21), SoUid(24), SoUid(25)};
  std::vector<SoUid> expected_42 = {
      SoUid(21), SoUid(22), SoUid(23), SoUid(25), SoUid(26), SoUid(27),
      SoUid(29), SoUid(30), SoUid(31), SoUid(37), SoUid(38), SoUid(39),
      SoUid(41), SoUid(43), SoUid(45), SoUid(46), SoUid(47), SoUid(53),
      SoUid(54), SoUid(55), SoUid(57), SoUid(58), SoUid(59), SoUid(61),
      SoUid(62), SoUid(63)};
  std::vector<SoUid> expected_63 = {SoUid(42), SoUid(43), SoUid(46), SoUid(47),
                                    SoUid(58), SoUid(59), SoUid(62)};

  std::sort(neighbors[SoUid(0)].begin(), neighbors[SoUid(0)].end());
  std::sort(neighbors[SoUid(4)].begin(), neighbors[SoUid(4)].end());
  std::sort(neighbors[SoUid(42)].begin(), neighbors[SoUid(42)].end());
  std::sort(neighbors[SoUid(63)].begin(), neighbors[SoUid(63)].end());

  EXPECT_EQ(expected_0, neighbors[SoUid(0)]);
  EXPECT_EQ(expected_4, neighbors[SoUid(4)]);
  EXPECT_EQ(expected_42, neighbors[SoUid(42)]);
  EXPECT_EQ(expected_63, neighbors[SoUid(63)]);
}

void RunUpdateGridTest(Simulation* simulation) {
  auto* rm = simulation->GetResourceManager();
  auto* grid =
      static_cast<UniformGridEnvironment*>(simulation->GetEnvironment());

  // Update the grid
  grid->Update();

  std::unordered_map<SoUid, std::vector<SoUid>> neighbors;
  neighbors.reserve(rm->GetNumSimObjects());

  // Lambda that fills a vector of neighbors for each cell (excluding itself)
  rm->ApplyOnAllElements([&](SimObject* so) {
    auto uid = so->GetUid();
    auto fill_neighbor_list = [&](const SimObject* neighbor) {
      auto nuid = neighbor->GetUid();
      if (uid != nuid) {
        neighbors[uid].push_back(nuid);
      }
    };

    grid->ForEachNeighborWithinRadius(fill_neighbor_list, *so, 1201);
  });

  std::vector<SoUid> expected_0 = {SoUid(4),  SoUid(5),  SoUid(16),
                                   SoUid(17), SoUid(20), SoUid(21)};
  std::vector<SoUid> expected_5 = {SoUid(0),  SoUid(2),  SoUid(4),  SoUid(6),
                                   SoUid(8),  SoUid(9),  SoUid(10), SoUid(16),
                                   SoUid(17), SoUid(18), SoUid(20), SoUid(21),
                                   SoUid(22), SoUid(24), SoUid(25), SoUid(26)};
  std::vector<SoUid> expected_41 = {
      SoUid(20), SoUid(21), SoUid(22), SoUid(24), SoUid(25),
      SoUid(26), SoUid(28), SoUid(29), SoUid(30), SoUid(36),
      SoUid(37), SoUid(38), SoUid(40), SoUid(44), SoUid(45),
      SoUid(46), SoUid(52), SoUid(53), SoUid(54), SoUid(56),
      SoUid(57), SoUid(58), SoUid(60), SoUid(61), SoUid(62)};
  std::vector<SoUid> expected_61 = {SoUid(40), SoUid(41), SoUid(44), SoUid(45),
                                    SoUid(46), SoUid(56), SoUid(57), SoUid(58),
                                    SoUid(60), SoUid(62)};

  std::sort(neighbors[SoUid(0)].begin(), neighbors[SoUid(0)].end());
  std::sort(neighbors[SoUid(5)].begin(), neighbors[SoUid(5)].end());
  std::sort(neighbors[SoUid(41)].begin(), neighbors[SoUid(41)].end());
  std::sort(neighbors[SoUid(61)].begin(), neighbors[SoUid(61)].end());

  EXPECT_EQ(expected_0, neighbors[SoUid(0)]);
  EXPECT_EQ(expected_5, neighbors[SoUid(5)]);
  EXPECT_EQ(expected_41, neighbors[SoUid(41)]);
  EXPECT_EQ(expected_61, neighbors[SoUid(61)]);
}

// TODO(lukas) Add tests for UniformGridEnvironment::ForEachNeighbor

TEST(GridTest, UpdateGrid) {
  Simulation simulation(TEST_NAME);
  auto* rm = simulation.GetResourceManager();
  auto* env = simulation.GetEnvironment();

  CellFactory(rm, 4);

  env->Update();

  // Remove cells 1 and 42
  rm->Remove(SoUid(1));
  rm->Remove(SoUid(42));

  EXPECT_EQ(62u, rm->GetNumSimObjects());

  RunUpdateGridTest(&simulation);
}

TEST(GridTest, NoRaceConditionDuringUpdate) {
  Simulation simulation(TEST_NAME);
  auto* rm = simulation.GetResourceManager();
  auto* env = simulation.GetEnvironment();

  CellFactory(rm, 4);

  // make sure that there are multiple cells per box
  rm->GetSimObject(SoUid(0))->SetDiameter(60);

  env->Update();

  // Remove cells 1 and 42
  rm->Remove(SoUid(1));
  rm->Remove(SoUid(42));

  // run 100 times to increase possibility of race condition due to different
  // scheduling of threads
  for (uint16_t i = 0; i < 100; i++) {
    RunUpdateGridTest(&simulation);
  }
}

TEST(GridTest, GetBoxIndex) {
  Simulation simulation(TEST_NAME);
  auto* rm = simulation.GetResourceManager();
  auto* grid =
      static_cast<UniformGridEnvironment*>(simulation.GetEnvironment());

  CellFactory(rm, 3);

  grid->Update();

  Double3 position_0 = {{0, 0, 0}};
  Double3 position_1 = {{1e-15, 1e-15, 1e-15}};
  Double3 position_2 = {{-1e-15, 1e-15, 1e-15}};

  size_t expected_idx_0 = 21;
  size_t expected_idx_1 = 21;
  size_t expected_idx_2 = 20;

  size_t idx_0 = grid->GetBoxIndex(position_0);
  size_t idx_1 = grid->GetBoxIndex(position_1);
  size_t idx_2 = grid->GetBoxIndex(position_2);

  EXPECT_EQ(expected_idx_0, idx_0);
  EXPECT_EQ(expected_idx_1, idx_1);
  EXPECT_EQ(expected_idx_2, idx_2);
}

TEST(GridTest, GridDimensions) {
  Simulation simulation(TEST_NAME);
  auto* rm = simulation.GetResourceManager();
  auto* env = simulation.GetEnvironment();

  CellFactory(rm, 3);

  env->Update();

  std::array<int32_t, 6> expected_dim_0 = {{-30, 90, -30, 90, -30, 90}};
  auto& dim_0 = env->GetDimensions();

  EXPECT_EQ(expected_dim_0, dim_0);

  rm->GetSimObject(SoUid(0))->SetPosition({{100, 0, 0}});
  env->Update();
  std::array<int32_t, 6> expected_dim_1 = {{-30, 150, -30, 90, -30, 90}};
  auto& dim_1 = env->GetDimensions();

  EXPECT_EQ(expected_dim_1, dim_1);
}

TEST(GridTest, GetBoxCoordinates) {
  Simulation simulation(TEST_NAME);
  auto* rm = simulation.GetResourceManager();
  auto* grid =
      static_cast<UniformGridEnvironment*>(simulation.GetEnvironment());

  CellFactory(rm, 3);

  // expecting a 4 * 4 * 4 grid
  grid->Update();

  EXPECT_ARR_EQ({3, 0, 0}, grid->GetBoxCoordinates(3));
  EXPECT_ARR_EQ({1, 2, 0}, grid->GetBoxCoordinates(9));
  EXPECT_ARR_EQ({1, 2, 3}, grid->GetBoxCoordinates(57));
}

TEST(GridTest, NonEmptyBoundedTestThresholdDimensions) {
  auto set_param = [](auto* param) {
    param->bound_space_ = true;
    param->min_bound_ = 1;
    param->max_bound_ = 99;
  };

  Simulation simulation(TEST_NAME, set_param);
  auto* rm = simulation.GetResourceManager();
  auto* env = simulation.GetEnvironment();

  rm->push_back(new Cell(10));

  env->Update();

  auto max_dimensions = env->GetDimensionThresholds();
  EXPECT_EQ(1, max_dimensions[0]);
  EXPECT_EQ(99, max_dimensions[1]);
}

struct ZOrderCallback : Functor<void, const SimObject*> {
  std::vector<std::set<SoUid>> zorder;
  uint64_t box_cnt = 0;
  uint64_t cnt = 0;
  ResourceManager* rm;
  SoUid ref_uid;

  ZOrderCallback(ResourceManager* rm, SoUid ref_uid)
      : rm(rm), ref_uid(ref_uid) {
    zorder.resize(8);
  }

  void operator()(const SimObject* so) {
    if (cnt == 8 || cnt == 12 || cnt == 16 || cnt == 18 || cnt == 22 ||
        cnt == 24 || cnt == 26) {
      box_cnt++;
    }
    zorder[box_cnt].insert(so->GetUid() - ref_uid);
    cnt++;
  }
};

TEST(GridTest, IterateZOrder) {
  Simulation simulation(TEST_NAME);
  auto* rm = simulation.GetResourceManager();
  auto* env = simulation.GetEnvironment();

  auto ref_uid = SoUid(simulation.GetSoUidGenerator()->GetHighestIndex());
  CellFactory(rm, 3);

  // expecting a 4 * 4 * 4 grid
  env->Update();

  ZOrderCallback callback(rm, ref_uid);
  env->IterateZOrder(callback);

  ASSERT_EQ(27u, callback.cnt);
  // check each box; no order within a box
  std::vector<std::set<SoUid>> expected(8);
  expected[0] = std::set<SoUid>{SoUid(0), SoUid(1),  SoUid(3),  SoUid(4),
                                SoUid(9), SoUid(10), SoUid(12), SoUid(13)};
  expected[1] = std::set<SoUid>{SoUid(2), SoUid(5), SoUid(11), SoUid(14)};
  expected[2] = std::set<SoUid>{SoUid(6), SoUid(7), SoUid(15), SoUid(16)};
  expected[3] = std::set<SoUid>{SoUid(8), SoUid(17)};
  expected[4] = std::set<SoUid>{SoUid(18), SoUid(19), SoUid(21), SoUid(22)};
  expected[5] = std::set<SoUid>{SoUid(20), SoUid(23)};
  expected[6] = std::set<SoUid>{SoUid(24), SoUid(25)};
  expected[7] = std::set<SoUid>{SoUid(26)};
  for (int i = 0; i < 8; i++) {
    EXPECT_EQ(expected[i], callback.zorder[i]);
  }
}

}  // namespace bdm
