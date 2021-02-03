class Stream:
    def __init__(self, data):
        self.data = data
        self.current = 0
        self.scopes = [len(data)]

    def peek_integer(self, count):
        assert(self.current + count <= self.scopes[-1])
        ret = 0
        for i in range(0, count):
            ret += (ord(self.data[self.current + i]) << (i * 8))
        return ret

    def get_integer(self, count):
        ret = self.peek_integer(count)
        self.current += count
        return ret

    def get_char(self):
        ret = self.data[self.current]
        self.current += 1
        return ret

    def get_fixed_length_string(self, count):
        assert(self.current + count <= self.scopes[-1])
        ret = "".join(self.data[self.current : self.current + count])
        self.current += count
        return ret

    def get_string(self):
        null = self.data.find('\x00', self.current)
        assert null >= 0
        ret = self.data[self.current:null]
        self.current = null + 1
        return ret

    def seek(self, offset):
        self.current += offset

    def at_end(self):
        return self.current == self.scopes[-1]

    def push_scope(self, size):
        self.scopes.append(self.current + size)

    def pop_scope(self):
        self.scopes.pop()
