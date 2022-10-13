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

#include "core/visualization/paraview/vtk_diffusion_grid.h"
// ParaView
#include <vtkCPDataDescription.h>
#include <vtkCPInputDataDescription.h>
#include <vtkDoubleArray.h>
#include <vtkExtentTranslator.h>
#include <vtkFloatArray.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
// BioDynaMo
#include "core/param/param.h"
#include "core/simulation.h"
#include "core/util/log.h"
#include "core/util/thread_info.h"
#include "core/visualization/paraview/parallel_vti_writer.h"

namespace bdm {

using vtkRealArray =
    typename type_ternary_operator<std::is_same<real_t, double>::value,
                                   vtkDoubleArray, vtkFloatArray>::type;

// -----------------------------------------------------------------------------
VtkDiffusionGrid::VtkDiffusionGrid(const std::string& name,
                                   vtkCPDataDescription* data_description) {
  auto* param = Simulation::GetActive()->GetParam();
  if (param->export_visualization) {
    auto* tinfo = ThreadInfo::GetInstance();
    data_.resize(tinfo->GetMaxThreads());
    piece_boxes_z_.resize(tinfo->GetMaxThreads());
  } else {
    data_.resize(1);
    piece_boxes_z_.resize(1);
  }

#pragma omp parallel for schedule(static, 1)
  for (uint64_t i = 0; i < data_.size(); ++i) {
    data_[i] = vtkImageData::New();
  }
  name_ = name;

  // get visualization config
  const Param::VisualizeDiffusion* vd = nullptr;
  for (auto& entry : param->visualize_diffusion) {
    if (entry.name == name) {
      vd = &entry;
      break;
    }
  }

  // If statement to prevent possible dereferencing of nullptr
  if (vd) {
    for (uint64_t i = 0; i < data_.size(); ++i) {
      // Add attribute data
      if (vd->concentration) {
        vtkNew<vtkRealArray> concentration;
        concentration->SetName("Substance Concentration");
        concentration_array_idx_ =
            data_[i]->GetPointData()->AddArray(concentration.GetPointer());
      }
      if (vd->gradient) {
        vtkNew<vtkRealArray> gradient;
        gradient->SetName("Diffusion Gradient");
        gradient->SetNumberOfComponents(3);
        gradient_array_idx_ =
            data_[i]->GetPointData()->AddArray(gradient.GetPointer());
      }
    }

    if (!param->export_visualization) {
      data_description->AddInput(name.c_str());
      data_description->GetInputDescriptionByName(name.c_str())
          ->SetGrid(data_[0]);
    }
  } else {
    Log::Warning("VtkDiffusionGrid::VtkDiffusionGrid", "Variable `name` (",
                 name, ") not found in `param->visualize_diffusion`.");
  }
}

// -----------------------------------------------------------------------------
VtkDiffusionGrid::~VtkDiffusionGrid() {
  name_ = "";
  for (auto& el : data_) {
    el->Delete();
  }
  data_.clear();
}

// -----------------------------------------------------------------------------
bool VtkDiffusionGrid::IsUsed() const { return used_; }

// -----------------------------------------------------------------------------
// ToDo (tobias):
// * remove debug code (print statements)
// * remove now unnecessary branching
// * add comments
void VtkDiffusionGrid::Update(const DiffusionGrid* grid) {
  used_ = true;

  auto num_boxes = grid->GetNumBoxesArray();
  auto grid_dimensions = grid->GetDimensions();
  auto box_length = grid->GetBoxLength();
  auto total_boxes = grid->GetNumBoxes();

  auto* tinfo = ThreadInfo::GetInstance();
  whole_extent_ = {{0, std::max(static_cast<int>(num_boxes[0]) - 1, 0), 0,
                    std::max(static_cast<int>(num_boxes[1]) - 1, 0), 0,
                    std::max(static_cast<int>(num_boxes[2]) - 1, 0)}};
  Dissect(num_boxes[2], tinfo->GetMaxThreads());
  CalcPieceExtents(num_boxes);
  uint64_t xy_num_boxes = num_boxes[0] * num_boxes[1];
  real_t origin_x = grid_dimensions[0];
  real_t origin_y = grid_dimensions[2];
  real_t origin_z = grid_dimensions[4];

  // do not partition data for insitu visualization
  if (data_.size() == 1) {
    data_[0]->SetOrigin(origin_x, origin_y, origin_z);
    data_[0]->SetDimensions(num_boxes[0], num_boxes[1], num_boxes[2]);
    data_[0]->SetSpacing(box_length, box_length, box_length);

    if (concentration_array_idx_ != -1) {
      auto* co_ptr = const_cast<real_t*>(grid->GetAllConcentrations());
      auto elements = static_cast<vtkIdType>(total_boxes);
      auto* array = static_cast<vtkRealArray*>(
          data_[0]->GetPointData()->GetArray(concentration_array_idx_));
      array->SetArray(co_ptr, elements, 1);
    }
    if (gradient_array_idx_ != -1) {
      auto gr_ptr = const_cast<real_t*>(grid->GetAllGradients());
      auto elements = static_cast<vtkIdType>(total_boxes * 3);
      auto* array = static_cast<vtkRealArray*>(
          data_[0]->GetPointData()->GetArray(gradient_array_idx_));
      array->SetArray(gr_ptr, elements, 1);
    }
    return;
  }

#pragma omp parallel for schedule(static, 1)
  for (uint64_t i = 0; i < piece_boxes_z_.size(); ++i) {
    uint64_t piece_elements;
    auto* e = piece_extents_[i].data();
    if (i < piece_boxes_z_.size() - 1) {
      piece_elements = piece_boxes_z_[i] * xy_num_boxes;
      data_[i]->SetDimensions(num_boxes[0], num_boxes[1], piece_boxes_z_[i]);
      data_[i]->SetExtent(e[0], e[1], e[2], e[3], e[4],
                          e[4] + piece_boxes_z_[i] - 1);
    } else {
      piece_elements = piece_boxes_z_[i] * xy_num_boxes;
      data_[i]->SetDimensions(num_boxes[0], num_boxes[1], piece_boxes_z_[i]);
      data_[i]->SetExtent(e[0], e[1], e[2], e[3], e[4],
                          e[4] + piece_boxes_z_[i] - 1);
    }
    // Compute partial sum of boxes until index i
    auto sum =
        std::accumulate(piece_boxes_z_.begin(), piece_boxes_z_.begin() + i, 0);
    real_t piece_origin_z = origin_z + box_length * sum;
    data_[i]->SetOrigin(origin_x, origin_y, piece_origin_z);
    data_[i]->SetSpacing(box_length, box_length, box_length);
    // // Debug
    // std::cout << "sum: " << sum << std::endl;
    // std::cout << "piece_origin_z : " << piece_origin_z << std::endl;

    if (concentration_array_idx_ != -1) {
      auto* co_ptr = const_cast<real_t*>(grid->GetAllConcentrations());
      auto elements = static_cast<vtkIdType>(piece_elements);
      auto* array = static_cast<vtkRealArray*>(
          data_[i]->GetPointData()->GetArray(concentration_array_idx_));
      auto offset = sum * xy_num_boxes;
      if (i < piece_boxes_z_.size() - 1) {
        // // Debug
        // std::cout << "Offset : " << offset << std::endl;
        // std::cout << "Value (begin) : " << co_ptr[offset] << std::endl;
        // std::cout << "Value (end): " << co_ptr[offset + elements - 1]
        //           << std::endl;
        array->SetArray(co_ptr + offset, elements, 1);
      } else {
        // // Debug
        // std::cout << "Offset : " << offset << std::endl;
        // std::cout << "Value (begin) : " << co_ptr[offset] << std::endl;
        // std::cout << "Value (end): " << co_ptr[offset + elements - 1]
        //           << std::endl;
        array->SetArray(co_ptr + offset, elements, 1);
      }
      // // Debug
      // std::cout << "elements : " << elements << std::endl;
    }
    if (gradient_array_idx_ != -1) {
      auto gr_ptr = const_cast<real_t*>(grid->GetAllGradients());
      auto elements = static_cast<vtkIdType>(piece_elements * 3);
      auto* array = static_cast<vtkRealArray*>(
          data_[i]->GetPointData()->GetArray(gradient_array_idx_));
      if (i < piece_boxes_z_.size() - 1) {
        array->SetArray(gr_ptr + (elements * i), elements, 1);
      } else {
        array->SetArray(gr_ptr + total_boxes - elements, elements, 1);
      }
    }
  }
}

// -----------------------------------------------------------------------------
void VtkDiffusionGrid::WriteToFile(uint64_t step) const {
  auto* sim = Simulation::GetActive();
  auto filename_prefix = Concat(name_, "-", step);

  ParallelVtiWriter writer;
  writer(sim->GetOutputDir(), filename_prefix, data_, piece_boxes_z_.size(),
         whole_extent_, piece_extents_);
}

// -----------------------------------------------------------------------------
void VtkDiffusionGrid::Dissect(uint64_t boxes_z, uint64_t num_pieces_target) {
  auto boxes_per_piece = static_cast<real_t>(boxes_z) / num_pieces_target;
  auto min_slices = static_cast<uint64_t>(std::floor(boxes_per_piece));
  auto leftover = boxes_z % num_pieces_target;
  // // Debug print info of all variables
  // std::cout << "boxes_z : " << boxes_z << std::endl;
  // std::cout << "num_pieces_target : " << num_pieces_target << std::endl;
  // std::cout << "boxes_per_piece : " << boxes_per_piece << std::endl;
  // std::cout << "min_slices : " << min_slices << std::endl;
  // std::cout << "leftover : " << leftover << std::endl;
  // Distribute leftover slices
  for (uint64_t i = 0; i < piece_boxes_z_.size(); ++i) {
    piece_boxes_z_[i] = min_slices;
    if (leftover > 0) {
      piece_boxes_z_[i]++;
      leftover--;
    }
  }
  // Shorten vector by removing all tailing zeros
  auto it = std::find(piece_boxes_z_.begin(), piece_boxes_z_.end(), 0);
  if (it != piece_boxes_z_.end()) {
    piece_boxes_z_.resize(std::distance(piece_boxes_z_.begin(), it));
  }

  // Check dissection
  auto sum = std::accumulate(piece_boxes_z_.begin(), piece_boxes_z_.end(), 0);
  assert(sum == boxes_z);
  // // Debug: Calculate sum of all entries
  // std::cout << "Sum : " << sum << std::endl;
  // std::cout << "boxes_z : " << boxes_z << std::endl;

  // // Debug print the vector
  // std::cout << "piece_boxes_z_ : ";
  // for (auto i : piece_boxes_z_) {
  //   std::cout << i << " ";
  // }
  // std::cout << std::endl;
}

// -----------------------------------------------------------------------------
void VtkDiffusionGrid::CalcPieceExtents(
    const std::array<size_t, 3>& num_boxes) {
  piece_extents_.resize(piece_boxes_z_.size());
  if (piece_boxes_z_.size() == 1) {
    piece_extents_[0] = whole_extent_;
    return;
  }
  int c = piece_boxes_z_[0];
  piece_extents_[0] = {{0, static_cast<int>(num_boxes[0]) - 1, 0,
                        static_cast<int>(num_boxes[1]) - 1, 0, c}};
  for (uint64_t i = 1; i < piece_boxes_z_.size() - 1; ++i) {
    piece_extents_[i] = {{0, static_cast<int>(num_boxes[0]) - 1, 0,
                          static_cast<int>(num_boxes[1]) - 1, c,
                          c + static_cast<int>(piece_boxes_z_[i])}};
    c += piece_boxes_z_[i];
  }
  piece_extents_.back() = {{0, static_cast<int>(num_boxes[0]) - 1, 0,
                            static_cast<int>(num_boxes[1]) - 1, c,
                            static_cast<int>(num_boxes[2]) - 1}};
}

}  // namespace bdm
