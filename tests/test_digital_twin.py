import tempfile
import unittest
from pathlib import Path

from simulation.simulate import run
from simulation.twin.loop import LoopConfig, run_current_loop, sample_times
from simulation.twin.plotting import write_series_svg
from simulation.twin.protection import FAULT_OVERTEMPERATURE, FAULT_OVERVOLTAGE
from simulation.twin.tuning import suggest_current_gains


class DigitalTwinTests(unittest.TestCase):
    def test_sample_times_do_not_run_past_the_duration(self) -> None:
        self.assertEqual(sample_times(0.15, 0.1), (0.0, 0.1))
        self.assertAlmostEqual(sample_times(0.3, 0.1)[-1], 0.3, places=9)
        self.assertEqual(len(sample_times(0.5, 0.001)), 501)
        self.assertEqual(sample_times(0.5, 0.001)[-1], 0.5)
        with self.assertRaises(ValueError):
            sample_times(0.1, 0.2)
        with self.assertRaises(ValueError):
            sample_times(0.0, 0.001)

    def test_default_trace_matches_the_historical_demonstration(self) -> None:
        result = run_current_loop(LoopConfig())
        first = result.samples[0]
        last = result.samples[-1]
        self.assertEqual(f"{first.time_s:.6f}", "0.000000")
        self.assertEqual(f"{first.reference_a:.6f}", "0.005000")
        self.assertEqual(f"{first.current_a:.6f}", "0.000044")
        self.assertEqual(f"{first.duty:.6f}", "0.000440")
        self.assertEqual(f"{last.time_s:.6f}", "0.500000")
        self.assertAlmostEqual(last.current_a, 0.999398, places=5)
        self.assertTrue(result.metrics.stable)
        self.assertIsNotNone(result.metrics.steady_state_error_a)
        assert result.metrics.steady_state_error_a is not None
        self.assertLess(abs(result.metrics.steady_state_error_a), 0.01)

    def test_protection_holds_zero_duty(self) -> None:
        overvoltage = run_current_loop(LoopConfig(bus_voltage_v=40.0, duration_s=0.01))
        overtemp = run_current_loop(LoopConfig(temperature_c=90.0, duration_s=0.01))
        self.assertTrue(overvoltage.metrics.final_faults & FAULT_OVERVOLTAGE)
        self.assertTrue(overtemp.metrics.final_faults & FAULT_OVERTEMPERATURE)
        self.assertTrue(all(sample.duty == 0.0 for sample in overvoltage.samples))
        self.assertFalse(overvoltage.metrics.stable)

    def test_suggestion_settles_on_the_default_plant(self) -> None:
        gains = suggest_current_gains(2.0, 0.02, 0.001)
        result = run_current_loop(
            LoopConfig(
                kp=gains["kp"],
                ki=gains["ki"],
                duration_s=0.6,
                reference_a=1.0,
            )
        )
        self.assertGreater(gains["kp"], 0.0)
        self.assertGreater(gains["ki"], 0.0)
        self.assertTrue(result.metrics.stable)
        with self.assertRaises(ValueError):
            suggest_current_gains(0.0, 0.02, 0.001)
        with self.assertRaises(ValueError):
            suggest_current_gains(2.0, 0.02, 0.001, crossover_hz=10_000.0)

    def test_plot_and_script_write_expected_files(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            svg = directory / "wave.svg"
            csv_path = directory / "output.csv"
            write_series_svg(svg, [0.0, 0.1, 0.2], {"current_a": [0.0, 0.4, 0.8]}, title="A & B")
            text = svg.read_text(encoding="utf-8")
            self.assertTrue(text.startswith("<?xml"))
            self.assertIn("<svg", text)
            self.assertIn("<polyline", text)
            self.assertIn("A &amp; B", text)
            run(0.02, 0.001, csv_path)
            rows = csv_path.read_text(encoding="utf-8").splitlines()
            self.assertEqual(rows[0], "time_s,reference_a,current_a,duty")
            self.assertLessEqual(float(rows[-1].split(",")[0]), 0.02)
            with self.assertRaises(ValueError):
                write_series_svg(svg, [], {"current_a": []}, title="empty")


if __name__ == "__main__":
    unittest.main()
