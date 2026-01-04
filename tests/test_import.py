import pytest

def test_import_pyuhdm():
    """Verify that the pyuhdm module can be imported successfully."""
    import pyuhdm
    # Verify checking a known (even if trivial) attribute to ensure it's not a dummy mock
    assert pyuhdm.__name__ == "pyuhdm"
