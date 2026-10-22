"""Digital twin of the portable current loop, protection logic, and grid PLL."""

from simulation.twin.loop import LoopConfig, LoopResult, run_current_loop
from simulation.twin.pll import PllConfig, PllResult, run_pll
from simulation.twin.protection import FAULT_BITS, evaluate
from simulation.twin.tuning import suggest_current_gains

__all__ = [
    "FAULT_BITS",
    "LoopConfig",
    "LoopResult",
    "PllConfig",
    "PllResult",
    "evaluate",
    "run_current_loop",
    "run_pll",
    "suggest_current_gains",
]
