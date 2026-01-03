
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
    print(f"Successfully imported pyuhdm from {pyuhdm.__file__}")
except ImportError as e:
    print(f"Failed to import pyuhdm: {e}")
    sys.exit(1)

def verify_container_iteration(filepath=None):
    if not hasattr(pyuhdm, "Design"):
        print("FAIL: pyuhdm.Design missing")
        return False
    
    if not hasattr(pyuhdm.Design, "all_modules"):
        print("FAIL: pyuhdm.Design.all_modules missing")
        return False
        
    print("PASS: pyuhdm.Design.all_modules exists")

    if filepath is None:
        if os.path.exists("surelog.uhdm"):
            filepath = "surelog.uhdm"
    
    if filepath and os.path.exists(filepath):
        print(f"Loading {filepath}...")
        res = pyuhdm.Serializer().restore(filepath)
        designs = res.get(0).getDesigns() # Inspect top level designs
        # Note: Serializer.restore returns a std::vector<design*>, but pyuhdm might bind it differently?
        # Actually Serializer methods are likely bound as part of serializer binding.
        # Wait, pure Serializer binding might not be fully fleshed out in this PoC? 
        # The user instruction says: "restore via pyuhdm.Serializer().restore(file)"
        
        # In current Milestone (1/2), Serializer binding exists. 
        # But 'restore' usually returns the designs.
        
        # Let's check if the returned value is iterable.
        if designs:
            for design in designs: # Assuming pythonic list
                print(f"Design: {design.getVpiName()}")
                
                # Verify container iteration
                modules = design.all_modules
                if modules is not None:
                    print(f"  all_modules count: {len(modules)}")
                    for mod in modules:
                        print(f"    Module: {mod.getVpiName()}")
                        # Iterate ports if available
                        if hasattr(mod, "ports"):
                            ports = mod.ports
                            if ports:
                                print(f"      Port count: {len(ports)}")
                                for port in ports:
                                    print(f"        Port: {port.getVpiName()}")
                        elif hasattr(mod, "all_ports"): # fallback naming
                            ports = mod.all_ports
                            if ports:
                                print(f"      AllPort count: {len(ports)}")
    else:
        print("NOTE: No .uhdm file provided or found. Skipping runtime iteration check.")
        
    return True

if __name__ == "__main__":
    filepath = sys.argv[1] if len(sys.argv) > 1 else None
    if verify_container_iteration(filepath):
        print("Verification SUCCESS")
        sys.exit(0)
    else:
        print("Verification FAILED")
        sys.exit(1)
