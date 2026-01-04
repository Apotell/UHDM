import pytest
import pyuhdm

class RecordingListener(pyuhdm.UhdmListener):
    def __init__(self):
        super().__init__()
        self.events = []

    def enter(self, obj):
        if isinstance(obj, pyuhdm.Design):
            self.events.append(("enter", "Design", obj.vpiName))
        elif isinstance(obj, pyuhdm.Module):
            # Use defName because instance name might be empty for top modules depending on how they are created
            name = obj.vpiDefName
            self.events.append(("enter", "Module", name))
        elif isinstance(obj, pyuhdm.Port):
            self.events.append(("enter", "Port", obj.vpiName))

    def leave(self, obj):
        if isinstance(obj, pyuhdm.Design):
            self.events.append(("leave", "Design", obj.vpiName))
        elif isinstance(obj, pyuhdm.Module):
            name = obj.vpiDefName
            self.events.append(("leave", "Module", name))
        elif isinstance(obj, pyuhdm.Port):
            self.events.append(("leave", "Port", obj.vpiName))

def test_listener_subclass(uhdm_file):
    """Verify that Python subclass of UhdmListener receives enter/leave callbacks."""
    s = pyuhdm.Serializer()
    res = s.restore(uhdm_file)
    design = res[0]
    
    listener = RecordingListener()
    
    # walk_design is the C++ function that traverses the design and calls enter/leave
    pyuhdm.walk_design(design, listener)
    
    # Basic verification of event existence
    assert len(listener.events) > 0
    
    # Verify nesting structure: Design enter -> ... -> Design leave
    assert listener.events[0][0] == "enter"
    assert listener.events[0][1] == "Design"
    
    assert listener.events[-1][0] == "leave"
    assert listener.events[-1][1] == "Design"
    
    # Verify Modules found (M1, M2 from classes_test.cpp)
    module_defs = [e[2] for e in listener.events if e[1] == "Module" and e[0] == "enter"]
    assert "M1" in module_defs
    assert "M2" in module_defs
    
    # Verify Port callbacks
    ports = [e[2] for e in listener.events if e[1] == "Port" and e[0] == "enter"]
    assert "i1" in ports
    assert "o1" in ports

def test_listener_ordering(uhdm_file):
    """Verify valid stack behavior (enter/leave balance)."""
    s = pyuhdm.Serializer()
    res = s.restore(uhdm_file)
    design = res[0]
    
    listener = RecordingListener()
    pyuhdm.walk_design(design, listener)
    
    stack = []
    for action, type_, name in listener.events:
        if action == "enter":
            stack.append((type_, name))
        elif action == "leave":
            assert len(stack) > 0, f"Unbalanced leave for {type_} {name}"
            top_type, top_name = stack.pop()
            assert top_type == type_, f"Type mismatch: entered {top_type}, left {type_}"
            # Name might not strictly match if we used different properties, but here we used same property
            assert top_name == name, f"Name mismatch: entered {top_name}, left {name}"
            
    assert len(stack) == 0, "Stack not empty at end of traversal"
