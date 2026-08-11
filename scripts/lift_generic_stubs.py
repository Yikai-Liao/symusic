from __future__ import annotations

import argparse
import ast
import copy
import textwrap
from collections import Counter
from pathlib import Path
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from collections.abc import Iterable, Sequence

TIME_UNITS = ("Tick", "Quarter", "Second")
GENERIC_BASES = (
    "Score",
    "Track",
    "Note",
    "KeySignature",
    "TimeSignature",
    "Tempo",
    "ControlChange",
    "Pedal",
    "PitchBend",
    "TextMeta",
)
EVENT_BASES = GENERIC_BASES[2:]


class StubLiftError(RuntimeError):
    pass


def _dump(node: ast.AST | None) -> str:
    return ast.dump(node, include_attributes=False)


def _generic(name: str, parameter: str = "TimeUnitT") -> ast.Subscript:
    return ast.Subscript(
        value=ast.Name(id=name, ctx=ast.Load()),
        slice=ast.Name(id=parameter, ctx=ast.Load()),
        ctx=ast.Load(),
    )


class _GenericizeSpecializations(ast.NodeTransformer):
    def visit_Name(self, node: ast.Name) -> ast.expr:
        for base in GENERIC_BASES:
            for unit in TIME_UNITS:
                if node.id == f"{base}{unit}":
                    return ast.copy_location(_generic(base, unit), node)
                if node.id == f"{base}{unit}List":
                    container = "PyVec" if base in EVENT_BASES else "list"
                    return ast.copy_location(
                        ast.Subscript(
                            value=ast.Name(id=container, ctx=ast.Load()),
                            slice=_generic(base, unit),
                            ctx=ast.Load(),
                        ),
                        node,
                    )
        return node


def _genericize_specializations(annotation: ast.expr | None) -> ast.expr | None:
    if annotation is None:
        return None
    return _GenericizeSpecializations().visit(copy.deepcopy(annotation))


def _union(expressions: Sequence[ast.expr]) -> ast.expr:
    unique: list[ast.expr] = []
    seen: set[str] = set()
    for expression in expressions:
        key = _dump(expression)
        if key not in seen:
            seen.add(key)
            unique.append(copy.deepcopy(expression))

    result = unique[0]
    for expression in unique[1:]:
        result = ast.BinOp(left=result, op=ast.BitOr(), right=expression)
    return result


def _merge_annotations(
    annotations: Sequence[ast.expr | None],
    units: Sequence[str],
    context: str,
    event_variants: Sequence[tuple[str, str]] | None = None,
) -> ast.expr | None:
    if all(annotation is None for annotation in annotations):
        return None
    if any(annotation is None for annotation in annotations):
        message = f"Cannot merge missing annotation for {context}"
        raise StubLiftError(message)

    expressions = [annotation for annotation in annotations if annotation is not None]
    if len({_dump(annotation) for annotation in expressions}) == 1:
        return copy.deepcopy(expressions[0])

    if all(isinstance(annotation, ast.Name) for annotation in expressions):
        names = [
            annotation.id
            for annotation in expressions
            if isinstance(annotation, ast.Name)
        ]
        if names == list(units):
            return ast.Name(id="TimeUnitT", ctx=ast.Load())
        if event_variants is not None:
            if names == [f"{base}{unit}" for base, unit in event_variants]:
                return ast.Name(id="EventT", ctx=ast.Load())
            if names == [f"{base}{unit}List" for base, unit in event_variants]:
                return _generic("PyVec", "EventT")
        for base in GENERIC_BASES:
            if names == [f"{base}{unit}" for unit in units]:
                return _generic(base)
            if names == [f"{base}{unit}List" for unit in units]:
                container = "PyVec" if base in EVENT_BASES else "list"
                return ast.Subscript(
                    value=ast.Name(id=container, ctx=ast.Load()),
                    slice=_generic(base),
                    ctx=ast.Load(),
                )

    if all(isinstance(annotation, ast.Subscript) for annotation in expressions):
        subscripts = [
            annotation
            for annotation in expressions
            if isinstance(annotation, ast.Subscript)
        ]
        if len({_dump(annotation.value) for annotation in subscripts}) == 1:
            return ast.Subscript(
                value=copy.deepcopy(subscripts[0].value),
                slice=_merge_annotations(
                    [annotation.slice for annotation in subscripts],
                    units,
                    context,
                    event_variants,
                ),
                ctx=ast.Load(),
            )

    if all(isinstance(annotation, ast.Tuple) for annotation in expressions):
        tuples = [
            annotation
            for annotation in expressions
            if isinstance(annotation, ast.Tuple)
        ]
        if len({len(annotation.elts) for annotation in tuples}) == 1:
            return ast.Tuple(
                elts=[
                    _merge_annotations(
                        [annotation.elts[index] for annotation in tuples],
                        units,
                        context,
                        event_variants,
                    )
                    for index in range(len(tuples[0].elts))
                ],
                ctx=ast.Load(),
            )

    return _union(expressions)


def _decorator_key(function: ast.FunctionDef) -> tuple[str, ...]:
    return tuple(_dump(decorator) for decorator in function.decorator_list)


def _function_map(
    class_node: ast.ClassDef,
) -> tuple[
    list[tuple[tuple[object, ...], ast.FunctionDef]],
    dict[tuple[object, ...], ast.FunctionDef],
]:
    counters: Counter[tuple[object, ...]] = Counter()
    ordered: list[tuple[tuple[object, ...], ast.FunctionDef]] = []
    mapped: dict[tuple[object, ...], ast.FunctionDef] = {}

    for node in class_node.body:
        if not isinstance(node, ast.FunctionDef) or node.name == "__init__":
            continue
        base_key: tuple[object, ...] = (node.name, _decorator_key(node))
        key = (*base_key, counters[base_key])
        counters[base_key] += 1
        ordered.append((key, node))
        mapped[key] = node

    return ordered, mapped


def _all_arguments(arguments: ast.arguments) -> Iterable[tuple[str, ast.arg]]:
    yield from (("posonly", argument) for argument in arguments.posonlyargs)
    yield from (("positional", argument) for argument in arguments.args)
    if arguments.vararg is not None:
        yield "vararg", arguments.vararg
    yield from (("kwonly", argument) for argument in arguments.kwonlyargs)
    if arguments.kwarg is not None:
        yield "kwarg", arguments.kwarg


def _finish_function(function: ast.FunctionDef) -> ast.FunctionDef:
    function.body = [ast.Expr(value=ast.Constant(value=Ellipsis))]
    function.type_comment = None
    return ast.fix_missing_locations(function)


def _merge_function(
    functions: Sequence[ast.FunctionDef],
    units: Sequence[str],
    context: str,
    event_variants: Sequence[tuple[str, str]] | None = None,
) -> ast.FunctionDef:
    result = copy.deepcopy(functions[-1])
    argument_groups = [list(_all_arguments(function.args)) for function in functions]
    result_arguments = list(_all_arguments(result.args))
    shapes = [
        [(kind, argument.arg) for kind, argument in arguments]
        for arguments in argument_groups
    ]
    if any(shape != shapes[0] for shape in shapes[1:]):
        message = f"Cannot merge argument lists for {context}"
        raise StubLiftError(message)

    for index, (_, result_argument) in enumerate(result_arguments):
        result_argument.annotation = _merge_annotations(
            [arguments[index][1].annotation for arguments in argument_groups],
            units,
            f"{context}.{result_argument.arg}",
            event_variants,
        )

    positional_defaults = [function.args.defaults for function in functions]
    keyword_defaults = [function.args.kw_defaults for function in functions]
    if (
        len({len(defaults) for defaults in positional_defaults}) != 1
        or len({len(defaults) for defaults in keyword_defaults}) != 1
    ):
        message = f"Cannot merge default values for {context}"
        raise StubLiftError(message)

    for index, defaults in enumerate(zip(*positional_defaults)):
        if len({_dump(default) for default in defaults}) != 1:
            result.args.defaults[index] = ast.Constant(value=Ellipsis)
    for index, defaults in enumerate(zip(*keyword_defaults)):
        if len({_dump(default) for default in defaults}) != 1:
            result.args.kw_defaults[index] = ast.Constant(value=Ellipsis)

    result.returns = _merge_annotations(
        [function.returns for function in functions],
        units,
        f"{context}.return",
        event_variants,
    )
    return _finish_function(result)


def _same_signatures(functions: Sequence[ast.FunctionDef]) -> bool:
    signatures = {
        _dump(_finish_function(copy.deepcopy(function))) for function in functions
    }
    return len(signatures) == 1


def _is_restrictable(function: ast.FunctionDef) -> bool:
    if any(
        (
            isinstance(decorator, ast.Name)
            and decorator.id in {"property", "staticmethod", "classmethod"}
        )
        or (
            isinstance(decorator, ast.Attribute)
            and decorator.attr in {"setter", "deleter"}
        )
        for decorator in function.decorator_list
    ):
        return False
    return any(argument.arg == "self" for _, argument in _all_arguments(function.args))


def _restricted_function(
    function: ast.FunctionDef,
    base: str,
    unit: str,
    add_overload: bool,
) -> ast.FunctionDef:
    result = copy.deepcopy(function)
    for _, argument in _all_arguments(result.args):
        if argument.arg == "self":
            argument.annotation = _generic(base, unit)
            break

    has_overload = any(
        isinstance(decorator, ast.Name) and decorator.id == "overload"
        for decorator in result.decorator_list
    )
    if add_overload and not has_overload:
        result.decorator_list.insert(0, ast.Name(id="overload", ctx=ast.Load()))
    result.returns = _genericize_specializations(result.returns)
    return _finish_function(result)


def _specialization(class_node: ast.ClassDef) -> tuple[str, str] | None:
    for base in class_node.bases:
        if not isinstance(base, ast.Subscript):
            continue
        if not isinstance(base.value, ast.Name) or not isinstance(base.slice, ast.Name):
            continue
        if base.value.id in GENERIC_BASES and base.slice.id in TIME_UNITS:
            return base.value.id, base.slice.id
    return None


def _ordered_keys(
    maps: Sequence[
        tuple[
            list[tuple[tuple[object, ...], ast.FunctionDef]],
            dict[tuple[object, ...], ast.FunctionDef],
        ]
    ],
) -> list[tuple[object, ...]]:
    result: list[tuple[object, ...]] = []
    seen: set[tuple[object, ...]] = set()
    for ordered, _ in maps:
        for key, _ in ordered:
            if key not in seen:
                seen.add(key)
                result.append(key)
    return result


def lift_generic_stubs(source: str) -> str:
    module = ast.parse(source)
    classes = {
        node.name: node for node in module.body if isinstance(node, ast.ClassDef)
    }
    specializations: dict[str, dict[str, ast.ClassDef]] = {
        base: {} for base in GENERIC_BASES
    }

    for class_node in classes.values():
        specialization = _specialization(class_node)
        if specialization is not None:
            base, unit = specialization
            specializations[base][unit] = class_node

    insertions: list[tuple[int, str]] = []
    for base in GENERIC_BASES:
        base_node = classes.get(base)
        if base_node is None:
            message = f"Missing generic base class {base}"
            raise StubLiftError(message)

        by_unit = specializations[base]
        missing = set(TIME_UNITS) - set(by_unit)
        if missing:
            message = (
                f"Missing specializations for {base}: {', '.join(sorted(missing))}"
            )
            raise StubLiftError(message)

        maps = [_function_map(by_unit[unit]) for unit in TIME_UNITS]
        generated: list[str] = []
        for key in _ordered_keys(maps):
            present = [
                (unit, mapping[key])
                for unit, (_, mapping) in zip(TIME_UNITS, maps)
                if key in mapping
            ]
            functions = [function for _, function in present]
            if len(present) == len(TIME_UNITS) and _same_signatures(functions):
                common = _finish_function(copy.deepcopy(functions[0]))
                generated.append(textwrap.indent(ast.unparse(common), "    "))
                continue

            if all(_is_restrictable(function) for function in functions):
                add_overload = len(present) > 1
                generated.extend(
                    textwrap.indent(
                        ast.unparse(
                            _restricted_function(function, base, unit, add_overload)
                        ),
                        "    ",
                    )
                    for unit, function in present
                )
                continue

            if len(present) != len(TIME_UNITS):
                continue
            merged = _merge_function(
                functions,
                TIME_UNITS,
                f"{base}.{functions[-1].name}",
            )
            generated.append(textwrap.indent(ast.unparse(merged), "    "))

        if not generated:
            message = f"No members generated for {base}"
            raise StubLiftError(message)
        insertions.append((base_node.end_lineno or 0, "\n\n".join(generated)))

    event_variants = [(base, unit) for base in EVENT_BASES for unit in TIME_UNITS]
    event_lists = [classes[f"{base}{unit}List"] for base, unit in event_variants]
    event_maps = [_function_map(class_node) for class_node in event_lists]
    common_keys = set(event_maps[0][1])
    for _, mapping in event_maps[1:]:
        common_keys.intersection_update(mapping)

    generated = []
    units = [unit for _, unit in event_variants]
    for key, function in event_maps[-1][0]:
        if key not in common_keys or function.name in {"numpy", "from_numpy"}:
            continue
        functions = [mapping[key] for _, mapping in event_maps]
        merged = _merge_function(
            functions,
            units,
            f"PyVec.{function.name}",
            event_variants,
        )
        generated.append(textwrap.indent(ast.unparse(merged), "    "))

    pyvec = classes.get("PyVec")
    if pyvec is None or not generated:
        message = "Cannot generate the generic PyVec surface"
        raise StubLiftError(message)
    insertions.append((pyvec.end_lineno or 0, "\n\n".join(generated)))

    lines = source.splitlines()
    for line_number, generated in sorted(insertions, reverse=True):
        lines[line_number:line_number] = ["", generated]
    return "\n".join(lines) + "\n"


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Lift nanobind specializations with restricted-self signatures."
    )
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    source = args.input.read_text(encoding="utf-8")
    result = lift_generic_stubs(source)
    ast.parse(result)
    args.output.write_text(result, encoding="utf-8")


if __name__ == "__main__":
    main()
