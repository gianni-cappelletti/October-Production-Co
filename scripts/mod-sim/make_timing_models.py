import copy
import json
import random
import sys

SIZES = {"nano": (4, 2), "feather": (8, 4), "standard": (16, 8)}


def layer_array_weight_count(layer):
    channels = layer["channels"]
    head_size = layer["head_size"]
    kernel = layer["kernel_size"]
    per_layer = kernel * channels * channels + channels + layer["condition_size"] * channels \
        + channels * channels + channels
    return layer["input_size"] * channels + len(layer["dilations"]) * per_layer \
        + channels * head_size + (head_size if layer["head_bias"] else 0)


def main(template_path, output_dir):
    template = json.load(open(template_path))
    assert template["architecture"] == "WaveNet" and len(template["config"]["layers"]) == 2
    rng = random.Random(1)
    for name, (first, second) in SIZES.items():
        model = copy.deepcopy(template)
        layers = model["config"]["layers"]
        layers[0].update(channels=first, head_size=second)
        layers[1].update(input_size=first, channels=second)
        count = sum(layer_array_weight_count(layer) for layer in layers) + 1
        model["weights"] = [rng.uniform(-0.05, 0.05) for _ in range(count)]
        model["metadata"] = {"name": f"timing-{name}"}
        path = f"{output_dir}/timing-{name}.nam"
        json.dump(model, open(path, "w"))
        print(f"{path}: {count} weights")


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
