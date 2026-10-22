import math
import unittest

from simulation.twin.pid import PIDController, PIDGains


class PIDTests(unittest.TestCase):
    def test_pi_step_matches_the_firmware_formula(self) -> None:
        controller = PIDController()
        duty = controller.step(0.005, 0.0, 0.001)
        self.assertAlmostEqual(duty, 0.00044, places=6)
        self.assertAlmostEqual(controller.integrator, 0.00004, places=7)
        duty = controller.step(0.005, 0.0, 0.001)
        self.assertAlmostEqual(duty, 0.00048, places=6)
        self.assertAlmostEqual(controller.integrator, 0.00008, places=7)

    def test_saturation_freezes_then_unwinds(self) -> None:
        controller = PIDController(PIDGains(kp=0.0, ki=5.0, output_limit=0.1, integrator_limit=1.0))
        controller.integrator = 0.2
        duty = controller.step(0.05, 0.0, 0.1)
        self.assertAlmostEqual(duty, 0.1, places=6)
        self.assertAlmostEqual(controller.integrator, 0.2, places=6)
        duty = controller.step(0.0, 0.05, 0.1)
        self.assertAlmostEqual(duty, 0.1, places=6)
        self.assertAlmostEqual(controller.integrator, 0.175, places=5)

    def test_negative_saturation_does_not_wind_up(self) -> None:
        controller = PIDController(PIDGains(kp=2.0, ki=50.0, output_limit=0.2))
        duty = controller.step(0.0, 1.0, 0.01)
        self.assertAlmostEqual(duty, -0.2, places=6)
        self.assertAlmostEqual(controller.integrator, 0.0, places=6)
        controller.step(0.0, 1.0, 0.01)
        self.assertAlmostEqual(controller.integrator, 0.0, places=6)

    def test_derivative_opposes_a_measurement_jump(self) -> None:
        controller = PIDController(PIDGains(kp=0.0, ki=0.0, kd=0.01, output_limit=1.0))
        self.assertAlmostEqual(controller.step(0.0, 0.0, 0.1), 0.0, places=6)
        self.assertAlmostEqual(controller.step(0.0, 1.0, 0.1), -0.1, places=5)

    def test_derivative_filter_reduces_the_kick(self) -> None:
        controller = PIDController(
            PIDGains(kp=0.0, ki=0.0, kd=0.01, output_limit=1.0, derivative_filter_hz=10.0)
        )
        controller.step(0.0, 0.0, 0.1)
        duty = controller.step(0.0, 1.0, 0.1)
        self.assertLess(duty, 0.0)
        self.assertGreater(duty, -0.1)

    def test_rejected_step_holds_integrator(self) -> None:
        controller = PIDController()
        controller.step(1.0, 0.0, 0.01)
        held = controller.integrator
        self.assertEqual(controller.step(math.nan, 0.0, 0.01), 0.0)
        self.assertEqual(controller.step(1.0, 0.0, 0.0), 0.0)
        self.assertEqual(controller.integrator, held)
        controller.gains = PIDGains(output_limit=0.0)
        self.assertEqual(controller.step(1.0, 0.0, 0.01), 0.0)
        self.assertEqual(controller.integrator, held)
        controller.reset()
        self.assertEqual(controller.integrator, 0.0)
        self.assertFalse(controller.initialized)


if __name__ == "__main__":
    unittest.main()
