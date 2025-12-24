
class UhdmVisitor:
    def visit(self, obj):
        if obj is None:
            return
        method_name = "visit_" + obj.__class__.__name__
        method = getattr(self, method_name, self.generic_visit)
        return method(obj)

    def generic_visit(self, obj):
        pass

class DebugVisitor(UhdmVisitor):
    def visit_Design(self, obj):
        # Access properties using the bound names (snake_case)
        print(f"Design: name={obj.name}, elaborated={obj.elaborated}")

        # Traverse top_modules if available
        # Note: autogen bindings must expose 'top_modules'
        if hasattr(obj, 'top_modules'):
            for module in obj.top_modules:
                self.visit(module)
        else:
            print("Warning: top_modules property not found on Design")

    def visit_Module(self, obj):
        print(f"Module: name={obj.name}")

        # Traverse ports if available
        if hasattr(obj, 'ports'):
            for port in obj.ports:
                self.visit(port)
        else:
             # Ports might be empty or missing property
            pass
        
        # Traverse IO decls?
        # For this PoC we stick to ports as requested

    def visit_Port(self, obj):
        # Port has name, and potentially 'direction' if bound.
        # direction is a scalar (int enum) usually.
        # Check attribute existence to avoid crashes if binding missing
        name = getattr(obj, 'name', 'N/A')
        direction = getattr(obj, 'direction', 'N/A')
        print(f"Port: name={name}, direction={direction}")
