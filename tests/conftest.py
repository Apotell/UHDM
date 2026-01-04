import pytest
import os
import subprocess
import shutil
import tempfile

@pytest.fixture(scope="session")
def uhdm_file():
    """
    Generates a .uhdm file by running the C++ classes_test binary.
    Returns the path to the generated file.
    """
    # Locate the compiled classes_test binary
    # Assumes we are running from project root
    base_dir = os.getcwd()
    binary_path = os.path.join(base_dir, "build", "bin", "classes_test")
    
    if not os.path.exists(binary_path):
        pytest.skip(f"Test binary not found at {binary_path}. Please build tests.")

    # Create a temporary directory for the file
    tmp_dir = tempfile.mkdtemp(prefix="pyuhdm_test_")
    
    # The C++ test writes to testing::TempDir(), which is usually /tmp/
    # We can try to force it or just look for the known filename `classes_test.uhdm`
    # The test code says: const std::string filename = testing::TempDir() + "/classes_test.uhdm";
    
    # We will try to override TMPDIR to capture the output, but gtest might use other logic.
    # Alternatively, we just look in /tmp/classes_test.uhdm as verified by the user/agent earlier,
    # BUT the user showed permission errors in /tmp related to systemd private dirs.
    # The safest way is to clear the potential target or set TMPDIR if gtest honors it.
    
    env = os.environ.copy()
    env["TMPDIR"] = tmp_dir
    # Also set LD_LIBRARY_PATH if needed for shared libs
    env["LD_LIBRARY_PATH"] = os.path.join(base_dir, "build", "lib") + ":" + env.get("LD_LIBRARY_PATH", "")

    try:
        subprocess.check_call(
            [binary_path, "--gtest_filter=ClassesTest.DesignSaveRestoreRoundtrip"],
            cwd=base_dir,
            env=env,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL
        )
    except subprocess.CalledProcessError as e:
        shutil.rmtree(tmp_dir)
        pytest.fail(f"Failed to run classes_test: {e}")

    # Expected file path in the temp dir
    # Note: GoogleTest's TempDir() on Linux usually falls back to /tmp if TMPDIR is not set or valid?
    # Actually, gtest honors TMPDIR.
    expected_file = os.path.join(tmp_dir, "classes_test.uhdm")
    
    if not os.path.exists(expected_file):
        # Fallback check in /tmp if TMPDIR wasn't respected
        fallback = "/tmp/classes_test.uhdm"
        if os.path.exists(fallback):
             # Copy it to our temp dir to own it
             shutil.copy(fallback, expected_file)
        else:
            shutil.rmtree(tmp_dir)
            pytest.fail(f"Generated .uhdm file not found at {expected_file} or {fallback}")

    yield expected_file
    
    # Cleanup
    if os.path.exists(tmp_dir):
        shutil.rmtree(tmp_dir)
