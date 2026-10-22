import json
import tempfile
import unittest
from contextlib import redirect_stdout
from io import StringIO
from pathlib import Path

from simulation.tune import main


def _run(argv: list[str]) -> tuple[int, dict | str]:
    output = StringIO()
    with redirect_stdout(output):
        code = main(argv)
    text = output.getvalue()
    try:
        return code, json.loads(text)
    except json.JSONDecodeError:
        return code, text


class TuneCliTests(unittest.TestCase):
    def test_schema_is_machine_readable(self) -> None:
        code, payload = _run(["schema"])
        self.assertIsInstance(payload, dict)
        assert isinstance(payload, dict)
        self.assertEqual(code, 0)
        self.assertTrue(payload["ok"])
        self.assertEqual(payload["schema_version"], 1)
        self.assertEqual(payload["fault_bits"]["OVERCURRENT"], 1)
        self.assertEqual(payload["fault_bits"]["COMM_TIMEOUT"], 1 << 8)
        self.assertIn("run", payload["commands"])
        self.assertIn("does not arm hardware", payload["safety"])

    def test_run_writes_json_csv_and_plot(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            code, text = _run([
                "run",
                "--duration",
                "0.05",
                "--output-dir",
                str(directory),
                "--human",
            ])
            self.assertEqual(code, 0)
            self.assertIsInstance(text, str)
            assert isinstance(text, str)
            self.assertIn("stable=", text)
            self.assertTrue((directory / "current_loop.csv").is_file())
            self.assertIn("<polyline", (directory / "current_loop.svg").read_text(encoding="utf-8"))

    def test_require_stable_and_invalid_config(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            code, payload = _run([
                "run",
                "--kp",
                "0",
                "--ki",
                "0",
                "--duration",
                "0.05",
                "--output-dir",
                str(directory),
                "--require-stable",
                "--no-plot",
            ])
            self.assertIsInstance(payload, dict)
            assert isinstance(payload, dict)
            self.assertEqual(code, 1)
            self.assertFalse(payload["ok"])
            self.assertFalse(payload["metrics"]["stable"])
            self.assertEqual(payload["artifacts"]["plot"], "")
            self.assertFalse((directory / "current_loop.svg").exists())
            self.assertTrue((directory / "current_loop.csv").is_file())

            rejected, error = _run(["run", "--duration", "-1", "--output-dir", str(directory)])
            self.assertIsInstance(error, dict)
            assert isinstance(error, dict)
            self.assertEqual(rejected, 2)
            self.assertFalse(error["ok"])

    def test_sweep_suggest_and_pll(self) -> None:
        code, sweep = _run([
            "sweep",
            "--kp",
            "0.08,0.2",
            "--ki",
            "8",
            "--duration",
            "0.2",
        ])
        self.assertIsInstance(sweep, dict)
        assert isinstance(sweep, dict)
        self.assertEqual(code, 0)
        self.assertEqual(sweep["count"], 2)
        self.assertIn(sweep["best"]["kp"], (0.08, 0.2))

        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            suggest_code, suggestion = _run([
                "suggest",
                "--duration",
                "0.6",
                "--output-dir",
                str(directory),
                "--require-stable",
            ])
            self.assertIsInstance(suggestion, dict)
            assert isinstance(suggestion, dict)
            self.assertEqual(suggest_code, 0)
            self.assertTrue(suggestion["metrics"]["stable"])
            self.assertGreater(suggestion["kp"], 0.0)
            self.assertIn("<polyline", (directory / "suggested_loop.svg").read_text(encoding="utf-8"))

            pll_code, pll = _run([
                "pll",
                "--frequency",
                "49",
                "--duration",
                "0.8",
                "--output-dir",
                str(directory),
            ])
            self.assertIsInstance(pll, dict)
            assert isinstance(pll, dict)
            self.assertEqual(pll_code, 0)
            self.assertTrue(pll["locked"])
            self.assertAlmostEqual(pll["mean_frequency_hz"], 49.0, delta=0.05)
            self.assertTrue((directory / "pll.svg").is_file())

        limited_code, limited = _run([
            "sweep",
            "--kp",
            "1,2,3,4,5,6,7,8",
            "--ki",
            "1,2,3,4,5,6,7,8",
            "--duration",
            "0.05",
        ])
        self.assertIsInstance(limited, dict)
        assert isinstance(limited, dict)
        self.assertEqual(limited_code, 2)
        self.assertIn("limited", limited["error"])


if __name__ == "__main__":
    unittest.main()
