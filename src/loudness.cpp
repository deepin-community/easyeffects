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

#include "loudness.hpp"
#include <sys/types.h>
#include <algorithm>
#include <memory>
#include <span>
#include <string>
#include "lv2_wrapper.hpp"
#include "pipe_manager.hpp"
#include "plugin_base.hpp"
#include "tags_plugin_name.hpp"
#include "util.hpp"

Loudness::Loudness(const std::string& tag,
                   const std::string& schema,
                   const std::string& schema_path,
                   PipeManager* pipe_manager,
                   PipelineType pipe_type)
    : PluginBase(tag,
                 tags::plugin_name::loudness,
                 tags::plugin_package::lsp,
                 schema,
                 schema_path,
                 pipe_manager,
                 pipe_type) {
  lv2_wrapper = std::make_unique<lv2::Lv2Wrapper>("http://lsp-plug.in/plugins/lv2/loud_comp_stereo");

  package_installed = lv2_wrapper->found_plugin;

  if (!package_installed) {
    util::debug(log_tag + "http://lsp-plug.in/plugins/lv2/loud_comp_stereo is not installed");
  }

  lv2_wrapper->bind_key_enum<"std", "std">(settings);

  lv2_wrapper->bind_key_enum<"fft", "fft">(settings);

  lv2_wrapper->bind_key_double<"volume", "volume">(settings);

  lv2_wrapper->bind_key_bool<"hclip", "clipping">(settings);

  lv2_wrapper->bind_key_double<"hcrange", "clipping-range">(settings);

  setup_input_output_gain();
}

Loudness::~Loudness() {
  if (connected_to_pw) {
    disconnect_from_pw();
  }

  util::debug(log_tag + name + " destroyed");
}

void Loudness::setup() {
  if (!lv2_wrapper->found_plugin) {
    return;
  }

  lv2_wrapper->set_n_samples(n_samples);

  if (lv2_wrapper->get_rate() != rate) {
    lv2_wrapper->create_instance(rate);
  }
}

void Loudness::process(std::span<float>& left_in,
                       std::span<float>& right_in,
                       std::span<float>& left_out,
                       std::span<float>& right_out) {
  if (!lv2_wrapper->found_plugin || !lv2_wrapper->has_instance() || bypass) {
    std::copy(left_in.begin(), left_in.end(), left_out.begin());
    std::copy(right_in.begin(), right_in.end(), right_out.begin());

    return;
  }

  if (input_gain != 1.0F) {
    apply_gain(left_in, right_in, input_gain);
  }

  lv2_wrapper->connect_data_ports(left_in, right_in, left_out, right_out);
  lv2_wrapper->run();

  if (output_gain != 1.0F) {
    apply_gain(left_out, right_out, output_gain);
  }

  /*
   This plugin gives the latency in number of samples
 */

  const auto lv = static_cast<uint>(lv2_wrapper->get_control_port_value("out_latency"));

  if (latency_n_frames != lv) {
    latency_n_frames = lv;

    latency_value = static_cast<float>(latency_n_frames) / static_cast<float>(rate);

    util::debug(log_tag + name + " latency: " + util::to_string(latency_value, "") + " s");

    util::idle_add([this]() {
      if (!post_messages || latency.empty()) {
        return;
      }

      latency.emit();
    });

    update_filter_params();
  }

  if (post_messages) {
    get_peaks(left_in, right_in, left_out, right_out);

    if (send_notifications) {
      notify();
    }
  }
}

auto Loudness::get_latency_seconds() -> float {
  return latency_value;
}
