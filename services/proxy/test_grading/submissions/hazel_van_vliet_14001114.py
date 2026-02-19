# Hazel van_Vliet
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
