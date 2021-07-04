#!/usr/bin/env python3

import os
import portio

status = portio.ioperm(0x2E, 2, 1)
if status:
    print(os.strerror(status))
    sys.exit(1)

portio.outb(0x60, 0x2e)
v = portio.inb(0x2f)
print(v)

# f = open("/dev/port", "w+b", buffering=0)
# f.seek(0x2e)
# f.write(bytes(b"\x60"))
# f.seek(0x2f)
# print(f.read(1))
