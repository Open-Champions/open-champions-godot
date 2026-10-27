import unittest

from run_loop_playthrough import pickup_verified


class PickupAcceptance(unittest.TestCase):
    def setUp(self):
        self.row = {"step": "loot_pickup", "result": "passed", "drop_produced": True,
                    "picked_up": True, "walked": True,
                    "inventory_units_before": 10, "inventory_units_after": 11}

    def test_real_pickup(self):
        self.assertTrue(pickup_verified([self.row]))

    def test_no_pickup_row(self):
        self.assertFalse(pickup_verified([]))
        self.assertFalse(pickup_verified([{"step": "enemy_fight", "result": "passed"}]))

    def test_no_drop_is_not_a_pass(self):
        self.row.update(drop_produced=False, picked_up=False, walked=False,
                        inventory_units_after=10)
        self.assertFalse(pickup_verified([self.row]))

    def test_each_required_observation(self):
        for key in ("drop_produced", "picked_up", "walked"):
            with self.subTest(key=key):
                self.assertFalse(pickup_verified([{**self.row, key: False}]))
                incomplete = dict(self.row)
                del incomplete[key]
                self.assertFalse(pickup_verified([incomplete]))

    def test_inventory_must_increase(self):
        for quantity in (9, 10):
            with self.subTest(quantity=quantity):
                self.assertFalse(pickup_verified([{**self.row, "inventory_units_after": quantity}]))

    def test_refused_result_is_not_a_pass(self):
        self.assertFalse(pickup_verified([{**self.row, "result": "refused"}]))


if __name__ == "__main__":
    unittest.main()
