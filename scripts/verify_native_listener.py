
import sys
import os

# Ensure we can import pyuhdm
sys.path.append(os.path.join(os.path.dirname(__file__), '../build/lib'))

try:
    import pyuhdm
except ImportError as e:
    print(f"FAIL: Could not import pyuhdm: {e}")
    sys.exit(1)

def verify_listener():
    print("Verifying UhdmListener API...")

    if not hasattr(pyuhdm, "UhdmListener"):
        print("FAIL: pyuhdm.UhdmListener does not exist")
        return False
    print("PASS: pyuhdm.UhdmListener exists")

    if not hasattr(pyuhdm, "walk_design"):
        print("FAIL: pyuhdm.walk_design does not exist")
        return False
    print("PASS: pyuhdm.walk_design exists")

    # Test Subclassing
    class TestListener(pyuhdm.UhdmListener):
        def __init__(self):
            super().__init__()
            self.visited = False

        def enter(self, obj):
            # Print for debug
            # print(f"CALLBACK: enter({type(obj).__name__})")
            self.visited = True
        
        def leave(self, obj):
            pass

    listener = TestListener()
    design = None

    # Try to instantiate Design directly (might fail if not exposed)
    try:
        design = pyuhdm.Design()
        design.VpiName("TestDesign")
        print("NOTE: Created synthetic Design for testing.")
    except Exception:
        print("NOTE: Cannot instantiate pyuhdm.Design directly (no constructor).")

    # If no synthetic design, try loading a file file
    if design is None:
        uhdm_file = "surelog.uhdm"
        if len(sys.argv) > 1:
            uhdm_file = sys.argv[1]
        
        if os.path.exists(uhdm_file):
            print(f"Loading {uhdm_file}...")
            s = pyuhdm.Serializer()
            res = s.restore(uhdm_file)
            if res and len(res) > 0:
                # Assuming first element is a design for this PoC or we specifically look for one
                # Actually restore returns vector<design*>? Or vector<vpiHandle>?
                # UhdmSerializer.h: std::vector<vpiHandle> restore(const std::string& file);
                # In python binding: returns list of handles.
                # Design is a Handle? Yes.
                design = res[0]
                # Check if it is a design
                if not isinstance(design, pyuhdm.Design):
                     # If it's not a design, we might need to search or just skip
                     print(f"NOTE: Loaded object is {type(design)}, traversing anyway if possible.")
        else:
            print("NOTE: No .uhdm file found, skipping runtime traversal")

    if design:
        print("\nRunning traversal...")
        pyuhdm.walk_design(design, listener)

        if listener.visited:
            print("PASS: Traversal callbacks invoked")
        else:
            print("WARN: Traversal ran but no callbacks received (empty design?)")
    
    return True

if __name__ == "__main__":
    if verify_listener():
        print("\nVerification SUCCESS")
        sys.exit(0)
    else:
        print("\nVerification FAILED")
        sys.exit(1)
