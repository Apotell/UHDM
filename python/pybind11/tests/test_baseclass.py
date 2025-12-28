import pytest
import pyuhdm

def test_baseclass_exists():
    assert hasattr(pyuhdm, "BaseClass")

def test_baseclass_methods():
    # Since we can't easily create a BaseClass directly (it's abstract),
    # and we don't have subclasses bound yet or a way to get one easily without a real design,
    # we inspect the class object for method presence.
    assert hasattr(pyuhdm.BaseClass, "getUhdmId")
    assert hasattr(pyuhdm.BaseClass, "getFile")
    assert hasattr(pyuhdm.BaseClass, "getStartLine")
    assert hasattr(pyuhdm.BaseClass, "getStartColumn")
    assert hasattr(pyuhdm.BaseClass, "getEndLine")
    assert hasattr(pyuhdm.BaseClass, "getEndColumn")
    assert hasattr(pyuhdm.BaseClass, "getVpiType")
    assert hasattr(pyuhdm.BaseClass, "getUhdmType")
