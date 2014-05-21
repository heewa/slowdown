#!/usr/bin/env python
"""Simple measurement of speed changes in mongo (not absolute "speed" by any
particular standard), by tracking speed of writing/querying/removing records.

Warning!!! This program tries to not affect data it didn't write, but if you
point it at an existing collection, don't be super shocked if there are missing
records or new/weird records in there.
"""
from datetime import datetime, timedelta
from sys import exit
from threading import Thread
from time import sleep

from pymongo import MongoClient, errors, ReadPreference
from bson.objectid import ObjectId


# globals, cuz lazy, shut up
Record = {
    'a_key': 'a value',
    'a_big_key': ['lotsa data!', 'yay!'] * 2048,
    'a_small_key______well_the_value_is_small__not_the_key': None,
}


ShouldExit = False


ReadsPerSec = 0
class ReadThread(Thread):
    def __init__(self, host, port, db, coll_name, conn_options):
        Thread.__init__(self)
        self.coll = MongoClient(host, port, **conn_options)[db][coll_name]

    def run(self):
        global ReadsPerSec
        global ShouldExit

        ReadsPerSec = 0
        interval = timedelta(seconds=1.8)
        last = datetime.now()
        things = 0

        while not ShouldExit:
            for i in xrange(10):
                list(self.coll.find({'not_a_key': {'$ne': 1}}))
                things += 1

            now = datetime.now()
            elapsed = now - last
            if elapsed >= interval:
                ReadsPerSec = things / elapsed.total_seconds()
                things = 0
                last = now


WritesPerSec = 0
class WriteThread(Thread):
    def __init__(self, host, port, db, coll_name, conn_options):
        Thread.__init__(self)
        self.coll = MongoClient(host, port, **conn_options)[db][coll_name]

    def run(self):
        global WritesPerSec
        global ShouldExit

        WritesPerSec = 0
        interval = timedelta(seconds=1.8)
        last = datetime.now()
        things = 0

        while not ShouldExit:
            for i in xrange(10):
                # This can insert so quickly that we get duplicate ID errors.
                # So, make up IDs cuz whatever.
                Record['_id'] = {'a_real_id': ObjectId(),
                                 'a_num_cuz_we_so_fast': i}
                self.coll.insert(Record)
                things += 1

            now = datetime.now()
            elapsed = now - last
            if elapsed >= interval:
                WritesPerSec = things / elapsed.total_seconds()
                things = 0
                last = now


RemovesPerSec = 0
class RemoveThread(Thread):
    def __init__(self, host, port, db, coll_name, conn_options):
        Thread.__init__(self)
        self.coll = MongoClient(host, port, **conn_options)[db][coll_name]

    def run(self):
        global RemovesPerSec
        global ShouldExit

        RemovesPerSec = 0
        interval = timedelta(seconds=1.8)
        last = datetime.now()
        things = 0

        while not ShouldExit:
            for i in xrange(10):
                self.coll.remove({'a_key': Record['a_key']})
                things += 1

            now = datetime.now()
            elapsed = now - last
            if elapsed >= interval:
                RemovesPerSec = things / elapsed.total_seconds()
                things = 0
                last = now


def main():
    global ShouldExit

    from optparse import OptionParser
    parser = OptionParser(usage='Usage: %prog [options] '
                                'host[:port]/db/collection')

    parser.add_option('', '--readonly', action='store_true', dest='readonly',
                      default=False,
                      help='Only query existing data, don\'t write/del any.')

    options, args = parser.parse_args()

    if not args or not args[0] or args[0].count('/') != 2:
        exit('Specify destination like host[:port]/db/collection')

    host, db, coll_name = args[0].split('/', 2)

    if ':' in host:
        host, port = host.split(':', 1)
        port = int(port)
    else:
        port = 27017

    if not host or not db or not coll_name:
        exit('Specify destination like host[:port]/db/collection')

    conn_options = {}
    if options.readonly:
        conn_options['read_preference'] = ReadPreference.NEAREST
    coll = MongoClient(host, port, **conn_options)[db][coll_name]

    # Seed coll with at least some records. This inserts so quickly that
    # we get duplicate ID errors. So, make up IDs cuz whatever.
    if not options.readonly:
        print 'Seeding collection with some records.'
        for i in xrange(1000):
            Record['_id'] = {'a_real_id': ObjectId(),
                             'a_num_cuz_we_so_fast': i}
            coll.insert(Record)

        # Remove id so things work in the lazy way I wrote it down there.
        del Record['_id']

    # Create & kick off threads.
    print 'Doing things and counting them.'
    read_thread = ReadThread(host, port, db, coll_name, conn_options)
    read_thread.start()
    if not options.readonly:
        write_thread = WriteThread(host, port, db, coll_name, conn_options)
        write_thread.start()

        remove_thread = RemoveThread(host, port, db, coll_name, conn_options)
        remove_thread.start()

    last = datetime.now()
    try:
        while True:
            sleep(1)
            now = datetime.now()
            if now - last >= timedelta(seconds=2):
                if options.readonly:
                    print '%6.1f reads/sec' % ReadsPerSec
                else:
                    print '%6.1f reads/sec  %6.1f writes/sec  %6.1f removes/sec' % (
                        ReadsPerSec,
                        WritesPerSec,
                        RemovesPerSec)
                last = now
    except KeyboardInterrupt:
        print
    finally:
        print 'Stopping things.'
        ShouldExit = True

        read_thread.join()
        if not options.readonly:
            write_thread.join()
            remove_thread.join()

        if not options.readonly:
            print 'Cleaning up.'
            coll.remove({'a_key': Record['a_key']}, multi=True)


if __name__ == '__main__':
    main()
