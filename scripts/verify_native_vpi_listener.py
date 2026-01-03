
import sys
import os

# Ensure we can import pyuhdm
sys.path.append(os.path.join(os.path.dirname(__file__), '../build/lib'))

try:
    import pyuhdm
except ImportError as e:
    print(f"FAIL: Could not import pyuhdm: {e}")
    sys.exit(1)

def verify_vpi_listener():
    print("Verifying VpiListener API...")

    if not hasattr(pyuhdm, "VpiListener"):
        print("FAIL: pyuhdm.VpiListener does not exist")
        return False
    print("PASS: pyuhdm.VpiListener exists")

    if not hasattr(pyuhdm, "walk_vpi"):
        print("FAIL: pyuhdm.walk_vpi does not exist")
        return False
    print("PASS: pyuhdm.walk_vpi exists")

    # Test Subclassing
    class MyVpiListener(pyuhdm.VpiListener):
        def __init__(self):
            super().__init__()
            self.visited_design = False
            self.visited_module = False

        def on_object(self, obj):
            # Not printing to avoid spam
            pass
            
        def on_design(self, obj):
            print("CALLBACK: on_design")
            self.visited_design = True
            
        def on_module(self, obj):
            print(f"CALLBACK: on_module (handle={obj})")
            self.visited_module = True

    listener = MyVpiListener()
    design = None
    
    # Try to load a design
    uhdm_file = "surelog.uhdm"
    if len(sys.argv) > 1:
        uhdm_file = sys.argv[1]

    if os.path.exists(uhdm_file):
        print(f"Loading {uhdm_file}...")
        s = pyuhdm.Serializer()
        res = s.restore(uhdm_file)
        if res and len(res) > 0:
            design = res[0]
    
    # Note: Cannot instantiate synthetic Design easily in Python as it requires Serializer context usually 
    # and we don't have full factory exposure in this PoC binding set.
    # But if we have no design, we just check infrastructure pass.

    if design:
        print("\nRunning VPI traversal...")
        # We need to cast design to vpiHandle? 
        # In C++, walk_vpi takes vpiHandle. 
        # pybind11 should handle Design* -> vpiHandle (void*) conversion?
        # Uhdm classes inherit from BaseClass. vpiHandle is typically void* or handle wrapper.
        # If pybind11 treats vpiHandle as an opaque pointer or int, we might need explicit cast.
        # But let's try passing the object.
        try:
            pyuhdm.walk_vpi(design, listener)
            
            if listener.visited_design:
                print("PASS: Visited Design")
            else:
                print("WARN: Traversal ran but on_design not called")
                
            if listener.visited_module:
                print("PASS: Visited Module")
                
        except TypeError as e:
            print(f"FAIL: walk_vpi raised TypeError: {e}")
            print("NOTE: This might be due to vpiHandle type mismatch in bindings.")
            return False
    else:
        print("NOTE: No .uhdm file found, skipping runtime traversal")

    return True

if __name__ == "__main__":
    if verify_vpi_listener():
        print("\nVerification SUCCESS")
        sys.exit(0)
    else:
        print("\nVerification FAILED")
        sys.exit(1)
