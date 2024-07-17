/*
 *  Copyright © 2017-2024 Wellington Wallace
 *
 *  This file is part of Easy Effects.
 *
 *  Easy Effects is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Easy Effects is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Easy Effects. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <samplerate.h>
#include <cmath>
#include <vector>

class Resampler {
 public:
  Resampler(const int& input_rate, const int& output_rate);
  Resampler(const Resampler&) = delete;
  auto operator=(const Resampler&) -> Resampler& = delete;
  Resampler(const Resampler&&) = delete;
  auto operator=(const Resampler&&) -> Resampler& = delete;
  ~Resampler();

  template <typename T>
  auto process(const T& input, const bool& end_of_input) -> const std::vector<float>& {
    output.resize(std::ceil(1.5 * resample_ratio * input.size()));

    // The number of frames of data pointed to by data_in
    src_data.input_frames = input.size();

    // A pointer to the input data samples
    src_data.data_in = input.data();

    // Maximum number of frames pointed to by data_out
    src_data.output_frames = static_cast<long>(output.size());

    // A pointer to the output data samples
    src_data.data_out = output.data();

    // Equal to output_sample_rate / input_sample_rate
    src_data.src_ratio = resample_ratio;

    // Equal to 0 if more input data is available and 1 otherwise
    src_data.end_of_input = static_cast<int>(end_of_input);

    src_process(src_state, &src_data);

    output.resize(src_data.output_frames_gen);

    return output;
  }

 private:
  double resample_ratio = 1.0;

  SRC_STATE* src_state = nullptr;

  SRC_DATA src_data{};

  std::vector<float> output;
};
