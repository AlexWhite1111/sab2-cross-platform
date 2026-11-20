"""Exercise real ONNX inference and the upstream Vision graph on generated video."""

import csv
import io
import pathlib
import subprocess
import sys
import tempfile

import cv2
import numpy as np
import onnx
from onnx import TensorProto, helper


def make_fixture(directory):
    video = directory / "frames.avi"
    model = directory / "fixture.onnx"
    frame = np.zeros((64, 64, 3), dtype=np.uint8)
    writer = cv2.VideoWriter(str(video), cv2.VideoWriter_fourcc(*"MJPG"), 10, (64, 64))
    if not writer.isOpened():
        raise RuntimeError("Could not create the MJPEG replay fixture.")
    for index in range(3):
        frame[:, :, 2] = 32 * index
        writer.write(frame)
    writer.release()

    # The second candidate overlaps the first and must be suppressed by the Vision graph.
    boxes = [
        8, 8, 20, 22, 0.9, 0,
        9, 9, 21, 23, 0.8, 0,
        40, 20, 52, 32, 0.6, 1,
    ]
    output = helper.make_tensor("boxes", TensorProto.FLOAT, [1, 3, 6], boxes)
    graph = helper.make_graph(
        [helper.make_node("Constant", [], ["detections"], value=output)],
        "replay-fixture",
        [helper.make_tensor_value_info("image", TensorProto.FLOAT, [1, 3, 64, 64])],
        [helper.make_tensor_value_info("detections", TensorProto.FLOAT, [1, 3, 6])],
    )
    onnx.save(helper.make_model(graph, ir_version=9, opset_imports=[helper.make_opsetid("", 13)]), model)
    return video, model


def test_replay(executable):
    with tempfile.TemporaryDirectory(prefix="sab2-replay-") as directory:
        video, model = make_fixture(pathlib.Path(directory))
        run = subprocess.run([str(executable), str(video), str(model)], text=True, capture_output=True, timeout=30)
        if run.returncode:
            raise RuntimeError(f"Replay failed with exit {run.returncode}: {run.stderr}\n{run.stdout}")
        rows = list(csv.DictReader(io.StringIO(run.stdout)))
        assert len(rows) == 6, rows
        expected = [(0, 0.9, 8, 8, 12, 14), (1, 0.6, 40, 20, 12, 12)]
        for index in range(3):
            for row, (class_id, confidence, x, y, width, height) in zip(rows[2 * index:2 * index + 2], expected):
                assert int(row["frame"]) == index, row
                assert int(row["class_id"]) == class_id, row
                assert abs(float(row["confidence"]) - confidence) < 1e-6, row
                actual_box = tuple(float(row[key]) for key in ("x", "y", "width", "height"))
                assert actual_box == (x, y, width, height), row
        track_ids = {int(row["track_id"]) for row in rows}
        assert len(track_ids) == 1 and min(track_ids) > 0, rows
        assert all(row["track_observed"] == "1" for row in rows), rows
        assert len({(row["track_x"], row["track_y"]) for row in rows}) == 1, rows
        print("Verified 3 decoded frames, 6 output detections, class-aware NMS and a stable track ID.")


if __name__ == "__main__":
    test_replay(pathlib.Path(sys.argv[1]).resolve())
