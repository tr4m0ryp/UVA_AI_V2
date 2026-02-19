#!/bin/bash
# Generate mock grading data: assignment, rubric, example, 30 student submissions
set -e
DIR="$(cd "$(dirname "$0")" && pwd)"
SUB="$DIR/submissions"
mkdir -p "$SUB"

# --- Assignment ---
cat > "$DIR/assignment.txt" << 'EOF'
Assignment: Implement a Stack in Python

Write a Python module that implements a Stack data structure with the following requirements:

1. Class named `Stack` with an internal list for storage
2. Methods: push(item), pop(), peek(), is_empty(), size()
3. pop() and peek() must raise an IndexError when the stack is empty
4. Include a __repr__ method that shows the stack contents
5. Write at least 3 unit tests using the `unittest` module in a `if __name__ == "__main__"` block

Constraints:
- Pure Python, no external libraries
- Code must be well-commented
- Follow PEP 8 style guidelines
EOF

# --- Rubric ---
cat > "$DIR/rubric.txt" << 'EOF'
Grading Rubric (Total: 100 points)

1. Class Structure (20 points)
   - Stack class defined correctly (10)
   - Uses internal list for storage (5)
   - Proper __init__ method (5)

2. Core Methods (30 points)
   - push() works correctly (8)
   - pop() works and raises IndexError on empty (8)
   - peek() works and raises IndexError on empty (7)
   - is_empty() returns correct boolean (4)
   - size() returns correct count (3)

3. __repr__ Method (10 points)
   - Returns meaningful string representation (5)
   - Shows stack contents (5)

4. Unit Tests (25 points)
   - At least 3 test cases present (10)
   - Tests cover push/pop operations (8)
   - Tests cover edge cases (empty stack) (7)

5. Code Quality (15 points)
   - Comments present and helpful (5)
   - PEP 8 compliance (5)
   - Clean, readable code (5)
EOF

# --- Example Answer ---
cat > "$DIR/example_answer.txt" << 'EOF'
class Stack:
    """A simple stack implementation using a Python list."""

    def __init__(self):
        # Internal storage
        self._items = []

    def push(self, item):
        """Push an item onto the stack."""
        self._items.append(item)

    def pop(self):
        """Remove and return the top item. Raises IndexError if empty."""
        if self.is_empty():
            raise IndexError("pop from empty stack")
        return self._items.pop()

    def peek(self):
        """Return the top item without removing it. Raises IndexError if empty."""
        if self.is_empty():
            raise IndexError("peek from empty stack")
        return self._items[-1]

    def is_empty(self):
        """Return True if the stack is empty."""
        return len(self._items) == 0

    def size(self):
        """Return the number of items in the stack."""
        return len(self._items)

    def __repr__(self):
        """Show stack contents, top on the right."""
        return f"Stack({self._items})"


import unittest

class TestStack(unittest.TestCase):
    def test_push_and_pop(self):
        s = Stack()
        s.push(1)
        s.push(2)
        self.assertEqual(s.pop(), 2)
        self.assertEqual(s.pop(), 1)

    def test_peek(self):
        s = Stack()
        s.push("a")
        self.assertEqual(s.peek(), "a")
        self.assertEqual(s.size(), 1)

    def test_empty_stack_errors(self):
        s = Stack()
        with self.assertRaises(IndexError):
            s.pop()
        with self.assertRaises(IndexError):
            s.peek()

    def test_is_empty_and_size(self):
        s = Stack()
        self.assertTrue(s.is_empty())
        s.push(42)
        self.assertFalse(s.is_empty())
        self.assertEqual(s.size(), 1)

if __name__ == "__main__":
    unittest.main()
EOF

# --- Student name/ID pools ---
FIRST=(Emma Liam Noah Olivia Ava Sophia Jackson Mia Lucas Amelia
       Ethan Harper Aiden Ella James Aria Logan Chloe Benjamin Lily
       Mason Grace Elijah Zoey Carter Nora Owen Stella Henry Hazel)
LAST=(de_Vries Jansen Bakker Visser Smit Meijer de_Boer Mulder
      de_Groot Bos Peters Hendriks van_Dijk Dekker Brouwer de_Jong
      Dijkstra van_Leeuwen Willems van_den_Berg Vermeer Kok de_Graaf
      van_der_Linden Huisman Molenaar de_Bruin Schouten Prins van_Vliet)

# Quality templates: excellent(5), good(8), average(10), poor(5), bad(2)
write_submission() {
    local idx=$1 quality=$2
    local first="${FIRST[$((idx % 30))]}"
    local last="${LAST[$((idx % 30))]}"
    local name="${first} ${last}"
    local sid=$((14000000 + idx * 37 + idx * idx % 100))
    local fname="${first,,}_${last,,}_${sid}.py"

    case "$quality" in
    excellent)
cat > "$SUB/$fname" << PYEOF
# Stack implementation by $name ($sid)
# Full implementation with comprehensive tests

class Stack:
    """A stack (LIFO) data structure."""

    def __init__(self):
        # Private list to hold elements
        self._data = []

    def push(self, item):
        """Add item to top of stack."""
        self._data.append(item)

    def pop(self):
        """Remove and return top item. Raises IndexError if empty."""
        if self.is_empty():
            raise IndexError("Cannot pop from an empty stack")
        return self._data.pop()

    def peek(self):
        """Return top item without removing. Raises IndexError if empty."""
        if self.is_empty():
            raise IndexError("Cannot peek at an empty stack")
        return self._data[-1]

    def is_empty(self):
        """Check if the stack has no elements."""
        return len(self._data) == 0

    def size(self):
        """Return the number of elements."""
        return len(self._data)

    def __repr__(self):
        """String representation showing contents."""
        return f"Stack(top -> {self._data[::-1]})"

import unittest

class TestStack(unittest.TestCase):
    def setUp(self):
        self.stack = Stack()

    def test_push_pop_order(self):
        for i in range(5):
            self.stack.push(i)
        for i in range(4, -1, -1):
            self.assertEqual(self.stack.pop(), i)

    def test_peek_does_not_remove(self):
        self.stack.push("hello")
        self.assertEqual(self.stack.peek(), "hello")
        self.assertEqual(self.stack.size(), 1)

    def test_empty_operations_raise(self):
        with self.assertRaises(IndexError):
            self.stack.pop()
        with self.assertRaises(IndexError):
            self.stack.peek()

    def test_size_and_is_empty(self):
        self.assertTrue(self.stack.is_empty())
        self.stack.push(1)
        self.assertFalse(self.stack.is_empty())
        self.assertEqual(self.stack.size(), 1)

    def test_repr(self):
        self.stack.push("a")
        self.assertIn("a", repr(self.stack))

if __name__ == "__main__":
    unittest.main()
PYEOF
    ;;
    good)
cat > "$SUB/$fname" << PYEOF
# $name - $sid
# Stack implementation

class Stack:
    def __init__(self):
        self._items = []

    def push(self, item):
        self._items.append(item)

    def pop(self):
        if not self._items:
            raise IndexError("pop from empty stack")
        return self._items.pop()

    def peek(self):
        if not self._items:
            raise IndexError("peek from empty stack")
        return self._items[-1]

    def is_empty(self):
        return len(self._items) == 0

    def size(self):
        return len(self._items)

    def __repr__(self):
        return f"Stack({self._items})"

import unittest

class TestStack(unittest.TestCase):
    def test_push_pop(self):
        s = Stack()
        s.push(10)
        s.push(20)
        self.assertEqual(s.pop(), 20)

    def test_peek(self):
        s = Stack()
        s.push("x")
        self.assertEqual(s.peek(), "x")

    def test_empty_error(self):
        s = Stack()
        with self.assertRaises(IndexError):
            s.pop()

if __name__ == "__main__":
    unittest.main()
PYEOF
    ;;
    average)
cat > "$SUB/$fname" << PYEOF
# $name
class Stack:
    def __init__(self):
        self.items = []

    def push(self, item):
        self.items.append(item)

    def pop(self):
        if len(self.items) == 0:
            raise IndexError("empty")
        return self.items.pop()

    def peek(self):
        return self.items[-1]  # no error handling

    def is_empty(self):
        return self.items == []

    def size(self):
        return len(self.items)

    def __repr__(self):
        return str(self.items)

# tests
import unittest
class Test(unittest.TestCase):
    def test_basic(self):
        s = Stack()
        s.push(1)
        self.assertEqual(s.pop(), 1)

    def test_size(self):
        s = Stack()
        s.push("a")
        self.assertEqual(s.size(), 1)

    def test_empty(self):
        s = Stack()
        self.assertTrue(s.is_empty())

if __name__ == "__main__":
    unittest.main()
PYEOF
    ;;
    poor)
cat > "$SUB/$fname" << PYEOF
# $name $sid
class Stack:
    def __init__(self):
        self.stack = []

    def push(self, val):
        self.stack.append(val)

    def pop(self):
        return self.stack.pop()  # no check for empty

    def peek(self):
        return self.stack[-1]   # no check

    def is_empty(self):
        if len(self.stack) > 0:
            return False
        return True

    # missing size() and __repr__

s = Stack()
s.push(1)
s.push(2)
print(s.pop())
print(s.peek())
PYEOF
    ;;
    bad)
cat > "$SUB/$fname" << PYEOF
# $name
# i tried but ran out of time
class stack:
    def __init__(self):
        self.l = []
    def push(self, x):
        self.l.append(x)
    def pop(self):
        return self.l.pop()

s = stack()
s.push(5)
print(s.pop())
PYEOF
    ;;
    esac
    echo "$name|$sid|$fname"
}

# Distribution: 5 excellent, 8 good, 10 average, 5 poor, 2 bad
echo "Generating 30 student submissions..."
IDX=0
for q in excellent excellent excellent excellent excellent \
         good good good good good good good good \
         average average average average average average average average average average \
         poor poor poor poor poor \
         bad bad; do
    write_submission $IDX "$q"
    IDX=$((IDX + 1))
done

echo "Done. Files in: $SUB/"
echo "Assignment: $DIR/assignment.txt"
echo "Rubric:     $DIR/rubric.txt"
echo "Example:    $DIR/example_answer.txt"
ls -1 "$SUB" | wc -l
echo "submission files generated."
