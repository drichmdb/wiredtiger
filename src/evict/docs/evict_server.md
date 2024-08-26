# Evict server

The evict servers walks the BTree to find pages that need eviction and adds the to the [Evict queue](evict_queue.md) for removal by worker threads.

## Walk policy

(Add notes here about the walk policy, soft positions, etc)