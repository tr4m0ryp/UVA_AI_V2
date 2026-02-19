# Stack implementation by Liam Jansen (14000038)
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
