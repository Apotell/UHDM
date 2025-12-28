import unittest
import pyuhdm

class TestModuleInst(unittest.TestCase):
    def test_module_inst_api_presence(self):
        # Verify class exists
        self.assertTrue(hasattr(pyuhdm, "module_inst"))
        
        # Verify inheritance
        self.assertTrue(issubclass(pyuhdm.module_inst, pyuhdm.BaseClass))
        
        # Verify method presence
        self.assertTrue(hasattr(pyuhdm.module_inst, "getDefName"))
        self.assertTrue(hasattr(pyuhdm.module_inst, "getPorts"))

if __name__ == '__main__':
    unittest.main()
