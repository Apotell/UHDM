import sys
import os

# Ensure the build directory is in sys.path
build_dir_lib = os.path.join(os.getcwd(), 'build', 'lib')
build_dir_py = os.path.join(os.getcwd(), 'build', 'python', 'pybind11')
if os.path.exists(build_dir_lib):
    sys.path.append(build_dir_lib)
elif os.path.exists(build_dir_py):
    sys.path.append(build_dir_py)

try:
    import pyuhdm
except ImportError as e:
    print(f"Failed to import pyuhdm: {e}")
    sys.exit(1)

# Usage:
# class DebugVisitor(pyuhdm.UhdmVisitor):
# ...

class DebugVisitor(pyuhdm.UhdmVisitor):
    def visit_Design(self, obj):
        print(f"Design: name={obj.name}")

    def visit_Module(self, obj):
        print(f"Module: name={obj.top_module if hasattr(obj, 'top_module') else 'Unknown'}")

    def visit_Port(self, obj):
        print(f"Port: name={obj.name}")

def main():
    if len(sys.argv) > 1:
        uhdm_file = sys.argv[1]
    else:
        # Check for sample file
        uhdm_file = "surelog.uhdm"

    if not os.path.exists(uhdm_file):
        print(f"Note: {uhdm_file} not found. Skipping runtime traversal.")
        # Verify class existence
        if hasattr(pyuhdm, 'UhdmVisitor'):
            print("PASS: pyuhdm.UhdmVisitor exists")
        else:
            print("FAIL: pyuhdm.UhdmVisitor missing")
            sys.exit(1)
            
        if hasattr(pyuhdm, 'traverse_design'):
            print("PASS: pyuhdm.traverse_design exists")
        else:
            print("FAIL: pyuhdm.traverse_design missing")
            sys.exit(1)
        return

    print(f"Loading {uhdm_file}...")
    s = pyuhdm.Serializer()
    roots = s.restore(uhdm_file)

    print("Traversing...")
    visitor = DebugVisitor()
    for root in roots:
        # traverse_design expects Design*
        # pybind11 casting handles inheritance usually.
        # But 'root' is UhdmDesign or valid pointer?
        # Serializer::restore returns vector of design*
        pyuhdm.traverse_design(root, visitor)
        
    print("Done.")

if __name__ == "__main__":
    main()
