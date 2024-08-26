# Eviction

(NOTE: The readme text is inentionally sparse and only exists to show potential content for these docs.)

> [!WARNING]  
> This section calls out high importance notes for a developer. A non-eviction example would be "Checkpoint blocks other threads and induces customer visible latency on other operations. Please take caution before triggering one."

## Overview

All the operations performed in WiredTiger are on the data read into pre-configured amount of memory. WiredTiger uses memory as a cache for all the data on the disk and the data in memory forms the current working set.

Since memory is limited and can't hold all the data stored on the disk, WiredTiger has to continuously move data that is currently not being accessed out of the memory to free up enough space to read data that are requested by the user but currently reside on the disk back into memory. This process is called eviction.

Additional [user facing docs][] are available on the WiredTiger website.

## Features

WiredTiger eviction runs in the background with one [eviction server](docs/evict_server.md) thread and several [eviction worker](docs/evict_worker.md) threads.

The eviction server thread walks the btrees and adds the least recently used pages to the eviction queues until the queues are full. 
The eviction workers continuously remove pages from the [eviction queue](docs/evict_queue.md) and try to evict them.

Eviction has to obtain exclusive access to the page as there may be application threads still reading content on the page that is being evicted.

> [!NOTE]
> (This section) can be populated from our work in phase 1.1

## Concepts

Some examples of what to add here
- Agressive score
- Cache stuck
- Trigger and target thresholds
- Types of eviction
- Urgent eviction

## Interactions with other components

**Checkpoint**
- Eviction runs in parallel with checkpoint, but needs to make sure it doesn't evict a page checkpoint is working on ... Discussion about locks, eviction working bottom up and checkpoint top down, hazard pointers, etc.

**Transactions**
- When trigger thresholds are exceeded eviction will co-opt application threads and use them for eviction until cache levels are returned to acceptable levels. This induces customer visible latency on operations.


[User facing docs]: https://source.wiredtiger.com/develop/arch-eviction.html

