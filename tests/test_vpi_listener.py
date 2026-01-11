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
    try:
        pyuhdm.walk_vpi(design, listener)
    except RuntimeError as e:
        if "No VPI runtime context" in str(e):
            pytest.skip("Skipping VPI test: No VPI runtime context available")
        else:
            raise e

    assert listener.visited_design, "Did not receive on_design callback"
    assert listener.visited_modules > 0, "Did not receive on_module callback"
    
    # Verify that we got some handle
    assert listener.last_handle is not None


def test_vpi_context_guard():
    """Verify that walk_vpi raises RuntimeError when no context is set."""
    listener = RecordingVpiListener()
    
    # Ensure context is clear before test
    if hasattr(pyuhdm, "clear_vpi_context"):
        pyuhdm.clear_vpi_context()

    # 1. No context -> RuntimeError
    with pytest.raises(RuntimeError, match="No VPI runtime context"):
        pyuhdm.walk_vpi(None, listener) # Passing None as design is safe with our guard
        
    # NOTE: Positive tests that inject a valid capsule are not included here.
    # They require a real VPI runtime (e.g. from a simulator) to provide a valid pointer.
    # Creating a fake capsule with a random pointer is unsafe and causes potential crashes/UB.
    
    # In C++, vpiHandle is a pointer. In pybind11, unless we have a specific wrapper,
    # it might come as a generic object or internal capsule. 
    # We just need to ensure it's not None/Null.
