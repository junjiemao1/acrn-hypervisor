# Copyright (C) 2023 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

from uuid import UUID
from acpiparser.uuids import *

class ExternalFacingPort:
    def __init__(self, data):
        self.is_external_facing = 0
        self.uid = 0

        for elem in data.elements:
            try:
                key, value = elem.elements
                print(key.get(), value.get())
                if key.get() == "ExternalFacingPort":
                    self.is_external_facing = value.get()
                elif key.get() == "UID":
                    self.uid = value.get()
            except IndexError:
                pass

def pairwise(iterable):
    a = iter(iterable)
    return zip(a, a)

def parse_dsd(dsd_package):
    acc = {}

    for uuid_buffer, data_package in pairwise(dsd_package.elements):
        uuid = UUID(bytes_le=uuid_buffer.data)
        if uuid == UUID_EXTERNAL_FACING_PORT:
            acc[UUID_EXTERNAL_FACING_PORT] = ExternalFacingPort(data_package)

    return acc
