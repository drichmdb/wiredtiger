# Types of eviction

(This is pulled directly from the website docs. I've made no attempts to change it)

## Clean eviction

In the case of clean eviction, there is no dirty content on the page and it is directly removed from the memory. After that, the page remains solely on the disk unchanged.


## History store eviction

For history store eviction, the page is modified but the changes are all committed. Therefore, the page should be clean and can be removed from memory after the dirty changes are written to the new disk image, which is the data format WiredTiger stores on disk. The process that builds the disk image is called reconciliation. In reconciliation, WiredTiger writes the newest committed value of each key to the disk image. All the older values of the key are moved to the history store table. In the end, reconciliation flushes the new disk image to the data file on the disk and later eviction removes the old page from memory.

The history store table is the internal table for WiredTiger to store historical values for all the user-created tables. Each entry in the history store represents a historical value of a key in the user-created tables. The pages in the history store table need to be evicted and loaded back to memory as well. Different from the user-created tables, all the content on the history store page is committed as the historical values it stores must all be committed, and each entry only has one value and a deletion marking the expiration of the entry on its update chain. Therefore, history store pages should always be clean after reconciliation and can be evicted from memory. Besides, only having one committed value for each key in the history store prevents eviction from moving content already in the history store recursively to the history store when evicting a history store page because we only move the values after the first committed value to the history store.

## Update restore eviction

Update restore eviction largely overlaps with history store eviction. The difference is that there are uncommitted changes on the page and WiredTiger cannot write them to the disk image. Instead, besides writing the disk image to the disk, eviction also keeps the disk image in memory and restores the uncommitted changes to the new disk image.

Since usually there is only a subset of keys that have uncommitted changes, eviction only needs to restore those keys that are dirty after reconciliation. Apart from that, eviction may split a page into multiple pages and some of them may be clean after reconciliation even though the old page before split has uncommitted changes. Therefore, WiredTiger tracks whether it needs to do update restore eviction at the key level and the page level.

At the key level, the key needs to be restored if it has uncommitted updates. For each new page, restoration is required if any of the keys on the new page needs to be restored. Once eviction decides to restore a page, it will make a copy of the disk image in memory during reconciliation.

For the restored keys, eviction also frees all the values that have been written to the disk image or moved to the history store as there is no need to duplicate them on the update chains. For the clean keys that are not restored, their whole update chains are removed from memory along with the old disk image.

## Exceptions

Eviction works differently for in-memory databases. Since they don't support the history store, eviction only discards the update chains when all the values on the update chains are globally visible. For the same reason, we cannot free the updates older than the update written to the disk image. Because there is no disk storage for in-memory database, eviction has to keep all the reconciled disk images in memory. 