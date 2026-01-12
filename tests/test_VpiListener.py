import pytest
import pyuhdm

class Listener(pyuhdm.VpiListener):
    def __init__(self):
        super().__init__()
        self.events = []

    def enterDesign(self, obj, handle):
        self.events.append(("enter", obj.uhdm_type.name, obj.name))

    def leaveDesign(self, obj, handle):
        self.events.append(("leave", obj.uhdm_type.name, obj.name))

    def enterModule(self, obj, handle):
        self.events.append(("enter", obj.uhdm_type.name, obj.name))

    def leaveModule(self, obj, handle):
        self.events.append(("leave", obj.uhdm_type.name, obj.name))

    def enterPort(self, obj, handle):
        self.events.append(("enter", obj.uhdm_type.name, obj.name))

    def leavePort(self, obj, handle):
        self.events.append(("leave", obj.uhdm_type.name, obj.name))


def test_VpiListener(uhdm_file):
    """Verify that Python subclass of VpiListener receives callbacks."""
    s = pyuhdm.Serializer()
    designs = s.restore(uhdm_file)

    listener = Listener()
    for design in designs:
        handle = design.vpi_handle
        print(f"handle = {handle}")
        listener.listenAny(design.vpi_handle)

    assert len(listener.events) > 0
    
    expected = [
      ("Design", "unnamed"),
      ("Module", "work@top"),
      ("Port", "a"),
      ("Port", "a"),
    ]

    assert listener.events == expected
