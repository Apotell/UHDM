import pytest
import pyuhdm

class MyVisitor(pyuhdm.UhdmVisitor):
    def __init__(self):
        super().__init__()
        self.visited_design = False
        self.visited_modules = []
        self.visited_ports = []
        self.visited_others = 0

    def visit(self, obj):
        if isinstance(obj, pyuhdm.Design):
            self.visited_design = True
        elif isinstance(obj, pyuhdm.Module):
            # Capture module name for verification
            name = obj.vpiName or obj.vpiDefName
            self.visited_modules.append(name)
        elif isinstance(obj, pyuhdm.Port):
            self.visited_ports.append(obj.vpiName)
        else:
            self.visited_others += 1

def test_visitor_subclass(uhdm_file):
    """Verify that Python subclass of UhdmVisitor receives callbacks."""
    s = pyuhdm.Serializer()
    res = s.restore(uhdm_file)
    design = res[0]
    
    visitor = MyVisitor()
    
    # traverse_design is the C++ function that walks the tree and calls visitor.visit()
    pyuhdm.traverse_design(design, visitor)
    
    assert visitor.visited_design, "Did not visit Design"
    assert len(visitor.visited_modules) >= 2, "Expected at least 2 modules"
    
    # Check that we visited generic objects if any (BaseClass fallback or other overloads)
    # The current binding has specific overloads for Design, Module, Port.
    # traverse_design implementation in C++ (native_visitor_poc.cpp) determines what is visited.
    
    # Check for M1 and M2 in visited modules
    # M1 and M2 are defNames in classes_test.cpp
    assert any("M1" in m for m in visitor.visited_modules if m)
    assert any("M2" in m for m in visitor.visited_modules if m)
    
    # Check ports - M2 has ports i1, o1
    assert "i1" in visitor.visited_ports
    assert "o1" in visitor.visited_ports

def test_manual_visit(uhdm_file):
    """Verify manual invocation of visit methods from Python."""
    s = pyuhdm.Serializer()
    res = s.restore(uhdm_file)
    design = res[0]
    
    visitor = MyVisitor()
    visitor.visit(design)
    assert visitor.visited_design
    
    # Visit a module manually
    if design.all_modules:
        m = design.all_modules[0]
        # Reset
        visitor.visited_modules = []
        visitor.visit(m)
        assert len(visitor.visited_modules) == 1
