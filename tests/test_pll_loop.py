import math
import unittest

from simulation.twin.pll import PllConfig, PllState, reset, run_pll, step


class PllTests(unittest.TestCase):
    def test_locks_near_50_and_49_hertz(self) -> None:
        nominal = run_pll(frequency_hz=50.0, duration_s=0.6, dt_s=1.0e-4)
        offset = run_pll(frequency_hz=49.0, duration_s=1.0, dt_s=1.0e-4)
        self.assertTrue(nominal.locked)
        self.assertTrue(offset.locked)
        self.assertAlmostEqual(nominal.mean_frequency_hz, 50.0, delta=0.05)
        self.assertAlmostEqual(offset.mean_frequency_hz, 49.0, delta=0.05)
        self.assertLess(abs(nominal.final_vq), 0.2)
        self.assertTrue(all(0.0 <= time_s for time_s in nominal.time_s))
        self.assertLessEqual(nominal.time_s[-1], 0.6 + 1e-9)

    def test_dc_does_not_lock(self) -> None:
        config = PllConfig()
        state = PllState(frequency_hz=config.nominal_hz)
        for _ in range(5000):
            self.assertTrue(step(state, config, 1.0, 1.0e-4))
            self.assertGreaterEqual(state.frequency_hz, config.nominal_hz - config.frequency_limit_hz)
            self.assertLessEqual(state.frequency_hz, config.nominal_hz + config.frequency_limit_hz)
        self.assertFalse(state.locked)

    def test_rejected_step_keeps_frequency(self) -> None:
        config = PllConfig()
        state = PllState(frequency_hz=config.nominal_hz)
        reset(state, config)
        self.assertTrue(step(state, config, 0.0, 1.0e-4))
        held = state.frequency_hz
        self.assertFalse(step(state, config, math.nan, 1.0e-4))
        self.assertFalse(step(state, config, 0.0, 0.0))
        self.assertEqual(state.frequency_hz, held)
        self.assertFalse(PllConfig(sogi_k=0.0).valid())
        with self.assertRaises(ValueError):
            run_pll(frequency_hz=0.0)

    def test_theta_stays_wrapped(self) -> None:
        result_state = PllState(frequency_hz=50.0)
        config = PllConfig()
        reset(result_state, config)
        for index in range(3000):
            voltage = math.cos(math.tau * 50.0 * index * 1.0e-4)
            self.assertTrue(step(result_state, config, voltage, 1.0e-4))
            self.assertGreaterEqual(result_state.theta_rad, 0.0)
            self.assertLess(result_state.theta_rad, math.tau + 1e-6)


if __name__ == "__main__":
    unittest.main()
