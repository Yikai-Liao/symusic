import re

from sphinx.application import Sphinx
from sphinx.ext.autodoc import MethodDocumenter


class NBFunctionDocumenter(MethodDocumenter):
    directivetype = "method"
    objtype = "method"
    priority = 20  # higher than default AttributeDocumenter

    @classmethod
    def can_document_member(cls, member, membername, isattr, parent):
        tname = type(member).__name__
        return bool(tname.startswith("nb_") or callable(member))


def _fix_examples(app: Sphinx, what: str, name: str, obj, options, lines):
    # Convert inline backtick examples to proper code-blocks
    if not lines:
        return
    new_lines = []
    in_inline = False
    buffer = []
    for line in lines:
        if line.strip().startswith("`python"):
            in_inline = True
            continue
        if in_inline:
            if line.strip().endswith("`"):
                in_inline = False
                # emit code-block
                new_lines.append(".. code-block:: python")
                new_lines.append("")
                new_lines.extend("    " + b for b in buffer)
                buffer = []
                continue
            buffer.append(line)
        else:
            # Drop standalone dashed separators that break lists
            if (
                line.strip() == "--------------"
                or line.strip().replace(" ", "") == "--------------"
                or re.match(r"^\s*-{6,}\s*$", line)
            ):
                continue
            new_lines.append(line)
    if new_lines != lines:
        lines[:] = new_lines


def setup(app: Sphinx):
    app.add_autodocumenter(NBFunctionDocumenter)
    app.connect("autodoc-process-docstring", _fix_examples)
    return {"version": "0.1", "parallel_read_safe": True}
