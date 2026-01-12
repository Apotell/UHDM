import os
import pytest
import pyuhdm

def test_import_pyuhdm():
    """Verify that the pyuhdm module can be imported successfully."""
    import pyuhdm
    # Verify checking a known (even if trivial) attribute to ensure it's not a dummy mock
    assert pyuhdm.__name__ == "pyuhdm"

def test_module_import():
    """Verify module import and class existence."""
    assert pyuhdm is not None
    assert hasattr(pyuhdm, "Serializer")

def test_serializer_construction():
    """Verify Serializer construction."""
    s = pyuhdm.Serializer()
    assert s is not None

# def test_restore_invalid_file():
#     """Verify restore raises generic RuntimeError for invalid files."""
#     s = pyuhdm.Serializer()
#     # Expect RuntimeError as the generic exception type for binding errors
#     with pytest.raises(Exception):
#         s.restore("nonexistent_file_for_testing.uhdm")

def test_restore_save_happy_path(uhdm_file):
    """Verify restore and save logic when a test file is provided."""
    if not os.path.exists(uhdm_file):
        pytest.skip(f"Test file not found: {uhdm_file}. Set PYUHDM_TEST_INPUT_FILE env var.")

    import tempfile
    with tempfile.NamedTemporaryFile(suffix=".uhdm", delete=False) as tmp:
        output_file = tmp.name
        
    try:
        s = pyuhdm.Serializer()
        handles = s.restore(uhdm_file)
        assert isinstance(handles, list)
            
        s.save(output_file)
            
        assert os.path.exists(output_file)
        assert os.stat(output_file).st_size > 0
    finally:
        if os.path.exists(output_file):
            os.remove(output_file)

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
    
    modules = design.all_modules
    assert len(modules) == 1

    names = set()
    for m in modules:
        assert isinstance(m, pyuhdm.Module)
        assert m.name
        names.add(m.name)

    assert names == {"work@top"}

def test_iterate_ports(uhdm_file):
    """Verify iteration of ports on a module."""
    s = pyuhdm.Serializer()
    res = s.restore(uhdm_file)
    design = res[0]
    
    top = None
    for m in design.all_modules:
        if m.name == "work@top":
            top = m
            break
            
    assert top is not None, "Could not find module top"
    
    ports = top.ports
    assert len(ports) == 1

    names = {p.name for p in ports}
    assert names == {"a"}

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
