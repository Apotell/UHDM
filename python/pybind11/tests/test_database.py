import os
import sys
import pytest

# Ensure we can import pyuhdm
# (Assumes PYTHONPATH is set or module is installed/in build dir)
try:
    import pyuhdm
except ImportError:
    # Fallback for local testing if not in path
    # This might need adjustment depending on how tests are invoked
    sys.path.append(os.path.join(os.path.dirname(__file__), '../../build_pybind11/lib'))
    import pyuhdm


def test_module_import():
    """Test 1: Module import verification."""
    assert pyuhdm is not None
    assert hasattr(pyuhdm, "Database")


def test_database_construction():
    """Test 2: Database construction."""
    db = pyuhdm.Database()
    assert db is not None
    # We cannot check is_loaded() as it was removed in Group 3


def test_load_invalid_file():
    """Test 3: Load invalid file (expect exception)."""
    db = pyuhdm.Database()
    # Expect RuntimeError as bind_db.cpp now maps general exceptions to RuntimeError
    # (previously FileNotFoundError, but that logic was removed from wrapper)
    with pytest.raises(RuntimeError):
        db.load("nonexistent_file_for_testing.uhdm")


def test_load_save_happy_path(tmp_path):
    """Test 4: Load + save happy path."""
    # We need a valid .uhdm file.
    # Since none exist in the repo by default, we look for a known location
    # or skip if not found.
    
    # Placeholder path - user should update this or provide a file
    input_file = os.getenv("UHDM_TEST_FILE", "surelog.uhdm")
    
    if not os.path.exists(input_file):
        pytest.skip(f"Test file not found: {input_file}. Set UHDM_TEST_FILE env var.")

    output_file = tmp_path / "output.uhdm"
    
    db = pyuhdm.Database()
    db.load(input_file)
    db.save(str(output_file))
    
    assert output_file.exists()
    assert output_file.stat().st_size > 0
