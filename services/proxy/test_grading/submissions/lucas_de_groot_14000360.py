# Lucas de_Groot - 14000360
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
