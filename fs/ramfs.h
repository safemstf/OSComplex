#ifndef RAMFS_H
#define RAMFS_H


#include "../fs/vfs.h"


/* Initialize RAM filesystem and mount it as root */
void ramfs_init(void);


#endif /* RAMFS_H */