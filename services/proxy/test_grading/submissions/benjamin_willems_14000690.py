# Benjamin Willems
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
