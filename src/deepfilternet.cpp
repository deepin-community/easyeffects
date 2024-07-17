/*
 *  Copyright © 2023 Torge Matthies
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

#include "deepfilternet.hpp"
#include <algorithm>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <vector>
#include "ladspa_wrapper.hpp"
#include "pipe_manager.hpp"
#include "plugin_base.hpp"
#include "resampler.hpp"
#include "tags_plugin_name.hpp"
#include "util.hpp"

DeepFilterNet::DeepFilterNet(const std::string& tag,
                             const std::string& schema,
                             const std::string& schema_path,
                             PipeManager* pipe_manager,
                             PipelineType pipe_type)
    : PluginBase(tag,
                 tags::plugin_name::deepfilternet,
                 tags::plugin_package::deepfilternet,
                 schema,
                 schema_path,
                 pipe_manager,
                 pipe_type) {
  ladspa_wrapper = std::make_unique<ladspa::LadspaWrapper>("libdeep_filter_ladspa.so", "deep_filter_stereo");

  package_installed = ladspa_wrapper->found_plugin();

  if (!package_installed) {
    util::debug(log_tag + "libdeep_filter_ladspa is not installed");
  }

  ladspa_wrapper->bind_key_double_db_exponential<"Attenuation Limit (dB)", "attenuation-limit", false>(settings);

  ladspa_wrapper->bind_key_double_db_exponential<"Min processing threshold (dB)", "min-processing-threshold", false>(
      settings);

  ladspa_wrapper
      ->bind_key_double_db_exponential<"Max ERB processing threshold (dB)", "max-erb-processing-threshold", false>(
          settings);

  ladspa_wrapper
      ->bind_key_double_db_exponential<"Max DF processing threshold (dB)", "max-df-processing-threshold", false>(
          settings);

  ladspa_wrapper->bind_key_int<"Min Processing Buffer (frames)", "min-processing-buffer">(settings);

  ladspa_wrapper->bind_key_double<"Post Filter Beta", "post-filter-beta">(settings);

  setup_input_output_gain();
}

DeepFilterNet::~DeepFilterNet() {
  if (connected_to_pw) {
    disconnect_from_pw();
  }

  util::debug(log_tag + name + " destroyed");
}

void DeepFilterNet::setup() {
  std::scoped_lock<std::mutex> lock(data_mutex);

  if (!ladspa_wrapper->found_plugin()) {
    return;
  }

  resample = rate != 48000;
  resampler_ready = !resample;

  util::idle_add([&, this] {
    ladspa_wrapper->n_samples = n_samples;
    std::scoped_lock<std::mutex> lock(data_mutex);

    if (ladspa_wrapper->get_rate() != 48000) {
      ladspa_wrapper->create_instance(48000);
      ladspa_wrapper->activate();
    }

    if (resample && !resampler_ready) {
      resampler_inL = std::make_unique<Resampler>(rate, 48000);
      resampler_inR = std::make_unique<Resampler>(rate, 48000);
      resampler_outL = std::make_unique<Resampler>(48000, rate);
      resampler_outR = std::make_unique<Resampler>(48000, rate);

      std::vector<float> dummy(n_samples);

      const auto resampled_inL = resampler_inL->process(dummy, false);
      const auto resampled_inR = resampler_inR->process(dummy, false);

      resampled_outL.resize(resampled_inL.size());
      resampled_outR.resize(resampled_inR.size());

      resampler_outL->process(resampled_inL, false);
      resampler_outR->process(resampled_inR, false);

      carryover_l.clear();
      carryover_r.clear();
      carryover_l.reserve(4);  // chosen by fair dice roll.
      carryover_r.reserve(4);  // guaranteed to be random.
      carryover_l.push_back(0.0F);
      carryover_r.push_back(0.0F);

      resampler_ready = true;
    }
  });
}

void DeepFilterNet::process(std::span<float>& left_in,
                            std::span<float>& right_in,
                            std::span<float>& left_out,
                            std::span<float>& right_out) {
  std::scoped_lock<std::mutex> lock(data_mutex);

  if (!ladspa_wrapper->found_plugin() || !ladspa_wrapper->has_instance() || bypass) {
    std::copy(left_in.begin(), left_in.end(), left_out.begin());
    std::copy(right_in.begin(), right_in.end(), right_out.begin());

    return;
  }

  if (input_gain != 1.0F) {
    apply_gain(left_in, right_in, input_gain);
  }

  if (resample) {
    const auto& resampled_inL = resampler_inL->process(left_in, false);
    const auto& resampled_inR = resampler_inR->process(right_in, false);

    resampled_outL.resize(resampled_inL.size());
    resampled_outR.resize(resampled_inR.size());

    ladspa_wrapper->n_samples = resampled_inL.size();
    ladspa_wrapper->connect_data_ports(resampled_inL, resampled_inR, resampled_outL, resampled_outR);
  } else {
    ladspa_wrapper->connect_data_ports(left_in, right_in, left_out, right_out);
  }

  ladspa_wrapper->run();

  if (resample) {
    const auto& outL = resampler_outL->process(resampled_outL, false);
    const auto& outR = resampler_outR->process(resampled_outR, false);

    auto carryover_end_l = std::min(carryover_l.size(), left_out.size());
    auto carryover_end_r = std::min(carryover_r.size(), right_out.size());

    auto left_offset =
        carryover_end_l + outL.size() > left_out.size() ? carryover_end_l : left_out.size() - outL.size();
    auto right_offset =
        carryover_end_r + outR.size() > right_out.size() ? carryover_end_r : right_out.size() - outR.size();

    auto left_count = std::min(outL.size(), left_out.size() - left_offset);
    auto right_count = std::min(outR.size(), right_out.size() - right_offset);

    std::copy(carryover_l.begin(), carryover_l.begin() + carryover_end_l, left_out.begin());
    std::copy(carryover_r.begin(), carryover_r.begin() + carryover_end_r, right_out.begin());

    carryover_l.erase(carryover_l.begin(), carryover_l.begin() + carryover_end_l);
    carryover_r.erase(carryover_r.begin(), carryover_r.begin() + carryover_end_r);

    std::fill(left_out.begin() + carryover_end_l, left_out.begin() + left_offset, 0);
    std::fill(right_out.begin() + carryover_end_r, right_out.begin() + right_offset, 0);

    std::copy(outL.begin(), outL.begin() + left_count, left_out.begin() + left_offset);
    std::copy(outR.begin(), outR.begin() + right_count, right_out.begin() + right_offset);

    carryover_l.insert(carryover_l.end(), outL.begin() + left_count, outL.end());
    carryover_r.insert(carryover_r.end(), outR.begin() + right_count, outR.end());

    std::fill(left_out.begin() + left_offset + left_count, left_out.end(), 0);
    std::fill(right_out.begin() + right_offset + right_count, right_out.end(), 0);
  }

  if (output_gain != 1.0F) {
    apply_gain(left_out, right_out, output_gain);
  }

  if (post_messages) {
    get_peaks(left_in, right_in, left_out, right_out);

    if (send_notifications) {
      notify();
    }
  }
}

auto DeepFilterNet::get_latency_seconds() -> float {
  return 0.02F + 1.0F / rate;
}
