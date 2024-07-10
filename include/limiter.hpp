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

#include <pipewire/proxy.h>
#include <sigc++/signal.h>
#include <sys/types.h>
#include <span>
#include <string>
#include <vector>
#include "pipe_manager.hpp"
#include "plugin_base.hpp"

class Limiter : public PluginBase {
 public:
  Limiter(const std::string& tag,
          const std::string& schema,
          const std::string& schema_path,
          PipeManager* pipe_manager,
          PipelineType pipe_type);
  Limiter(const Limiter&) = delete;
  auto operator=(const Limiter&) -> Limiter& = delete;
  Limiter(const Limiter&&) = delete;
  auto operator=(const Limiter&&) -> Limiter& = delete;
  ~Limiter() override;

  void setup() override;

  void process(std::span<float>& left_in,
               std::span<float>& right_in,
               std::span<float>& left_out,
               std::span<float>& right_out,
               std::span<float>& probe_left,
               std::span<float>& probe_right) override;

  void update_probe_links() override;

  auto get_latency_seconds() -> float override;

  sigc::signal<void(const float)> gain_left, gain_right, sidechain_left, sidechain_right;

  float gain_l_port_value = 0.0F;
  float gain_r_port_value = 0.0F;
  float sidechain_l_port_value = 0.0F;
  float sidechain_r_port_value = 0.0F;

 private:
  uint latency_n_frames = 0U;

  std::vector<pw_proxy*> list_proxies;

  void update_sidechain_links(const std::string& key);
};
