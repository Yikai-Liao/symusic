class Widget:
    """Example pure-Python class used to validate autodoc rendering.

    Provides attributes, a property, instance methods, a classmethod,
    and a staticmethod with type hints to exercise Sphinx behaviors.
    """

    def __init__(self, name: str, x: int = 0) -> None:
        self.name: str = name
        self.x: int = x

    @property
    def size(self) -> int:
        """Computed size derived from ``x``."""
        return self.x * 2

    def inc(self, by: int = 1) -> int:
        """Increment ``x`` by ``by`` and return the new value."""
        self.x += by
        return self.x

    @classmethod
    def from_default(cls) -> "Widget":
        """Create a default widget."""
        return cls("default", 1)

    @staticmethod
    def add(a: int, b: int) -> int:
        """Add two integers."""
        return a + b