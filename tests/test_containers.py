import pytest
import pyuhdm

def test_design_restore(uhdm_file):
    """Verify that we can restore designs from a UHDM file."""
    s = pyuhdm.Serializer()
    designs = s.restore(uhdm_file)
    assert len(designs) > 0
    assert isinstance(designs[0], pyuhdm.Design)

def test_iterate_modules(uhdm_file):
    """Verify safe iteration over design.all_modules."""
    s = pyuhdm.Serializer()
    res = s.restore(uhdm_file)
    design = res[0]
    
    # The C++ test `classes_test.cpp` creates a design with 2 modules: M1 and M2
    modules = design.all_modules
    assert len(modules) >= 2 
    
    names = set()
    for m in modules:
        assert isinstance(m, pyuhdm.Module)
        name = m.vpiName
        if name:
            names.add(name)
            
    # Check for likely names from the C++ test
    # M1 is defName, M1 is top module instance name
    # We should at least see M1 and M2 in def names
    def_names = {m.vpiDefName for m in modules if m.vpiDefName}
    assert "M1" in def_names
    assert "M2" in def_names

def test_iterate_ports(uhdm_file):
    """Verify iteration of ports on a module."""
    s = pyuhdm.Serializer()
    res = s.restore(uhdm_file)
    design = res[0]
    
    # Find M2 which has ports in the C++ test
    m2 = None
    for m in design.all_modules:
        if m.vpiDefName == "M2":
            m2 = m
            break
            
    assert m2 is not None, "Could not find module M2"
    
    ports = m2.ports
    # M2 has 2 ports: i1, o1
    assert len(ports) == 2
    
    port_names = {p.vpiName for p in ports}
    assert "i1" in port_names
    assert "o1" in port_names

def test_nullptr_safety(uhdm_file):
    """Verify that iterating over empty collections does not crash."""
    s = pyuhdm.Serializer()
    res = s.restore(uhdm_file)
    design = res[0]
    
    # Arbitrary check: if we ask for something likely empty in this simple design,
    # e.g. all_udps (the test creates classes and modules but not UDPs)
    udps = design.all_udps
    
    # It might return an empty list or None depending on binding implementation
    # The binding uses a helper that returns std::vector, so it should be an empty list if underlying is null
    assert isinstance(udps, list)
    assert len(udps) == 0
    
    for u in udps:
        # Should not execute
        assert False, "Iterated over empty collection"
