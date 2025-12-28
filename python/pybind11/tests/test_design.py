import unittest
import pyuhdm

class TestDesign(unittest.TestCase):
    def test_design_api_presence(self):
        # Verify class exists
        self.assertTrue(hasattr(pyuhdm, "Design"))
        
        # Verify inheritance
        self.assertTrue(issubclass(pyuhdm.Design, pyuhdm.BaseClass))
        
        # Verify method presence
        self.assertTrue(hasattr(pyuhdm.Design, "getName"))
        self.assertTrue(hasattr(pyuhdm.Design, "getTopModules"))

    def test_uhdm_type_enum(self):
        # Verify Enum binding
        self.assertTrue(hasattr(pyuhdm, "UhdmType"))
        self.assertTrue(hasattr(pyuhdm.UhdmType, "Design"))
        # We assume Design type ID is 2051 per uhdm_types.h (checked in previous view)
        # But we simply check if the value exists and is an integer-like enum
        self.assertIsInstance(pyuhdm.UhdmType.Design, pyuhdm.UhdmType)

if __name__ == '__main__':
    unittest.main()
