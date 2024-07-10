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

#include <sigc++/signal.h>
#include <span>
#include <string>
#include "pipe_manager.hpp"
#include "plugin_base.hpp"

class Deesser : public PluginBase {
 public:
  Deesser(const std::string& tag,
          const std::string& schema,
          const std::string& schema_path,
          PipeManager* pipe_manager,
          PipelineType pipe_type);
  Deesser(const Deesser&) = delete;
  auto operator=(const Deesser&) -> Deesser& = delete;
  Deesser(const Deesser&&) = delete;
  auto operator=(const Deesser&&) -> Deesser& = delete;
  ~Deesser() override;

  void setup() override;

  void process(std::span<float>& left_in,
               std::span<float>& right_in,
               std::span<float>& left_out,
               std::span<float>& right_out) override;

  auto get_latency_seconds() -> float override;

  sigc::signal<void(const double)> compression, detected;

  double compression_port_value = 0.0;
  double detected_port_value = 0.0;

 private:
};
