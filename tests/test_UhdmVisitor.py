import pytest
import pyuhdm

class Visitor(pyuhdm.UhdmVisitor):
    def __init__(self):
        super().__init__()
        self.events = []

    def visitDesign(self, obj):
        self.events.append((obj.uhdm_type.name, obj.name))

    def visitModule(self, obj):
        self.events.append((obj.uhdm_type.name, obj.name))

    def visitPort(self, obj):
        self.events.append((obj.uhdm_type.name, obj.name))


def test_UhdmVisitor(uhdm_file):
    """Verify that Python subclass of UhdmListener receives enter/leave callbacks."""
    s = pyuhdm.Serializer()
    designs = s.restore(uhdm_file)

    visitor = Visitor()
    for design in designs:
        visitor.visit(design)
    
    assert len(visitor.events) > 0
    
    expected = [
      ("Design", "unnamed"),
      ("Module", "work@top"),
      ("Port", "a"),
      ("Port", "a"),
    ]

    assert visitor.events == expected
