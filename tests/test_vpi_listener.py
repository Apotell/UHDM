import pytest
import pyuhdm

class RecordingVpiListener(pyuhdm.VpiListener):
    def __init__(self):
        super().__init__()
        self.visited_design = False
        self.visited_modules = 0
        self.last_handle = None

    def on_design(self, obj):
        # obj is a vpiHandle (which might be wrapped as int or object depending on binding)
        self.visited_design = True
        self.last_handle = obj

    def on_module(self, obj):
        self.visited_modules += 1
        self.last_handle = obj

def test_vpi_listener_subclass(uhdm_file):
    """Verify that Python subclass of VpiListener receives callbacks."""
    s = pyuhdm.Serializer()
    res = s.restore(uhdm_file)
    design = res[0]
    
    listener = RecordingVpiListener()
    
    # walk_vpi is a PoC function that traverses Design -> TopModules and calls callbacks
    pyuhdm.walk_vpi(design, listener)
    
    assert listener.visited_design, "Did not receive on_design callback"
    assert listener.visited_modules > 0, "Did not receive on_module callback"
    
    # Verify that we got some handle
    assert listener.last_handle is not None
    
    # In C++, vpiHandle is a pointer. In pybind11, unless we have a specific wrapper,
    # it might come as a generic object or internal capsule. 
    # We just need to ensure it's not None/Null.
