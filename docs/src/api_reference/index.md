# API Reference

This section provides detailed documentation for Symusic's API. Each class and function is documented with its parameters, return types, and usage examples.

## Organization

The API reference is organized by the main components of the library:

- **[Score](./score.md)**: The container for a complete musical piece
- **[Track](./track.md)**: Individual tracks within a score
- **[Events](./events.md)**: Musical events like notes, control changes, etc.
- **[Time Units](./time_units.md)**: Different time representation systems
- **[Synthesizer](./synthesizer.md)**: Audio synthesis functionality
- **[Utility Functions](./utility.md)**: Helper functions for common tasks

## Type Conventions

Throughout the API documentation, you'll see the following type conventions:

- `ttype`: Time unit type (Tick, Quarter, or Second)
- `unit`: The actual value type corresponding to the time unit (int for Tick, float for Quarter and Second)
- Generics are often used to make functions work with different time unit types

## Common Patterns

Most classes in Symusic follow these common design patterns:

1. **Time Unit Conversion**: Most objects can be converted between different time units using the `to()` method
2. **Inplace vs. Copy**: Most modification operations have both inplace and copy versions
3. **Serialization**: All objects support Python's pickle protocol for efficient serialization
4. **Factory Methods**: Classes often provide factory methods (like `from_file`, `from_midi`) for different creation paths

## Getting Started

If you're new to Symusic, we recommend starting with the [Score](./score.md) class documentation, as it's the main entry point for most tasks. For loading and saving files, check the I/O methods in the Score class.

For examples of how to use the API effectively, see the [Tutorials](../tutorials/index.md) and [Examples](../examples/index.md) sections. 