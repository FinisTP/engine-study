"""Python view of the engine's training environment (UmbraPolicyEnv through ctypes).

Developer-only. The fights run in the engine's own C++ turn rules; Python only chooses actions.
Library path: --library, or the UMBRA_POLICY_ENV environment variable.
"""
import ctypes
import os

import numpy as np


def load_library(path=None):
    path = path or os.environ.get("UMBRA_POLICY_ENV")
    if not path:
        raise RuntimeError("pass --library <path to UmbraPolicyEnv.dll/.so> or set UMBRA_POLICY_ENV")
    lib = ctypes.CDLL(path)
    f, i, u, c, p = ctypes.c_float, ctypes.c_int, ctypes.c_uint, ctypes.c_char_p, ctypes.c_void_p
    fp, ip, bp = ctypes.POINTER(f), ctypes.POINTER(i), ctypes.POINTER(ctypes.c_ubyte)
    signatures = {
        "umbra_observation_version": ([], c), "umbra_observation_size": ([], i),
        "umbra_env_create": ([c, c, c, f, i, i, i], p), "umbra_env_destroy": ([p], None), "umbra_env_reset": ([p, u], i),
        "umbra_env_action_count": ([p], i), "umbra_env_observe": ([p, fp], None), "umbra_env_mask": ([p, bp], None),
        "umbra_env_step": ([p, i, fp, ip, ip, ip], i), "umbra_env_conventional": ([p], i), "umbra_env_illegal": ([p], i),
        "umbra_env_is_attack": ([p, i], i), "umbra_env_action_name": ([p, i], c), "umbra_env_fingerprint": ([p], c),
        "umbra_model_load": ([c, p], p), "umbra_model_destroy": ([p], None), "umbra_model_logits": ([p, fp, fp], i),
        "umbra_model_choose": ([p, fp, bp, i], i),
    }
    for name, (args, result) in signatures.items():
        fn = getattr(lib, name)
        fn.argtypes, fn.restype = args, result
    return lib


class FightEnv:
    """One learner-vs-opponent turn fight. Observations and masks come straight from the engine."""

    def __init__(self, lib, content, learner="bandit_leader", opponent="hero", randomness=0.15, random_start=True, max_rounds=30, opponent_level=1):
        self.lib = lib
        self.handle = lib.umbra_env_create(content.encode(), learner.encode(), opponent.encode(), randomness, int(random_start), max_rounds, opponent_level)
        if not self.handle:
            raise RuntimeError("could not create the environment (see stderr)")
        self.obs_size = lib.umbra_observation_size()
        self.actions = lib.umbra_env_action_count(self.handle)
        self.names = [lib.umbra_env_action_name(self.handle, a).decode() for a in range(self.actions)]
        self.attack = np.array([lib.umbra_env_is_attack(self.handle, a) for a in range(self.actions)], dtype=bool)
        self.fingerprint = lib.umbra_env_fingerprint(self.handle).decode()
        self._obs = (ctypes.c_float * self.obs_size)()
        self._mask = (ctypes.c_ubyte * self.actions)()
        self._r, self._d, self._t, self._w = ctypes.c_float(), ctypes.c_int(), ctypes.c_int(), ctypes.c_int()

    def reset(self, seed):
        if not self.lib.umbra_env_reset(self.handle, seed):
            raise RuntimeError("reset failed")
        return self.observe()

    def observe(self):
        self.lib.umbra_env_observe(self.handle, self._obs)
        self.lib.umbra_env_mask(self.handle, self._mask)
        return np.frombuffer(self._obs, dtype=np.float32).copy(), np.frombuffer(self._mask, dtype=np.uint8).copy()

    def step(self, action):
        self.lib.umbra_env_step(self.handle, int(action), ctypes.byref(self._r), ctypes.byref(self._d), ctypes.byref(self._t), ctypes.byref(self._w))
        return self._r.value, bool(self._d.value), bool(self._t.value), self._w.value

    def conventional(self):
        return self.lib.umbra_env_conventional(self.handle)

    def illegal(self):
        return self.lib.umbra_env_illegal(self.handle)

    def close(self):
        if self.handle:
            self.lib.umbra_env_destroy(self.handle)
            self.handle = None
