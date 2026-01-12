import pytest
import pyuhdm

class Listener(pyuhdm.UhdmListener):
    def __init__(self):
        super().__init__()
        self.events = []

    def enterDesign(self, obj, relation):
        self.events.append(("enter", obj.uhdm_type.name, obj.name))

    def leaveDesign(self, obj, relation):
        self.events.append(("leave", obj.uhdm_type.name, obj.name))

    def enterModule(self, obj, relation):
        self.events.append(("enter", obj.uhdm_type.name, obj.name))

    def leaveModule(self, obj, relation):
        self.events.append(("leave", obj.uhdm_type.name, obj.name))

    def enterPort(self, obj, relation):
        self.events.append(("enter", obj.uhdm_type.name, obj.name))

    def leavePort(self, obj, relation):
        self.events.append(("leave", obj.uhdm_type.name, obj.name))

    def enterModuleCollection(self, obj, modules, relation):
        self.events.append(("enter", obj.uhdm_type.name, {m.name for m in modules}))

    def leaveModuleCollection(self, obj, modules, relation):
        self.events.append(("leave", obj.uhdm_type.name, {m.name for m in modules}))

def test_UhdmListener(uhdm_file):
    """Verify that Python subclass of UhdmListener receives enter/leave callbacks."""
    s = pyuhdm.Serializer()
    designs = s.restore(uhdm_file)

    listener = Listener()
    for design in designs:
        listener.listenAny(design, 0)
    
    # Basic verification of event existence
    assert len(listener.events) > 0
    
    expected = [
      ("enter", "Design", "unnamed"),
      ("enter", "Design", {"work@top"}),
      ("enter", "Module", "work@top"),
      ("enter", "Port", "a"),
      ("leave", "Port", "a"),
      ("enter", "Port", "a"),
      ("leave", "Port", "a"),
      ("leave", "Module", "work@top"),
      ("leave", "Design", {"work@top"}),
      ("leave", "Design", "unnamed"),
    ]

    assert listener.events == expected
