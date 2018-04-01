# jullop

This is a side project to implement a **key-value datastore** in c with **no synchronization** or locks. The idea is that there is exactly one thread per core and each has a dedicated region of memory that they are responsible for. This requires that there is absolutely no blocking operations and **epoll** is used to handle most of that.
