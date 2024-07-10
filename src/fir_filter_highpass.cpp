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

#include "fir_filter_highpass.hpp"
#include <algorithm>
#include <string>
#include <utility>
#include "fir_filter_base.hpp"

FirFilterHighpass::FirFilterHighpass(std::string tag) : FirFilterBase(std::move(tag)) {}

FirFilterHighpass::~FirFilterHighpass() = default;

void FirFilterHighpass::setup() {
  kernel = create_lowpass_kernel(min_frequency, transition_band);

  std::ranges::for_each(kernel, [](auto& v) { v *= -1.0F; });

  kernel[(kernel.size() - 1U) / 2U] += 1.0F;

  delay = 0.5F * static_cast<float>(kernel.size() - 1U) / static_cast<float>(rate);

  setup_zita();
}
