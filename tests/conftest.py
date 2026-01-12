import pytest
import os
import platform
import shutil
import subprocess
import tempfile

from pathlib import Path

@pytest.fixture(scope="session")
def uhdm_file():
    """
    Generates a .uhdm file by running the C++ uhdm-gentest binary.
    Returns the path to the generated file.
    """
    # Locate the compiled uhdm-gentest binary
    this_filepath = Path(__file__).resolve()
    base_dirpath = this_filepath.parent.parent
    binary_paths = list(base_dirpath.rglob('uhdm-gentest.exe' if platform.system() == 'Windows' else 'uhdm-gentest'))

    if not binary_paths:
        pytest.skip(f"Test binary not found at {base_dirpath}. Please build utils.")

    # Create a temporary directory for the file
    tmp_dirpath = tempfile.mkdtemp(prefix="pyuhdm_test_")

    binary_path = binary_paths[0]
    env = os.environ.copy()
    env["TMPDIR"] = tmp_dirpath

    try:
        subprocess.check_call(
            [str(binary_path), f"{tmp_dirpath}/gentest.uhdm"],
            cwd=str(base_dirpath),
            env=env,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL
        )
    except subprocess.CalledProcessError as e:
        shutil.rmtree(tmp_dirpath)
        pytest.fail(f"Failed to run classes_test: {e}")

    # Expected file path in the temp dir
    # Note: GoogleTest's TempDir() on Linux usually falls back to /tmp if TMPDIR is not set or valid?
    # Actually, gtest honors TMPDIR.
    expected_file = os.path.join(tmp_dirpath, "gentest.uhdm")
    
    if not os.path.exists(expected_file):
        pytest.fail(f"Generated .uhdm file not found at {expected_file}")

    os.environ["PYUHDM_TEST_INPUT_FILE"] = expected_file
    yield expected_file
    
    # Cleanup
    if os.path.exists(tmp_dirpath):
        shutil.rmtree(tmp_dirpath)
