import onnx
from onnx import helper, TensorProto
import sys
# Deterministic segmentation-shaped graph for testing the real ORT call.
x = helper.make_tensor_value_info("rgb", TensorProto.FLOAT, [1, 3, 320, 320])
y = helper.make_tensor_value_info("mask", TensorProto.FLOAT, [1, 1, 320, 320])
nodes = [
    helper.make_node("ReduceMean", ["rgb"], ["mean"], axes=[1], keepdims=1),
    helper.make_node("Sigmoid", ["mean"], ["mask"]),
]
model = helper.make_model(helper.make_graph(nodes, "test-mask", [x], [y]), opset_imports=[helper.make_opsetid("", 13)], ir_version=9)
onnx.checker.check_model(model)
onnx.save(model, sys.argv[1])
