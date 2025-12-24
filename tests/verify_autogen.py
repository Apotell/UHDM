
import sys
import os

# Ensure the build directory is in sys.path
build_dir_lib = os.path.join(os.getcwd(), 'build', 'lib')
build_dir_py = os.path.join(os.getcwd(), 'build', 'python', 'pybind11')
if os.path.exists(build_dir_lib):
    sys.path.append(build_dir_lib)
elif os.path.exists(build_dir_py):
    sys.path.append(build_dir_py)
else:
    # Try finding pyuhdm shared object
    # This assumes we are running from root or scripts
    pass

try:
    import pyuhdm
    print(f"Successfully imported pyuhdm from {pyuhdm.__file__}")
except ImportError as e:
    print(f"Failed to import pyuhdm: {e}")
    sys.exit(1)

# Verification
failed = False

# 1. Check Design class presence
if hasattr(pyuhdm, "Design"):
    print("PASS: pyuhdm.Design exists")
else:
    print("FAIL: pyuhdm.Design missing")
    failed = True

# 2. Check Property presence
if hasattr(pyuhdm, "Design") and hasattr(pyuhdm.Design, "elaborated"):
    print("PASS: pyuhdm.Design.elaborated exists")
else:
    print("FAIL: pyuhdm.Design.elaborated missing")
    failed = True

if failed:
    sys.exit(1)
else:
    print("All checks passed.")
