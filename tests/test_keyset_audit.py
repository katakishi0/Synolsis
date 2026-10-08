import unittest
from tools.keyset_audit import audit_text


class KeysetAuditTests(unittest.TestCase):
    def test_latest_keys_without_material_in_results(self):
        value = "a" * 32
        result = audit_text(
            "master_key_16 = " + value + "\n" +
            "titlekek_16 = " + value + "\n" +
            "key_area_key_application_16 = " + value
        )
        self.assertTrue(result["latest_revision_fields_present"]["master_key"])
        self.assertTrue(result["latest_revision_fields_present"]["titlekek"])
        self.assertFalse(result["errors"])
        self.assertNotIn(value, repr(result))

    def test_preexisting_generations_and_comments(self):
        result = audit_text("# header\r\n titlekek_0f = " + "0" * 32 + " ; note")
        self.assertEqual(result["recognized_generations"]["titlekek"], ["0f"])
        self.assertFalse(result["errors"])

    def test_invalid_and_duplicate_lines(self):
        result = audit_text("titlekek_16=bad\ntitlekek_16=" + "a" * 32)
        self.assertEqual(len(result["errors"]), 2)

    def test_no_newline_is_ok(self):
        result = audit_text("header_key=" + "1" * 64)
        self.assertEqual(result["errors"], [])

    def test_unknown_future_fields_are_not_blocked(self):
        result = audit_text("future_key_ab=" + "c" * 64)
        self.assertFalse(result["errors"])


if __name__ == "__main__":
    unittest.main()
