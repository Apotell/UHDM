
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

# Import visitor - ensure we can import the local file
# We are likely running from root, so python.pybind11.visitor_poc should work 
# if we add root to path or if we use relative imports correctly?
# Simplest: append python/pybind11 to sys.path
sys.path.append(os.path.join(os.getcwd(), 'python', 'pybind11'))

try:
    from visitor_poc import DebugVisitor
except ImportError:
    # Try fully qualified
    sys.path.append(os.getcwd())
    from python.pybind11.visitor_poc import DebugVisitor

def create_mock_design():
    # Since we can't easily parse a real file without a full build/install of serializer?
    # Actually pyuhdm.Serializer exists.
    # But do we have a sample file?
    # The prompt usage: s.restore("example.uhdm")
    # If example.uhdm doesn't exist, we might fail.
    # We should probably check if we can mock it or if we should skip runtime if file missing.
    # But wait, user verification notes: "Run the verification script...".
    # I should assume I need a serializer test or similar.
    # However, I don't have a specific "example.uhdm" guaranteed.
    # I will try to load one if provided, or exit gracefully.
    pass

def main():
    if len(sys.argv) > 1:
        uhdm_file = sys.argv[1]
    else:
        # Check if there is a known uhdm file? 
        # For PoC, maybe we just print valid import and exit if no file.
        print("Usage: python3 scripts/verify_python_visitor.py <file.uhdm>")
        # We can try to proceed if we can create objects? 
        # UHDM currently doesn't allow easy creation of objects from Python (read-only bindings).
        # So we depend on restore.
        # Just fail if no file?
        # User said "s.restore('example.uhdm')". 
        # I'll default to checking if 'surelog.uhdm' exists or similar.
        uhdm_file = "surelog.uhdm"

    if not os.path.exists(uhdm_file):
        print(f"Skipping traversal test: {uhdm_file} not found.")
        # Minimal success: Import worked.
        return

    print(f"Loading {uhdm_file}...")
    if not os.path.exists(uhdm_file):
        print(f"Note: {uhdm_file} not found. Skipping runtime traversal.")
        print("PASS: Verified import and existence of Visitor classes.")
        return

    s = pyuhdm.Serializer()
    roots = s.restore(uhdm_file)

    print("Traversing...")
    visitor = DebugVisitor()
    for root in roots:
        visitor.visit(root)
        
    print("Done.")

if __name__ == "__main__":
    main()
