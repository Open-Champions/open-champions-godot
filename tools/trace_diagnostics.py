"""Bounded diagnostics only; callers retain their existing equality contracts."""
from collections import deque
from itertools import zip_longest
import json
import math
import re


class IncompleteTrace(ValueError):
    """Required producer evidence is unavailable (CLI exit 2, never a pass)."""


MISSING = object()


def brief(value):
    text = '<missing>' if value is MISSING else repr(value)
    return text if len(text) <= 400 else text[:397] + '...'


def difference(left, right, field='', tolerance=False):
    """First nested field in stable order; tolerate only finite numeric leaves."""
    if isinstance(left, dict) and isinstance(right, dict):
        for key in sorted(left.keys() | right.keys()):
            found = difference(left.get(key, MISSING), right.get(key, MISSING),
                               f'{field}.{key}' if field else key, tolerance)
            if found:
                return found
        return None
    if isinstance(left, list) and isinstance(right, list):
        for index, (a, b) in enumerate(zip_longest(left, right, fillvalue=MISSING)):
            found = difference(a, b, f'{field}[{index}]', tolerance)
            if found:
                return found
        return None
    numeric = type(left) in (int, float) and type(right) in (int, float)
    if tolerance and numeric and re.search(
            r"(?:^|\.)(?:position|positions|quaternion_wxyz|model_matrix|skin_matrix)\[", field):
        try:
            if math.isfinite(left) and math.isfinite(right):
                if abs(left - right) <= 1e-12 + 1e-12 * max(abs(left), abs(right)):
                    return None
        except OverflowError:
            pass
    elif (numeric or type(left) is type(right)) and left == right:
        return None
    return field, left, right


def diagnostic(suite, index, field, left, right, labels, history, tick=None):
    location = f'suite={suite} record index={index} (zero-based)'
    if tick is not None:
        location += f' tick={brief(tick)}'
    context = '\n'.join(f'  record index={i}: {brief(row)}' for i, row in history)
    return (f'{location} field={field}: {labels[0]}={brief(left)}; '
            f'{labels[1]}={brief(right)}\npreceding matching state (up to 3 records):\n'
            + (context or '  <none>'))


def structured_diagnostic(suite, left, right, labels, tolerance=True, with_index=False):
    history = deque(maxlen=3)
    for index, (a, b) in enumerate(zip_longest(left, right, fillvalue=MISSING)):
        found = difference(a, b, tolerance=tolerance)
        if found:
            tick = a.get('tick') if isinstance(a, dict) else None
            message = diagnostic(suite, index, *found, labels, history, tick)
            return (index, message) if with_index else message
        history.append((index, a))
    return None


def tsv_diagnostic(suite, left, right, labels, with_index=False):
    """Compare raw lines first, so formatting and headers remain exact gates."""
    history = deque(maxlen=3)
    record_index = 0
    names = ('record', 'operation', 'result') if suite == 'arithmetic' else (
        ('operation', 'configuration', 'inputs', 'result') if suite == 'math' else
        ('case', 'operation', 'arguments', 'result', 'state'))
    for line_index, (a, b) in enumerate(zip_longest(left, right, fillvalue=MISSING)):
        if a != b:
            field, x, y = f'raw_line[{line_index}]', a, b
            if (a is not MISSING and b is not MISSING
                    and not (a.startswith(b"#") or b.startswith(b"#"))):
                aa = a.decode('ascii', errors='backslashreplace').rstrip('\r\n').split('\t')
                bb = b.decode('ascii', errors='backslashreplace').rstrip('\r\n').split('\t')
                for col, (xcol, ycol) in enumerate(zip_longest(aa, bb, fillvalue=MISSING)):
                    if xcol != ycol:
                        field = names[col] if col < len(names) else f'column[{col}]'
                        x, y = xcol, ycol
                        # Locate nested authored JSON state without weakening byte equality.
                        try:
                            nested = difference(json.loads(x), json.loads(y), field)
                            if nested:
                                field, x, y = nested
                        except (ValueError, TypeError):
                            pass
                        break
            message = diagnostic(suite, record_index, field, x, y, labels, history)
            return (record_index, message) if with_index else message
        if a.strip() and not a.startswith(b'#'):
            history.append((record_index, a))
            record_index += 1
    return None


def require_producer(result):
    if result != 'success':
        raise IncompleteTrace(f'native producer result={result}; five-platform comparison unavailable')
