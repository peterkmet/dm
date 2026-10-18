# SPDX-License-Identifier: GPL-3.0-or-later
"""The constants of the C headers: #define lines and enum members of src/**/*.h, evaluated once at import.

Tests write C.KEY_RETURN. A value is an integer expression over literals, other constants, casts, shifts and
arithmetic; a define with another value (a string, a macro with arguments, sizeof) is not a constant here.
"""

import ast
import operator
import re
from pathlib import Path

_SRC = Path(__file__).resolve().parents[2] / "src"
_COMMENT = re.compile(r"/\*.*?\*/|//[^\n]*", re.S)
_DEFINE = re.compile(r"^[ \t]*#[ \t]*define[ \t]+([A-Z][A-Z0-9_]*)[ \t]+(.+)$", re.M)
_ENUM = re.compile(r"\benum\b\s*\w*\s*\{([^}]*)\}", re.S)
_INT_SUFFIX = re.compile(r"\b(0[xX][0-9a-fA-F]+|\d+)[uUlL]+\b")
_CHAR = re.compile(r"'(?:\\.|[^'\\])'")
_CAST = re.compile(r"\(\s*(u?int(?:8|16|32)_t)\s*\)")
_OCTAL = re.compile(r"\b0([0-7]+)\b")
_WIDTH = {"int8_t": (8, True), "uint8_t": (8, False), "int16_t": (16, True), "uint16_t": (16, False),
          "int32_t": (32, True), "uint32_t": (32, False)}


def _wrap(value, bits, signed):
    value &= (1 << bits) - 1
    return value - (1 << bits) if signed and value >> (bits - 1) else value


def _c_div(a, b):
    quotient = abs(a) // abs(b)
    return quotient if (a >= 0) == (b >= 0) else -quotient


_BINARY = {ast.Add: operator.add, ast.Sub: operator.sub, ast.Mult: operator.mul, ast.Div: _c_div,
           ast.Mod: lambda a, b: a - b * _c_div(a, b), ast.LShift: operator.lshift, ast.RShift: operator.rshift,
           ast.BitAnd: operator.and_, ast.BitOr: operator.or_, ast.BitXor: operator.xor}
_UNARY = {ast.USub: operator.neg, ast.UAdd: operator.pos, ast.Invert: operator.invert}


def _value(node, known):
    if isinstance(node, ast.Constant) and type(node.value) is int:
        return node.value
    if isinstance(node, ast.Name):
        return known[node.id]
    if isinstance(node, ast.UnaryOp) and type(node.op) in _UNARY:
        return _UNARY[type(node.op)](_value(node.operand, known))
    if isinstance(node, ast.BinOp) and isinstance(node.op, ast.Pow) and isinstance(node.left, ast.Name) and node.left.id in _WIDTH:
        return _wrap(_value(node.right, known), *_WIDTH[node.left.id])
    if isinstance(node, ast.BinOp) and type(node.op) in _BINARY:
        return _BINARY[type(node.op)](_value(node.left, known), _value(node.right, known))
    raise ValueError(ast.dump(node))


def evaluate(expression, known):
    """The value of a C integer constant expression; KeyError for an unknown name, ValueError for other C.

    A cast (int16_t)x becomes int16_t ** x: ** binds like a C cast here and does not occur in C."""
    text = _INT_SUFFIX.sub(r"\1", expression)
    text = _CHAR.sub(lambda m: str(ord(ast.literal_eval(m.group()))), text)
    text = _OCTAL.sub(r"0o\1", _CAST.sub(r"\1 ** ", text))
    try:
        tree = ast.parse(text.strip(), mode="eval")
    except SyntaxError as error:
        raise ValueError(expression) from error
    return _value(tree.body, known)


def parse(texts):
    """The constants of C header texts as {name: value}; an enum member without a value counts up from the previous one."""
    raw = []
    for text in texts:
        text = _COMMENT.sub(" ", text).replace("\\\n", " ")
        raw += _DEFINE.findall(text)
        for body in _ENUM.findall(text):
            previous = None
            for item in body.split(","):
                if not (item := item.strip()):
                    continue
                name, _, expression = (part.strip() for part in item.partition("="))
                raw.append((name, expression or (f"({previous}) + 1" if previous else "0")))
                previous = name
    known, pending = {}, raw
    while pending:
        left = []
        for name, expression in pending:
            try:
                known[name] = evaluate(expression, known)
            except KeyError:
                left.append((name, expression))
            except (ValueError, TypeError, ZeroDivisionError):
                pass
        if len(left) == len(pending):
            break
        pending = left
    return known


VALUES = parse(path.read_text(errors="replace") for path in sorted(p for p in _SRC.rglob("*.h") if p.relative_to(_SRC).parts[0] != "third_party"))
globals().update(VALUES)


def __getattr__(name):
    raise AttributeError(f"{name} is not a constant of src/**/*.h")
