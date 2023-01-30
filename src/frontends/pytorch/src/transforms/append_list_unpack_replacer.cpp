// Copyright (C) 2018-2023 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "append_list_unpack_replacer.hpp"

#include <memory>
#include <utility>

#include "openvino/core/rt_info.hpp"
#include "openvino/op/constant.hpp"
#include "openvino/op/split.hpp"
#include "openvino/op/squeeze.hpp"
#include "openvino/op/util/framework_node.hpp"
#include "openvino/pass/pattern/matcher.hpp"
#include "openvino/pass/pattern/op/wrap_type.hpp"
#include "utils.hpp"

namespace ov {
namespace frontend {
namespace pytorch {
namespace pass {

AppendListUnpackReplacer::AppendListUnpackReplacer() {
    auto list_unpack = ov::pass::pattern::wrap_type<ov::op::util::FrameworkNode>();

    ov::matcher_pass_callback callback = [](ov::pass::pattern::Matcher& m) {
        auto list_unpack = cast_fw_node(m.get_match_root(), "prim::ListUnpack");
        if (!list_unpack)
            return false;

        OutputVector tmp_inputs;
        NodeVector rt_copy_from{list_unpack};
        auto input_node = list_unpack->input_value(0).get_node_shared_ptr();

        // Optional aten::__getitem__ node.
        auto getitem_node = cast_fw_node(input_node, "aten::__getitem__");
        if (getitem_node) {
            rt_copy_from.push_back(getitem_node);
            input_node = getitem_node->input(0).get_source_output().get_node_shared_ptr();
        }

        while (auto append_node = cast_fw_node(input_node, "aten::append")) {
            rt_copy_from.push_back(append_node);
            tmp_inputs.push_back(append_node->input(1).get_source_output());
            input_node = append_node->input(0).get_source_output().get_node_shared_ptr();
        }
        OutputVector inputs;
        auto list_construct_node = cast_fw_node(input_node, "prim::ListConstruct");
        if (!list_construct_node) {
            return false;
        }
        rt_copy_from.push_back(list_construct_node);
        for (auto& input : list_construct_node->inputs()) {
            inputs.push_back(input.get_source_output());
        }

        inputs.insert(inputs.end(), tmp_inputs.rbegin(), tmp_inputs.rend());
        if (getitem_node) {
            // If aten::__getitem__, expect inputs to be equivalent of pytorch Tensor[][].
            // Tensor selected by aten::__getitem__ index needs to be splitted in axis 0.
            auto getitem_index_ptr = getitem_node->input_value(1).get_node_shared_ptr();
            auto getitem_index_const = std::dynamic_pointer_cast<ov::op::v0::Constant>(getitem_index_ptr);
            auto index_val = getitem_index_const->cast_vector<int64_t>();
            auto index = 0;
            if (index_val[0] >= 0) {
                index = index_val[0];
            } else {
                index = inputs.size() + index_val[0];
            }
            auto axis_0 = ov::op::v0::Constant::create(element::i64, Shape{}, {0});
            auto split = std::make_shared<ov::op::v1::Split>(inputs[index], axis_0, list_unpack->get_output_size());
            NodeVector to_copy_rt{axis_0, split};
            OutputVector res;
            for (auto output : split->outputs()) {
                auto squeeze = std::make_shared<ov::op::v0::Squeeze>(output, axis_0);
                to_copy_rt.push_back(squeeze);
                res.push_back(squeeze);
            }
            copy_runtime_info(rt_copy_from, to_copy_rt);
            replace_node(list_unpack, res);
            return true;
        } else {
            // Without aten::__getitem__, expect inputs to be equivalent od pytorch Tensor[].
            // Return all inputs.
            replace_node(list_unpack, inputs);
            return true;
        }
        return false;
    };

    auto m = std::make_shared<ov::pass::pattern::Matcher>(list_unpack,
                                                          "ov::frontend::pytorch::pass::AppendListUnpackReplacer");
    this->register_matcher(m, callback);
};

}  // namespace pass
}  // namespace pytorch
}  // namespace frontend
}  // namespace ov
