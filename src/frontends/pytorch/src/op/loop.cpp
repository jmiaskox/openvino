// Copyright (C) 2018-2023 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "openvino/op/loop.hpp"

#include "openvino/frontend/pytorch/node_context.hpp"
#include "utils.hpp"

namespace ov {
namespace frontend {
namespace pytorch {
namespace op {

OutputVector translate_loop(NodeContext& context) {
    const auto& inputs = context.inputs();
    FRONT_END_OP_CONVERSION_CHECK(inputs.size() >= 2, "Loop must have at least 2 inputs.");
    auto loop = std::make_shared<ov::op::v5::Loop>(inputs[0], inputs[1]);
    auto decoder = context.get_decoder();
    FRONT_END_OP_CONVERSION_CHECK(decoder->get_subgraph_size() == 1, "Loop must have 1 subgraph.");
    auto subgraph_decoder = decoder->get_subgraph_decoder(0);
    auto body = context.convert_subgraph(0);
    loop->set_function(body);
    ov::op::v5::Loop::SpecialBodyPorts spec_ports{0, 0};
    loop->set_special_body_ports(spec_ports);

    auto body_parameters = body->get_parameters();
    // #0 body parameter is counter; #0 loop input is counter, #1 loop input is condition
    // Connect other inputs
    for (size_t i = 2; i < inputs.size(); i++) {
        loop->set_invariant_inputs(inputs[i], {body_parameters[i - 1]});
    }
    // Connect inputs from external context
    for (auto i = inputs.size() - 1; i < body_parameters.size(); i++) {
        auto param = body_parameters[i];
        auto name = param->get_output_tensor(0).get_any_name();
        size_t input_idx = (size_t)std::stoll(name);
        auto external_output = context.get_tensor_from_model_or_create_input(input_idx);
        loop->set_invariant_inputs(external_output, {param});
    }
    // TODO: Connect back edges (merged inputs)
    auto body_results = body->get_results();
    FRONT_END_OP_CONVERSION_CHECK(body_results.size() > 0, "At least one output from loop is required - condition.");
    std::set<size_t> output_idxs;
    // 0 output is condition, do not need to connect it
    for (size_t i = 1; i < body_results.size(); i++) {
        auto result = body_results[i];
        auto name = result->input(0).get_tensor().get_any_name();
        size_t out_idx = (size_t)std::stoll(name);
        FRONT_END_OP_CONVERSION_CHECK(output_idxs.count(out_idx) == 0,
                                      "More then one body output with same tensor name.");
        output_idxs.insert(out_idx);
        context.add_tensor_to_context(out_idx, loop->get_iter_value(result, -1));
    }
    loop->validate_and_infer_types();
    return {context.mark_node(loop)->outputs()};
};

}  // namespace op
}  // namespace pytorch
}  // namespace frontend
}  // namespace ov
