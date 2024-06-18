#!/usr/bin/env python
#
# Public Domain 2014-present MongoDB, Inc.
# Public Domain 2008-2014 WiredTiger, Inc.
#
# This is free and unencumbered software released into the public domain.
#
# Anyone is free to copy, modify, publish, use, compile, sell, or
# distribute this software, either in source code form or as a compiled
# binary, for any purpose, commercial or non-commercial, and by any
# means.
#
# In jurisdictions that recognize copyright laws, the author or authors
# of this software dedicate any and all copyright interest in the
# software to the public domain. We make this dedication for the benefit
# of the public at large and to the detriment of our heirs and
# successors. We intend this dedication to be an overt act of
# relinquishment in perpetuity of all present and future rights to this
# software under copyright law.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
# IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
# OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
# ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
# OTHER DEALINGS IN THE SOFTWARE.

import os, time, wiredtiger, wttest

StorageSource = wiredtiger.StorageSource  # easy access to constants

# test_oligarch03.py
#    Basic oligarch tree cursor insert and read
class test_oligarch03(wttest.WiredTigerTestCase):

    uri_base = "test_oligarch03"
    conn_config = 'log=(enabled),verbose=[oligarch]'

    uri = "oligarch:" + uri_base

    # Test inserting a record into an oligarch tree
    def test_oligarch03(self):
        base_create = 'key_format=S,value_format=S'

        self.pr("create oligarch tree")
        self.session.create(self.uri, base_create)

        self.pr('opening cursor')
        cursor = self.session.open_cursor(self.uri, None, None)

        self.pr('Inserting a value')
        cursor["Hello"] = "World"
        cursor["Hi"] = "There"
        cursor["OK"] = "Go"

        cursor.set_key("Hello")
        cursor.search()
        value = cursor.get_value()
        value = cursor["Hello"]
        print("Search retrieved: " + cursor.get_key() + ":" + cursor.get_value())

        cursor.reset()
        while cursor.next() == 0:
            print("Traversal retrieved: " + cursor.get_key() + ":" + cursor.get_value())

        cursor.reset()
        while cursor.prev() == 0:
            print("Traversal retrieved: " + cursor.get_key() + ":" + cursor.get_value())

        self.pr('opening cursor')
        time.sleep(0.5)
        cursor.close()

