import os
import sys
import unittest
import tempfile

# Ensure we can import pyuhdm
try:
    import pyuhdm
except ImportError:
    # Fallback for local testing if not in path
    # Path from tests/ to build_clean/lib: up 3 levels
    # tests -> pybind11 -> python -> root -> build_clean/lib
    sys.path.append(os.path.join(os.path.dirname(__file__), '../../../build_clean/lib'))
    try:
        import pyuhdm
    except ImportError as e:
        print(f"Error: Could not import pyuhdm: {e}")
        sys.exit(1)

class TestSerializer(unittest.TestCase):

    def test_module_import(self):
        """Verify module import and class existence."""
        self.assertTrue(pyuhdm is not None)
        self.assertTrue(hasattr(pyuhdm, "Serializer"))

    def test_serializer_construction(self):
        """Verify Serializer construction."""
        s = pyuhdm.Serializer()
        self.assertIsNotNone(s)

    def test_restore_invalid_file(self):
        """Verify restore raises generic RuntimeError for invalid files."""
        s = pyuhdm.Serializer()
        # Expect RuntimeError as the generic exception type for binding errors
        with self.assertRaises(RuntimeError):
            s.restore("nonexistent_file_for_testing.uhdm")

    def test_restore_save_happy_path(self):
        """Verify restore and save logic when a test file is provided."""
        input_file = os.getenv("UHDM_TEST_FILE", "surelog.uhdm")
        
        if not os.path.exists(input_file):
            self.skipTest(f"Test file not found: {input_file}. Set UHDM_TEST_FILE env var.")

        with tempfile.NamedTemporaryFile(suffix=".uhdm", delete=False) as tmp:
            output_file = tmp.name
        
        try:
            s = pyuhdm.Serializer()
            handles = s.restore(input_file)
            self.assertIsInstance(handles, list)
            
            s.save(output_file)
            
            self.assertTrue(os.path.exists(output_file))
            self.assertGreater(os.stat(output_file).st_size, 0)
        finally:
            if os.path.exists(output_file):
                os.remove(output_file)

if __name__ == '__main__':
    unittest.main()
