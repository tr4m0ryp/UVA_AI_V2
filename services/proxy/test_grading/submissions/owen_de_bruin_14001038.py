# Owen de_Bruin 14001038
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
