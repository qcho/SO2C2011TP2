#include <fs/diskCache.h>

#define MIN(x,y)	((x) < (y) ? (x) : (y))

PRIVATE int _cacheIndex(int disk, short sector);
PRIVATE int _nextFreeIndex();
PRIVATE void _cachSector(int index, int disk, short sector);
PRIVATE void _flushCache(int index);

static cachedSector cachedData[CACHE_SIZE];

void diskCache_init() {
	log(L_DEBUG, "Initializing disk cache....");
	int i;
	for (i = 0; i < CACHE_SIZE; i++) {
		cachedData[i].accessCount = -1;
	}
}

void diskCache_read(int disk, void* msg, int bytes, unsigned short sector, int offset) {
	ata_normalize(&sector, &offset);
	int index = _cacheIndex(disk, sector);
	if (index != -1) {										// sector is cached
		u8int* from = cachedData[index].contents;
		if (offset + bytes > SECTOR_SIZE) {
			int bytesFromFisrtSector = SECTOR_SIZE - offset - 1;
			memcpy(msg, cachedData[index].contents + offset, bytesFromFisrtSector);
			diskCache_read(cachedData[index].disk, msg + bytesFromFisrtSector,
					bytes - bytesFromFisrtSector, sector + 1, 0);
			return;
		}
		memcpy(msg, from + offset, bytes);
		cachedData[index].accessCount++;
		return;
	}
		log(L_DEBUG, "sector is NOT cached! [%d, %d]", sector, offset);
	int nextFree = _nextFreeIndex();
	_cachSector(nextFree, disk, sector);
	memcpy(msg, cachedData[nextFree].contents + offset, bytes);
}

void diskCache_write(int disk, void* msg, int bytes, unsigned short sector, int offset) {
	/*	//log(L_DEBUG, "WRITE cache -> %d, %d", sector, offset);
	ata_normalize(&sector, &offset);
		//log(L_DEBUG, "\tnormalized-> %d, %d", sector, offset);
	int cluster;
	int index = _cacheIndex(disk, sector, &cluster);
	if (index != -1) {
			log(L_DEBUG, "write - CACHED SECTOR!! [%d, %d]");
		if (cluster == 4 && bytes > 512) {			// Flush all cluster and then write contents
			_flushCache(index);
			ata_write(disk, msg, bytes, sector, offset);
			return;
		}
		char* from = (char*) cachedData[index].contents[cluster];
		memcpy(from + offset, msg, bytes);
		cachedData[index].accessCount++;
		return;
	}
	log(L_DEBUG, "write sector is NOT cached");
	_cachSector(_nextFreeIndex(), disk, sector);*/
	ata_normalize(&sector, &offset);
	int index = _cacheIndex(disk, sector);
	if (index != -1) {
		_flushCache(index);
	}
	ata_write(disk, msg, bytes, sector, offset);
}

/*
 * Busca el indice del array en el que se encuentra cacheado el sector "sector" del disco "disk"
 */
PRIVATE int _cacheIndex(int disk, short sector) {
		log(L_DEBUG, "searching for disk: %d, sector: %d", disk, sector);
	int i;
	for (i = 0; i < CACHE_SIZE; i++) {
			log(L_DEBUG, "\t%d -> accessCount : %d, cachedSector: %d, disk: %d", i, cachedData[i].accessCount, cachedData[i].sector, cachedData[i].disk);
		if (cachedData[i].accessCount != -1 &&
			cachedData[i].sector == sector &&
			cachedData[i].disk == disk) {
			return i;
		}
	}
	log(L_DEBUG, "not found...");
	return -1;
}

/*
 * Busca la siguiente posicion del array para guardar nuevos contenidos
 */
PRIVATE int _nextFreeIndex() {
	int i, nextTofree = -1;
	for(i = 0; i < CACHE_SIZE; i++) {
		if (cachedData[i].accessCount == -1) {
			return i;
		} else if (nextTofree == -1 || cachedData[i].accessCount < cachedData[nextTofree].accessCount) {
			nextTofree = i;
		}
	}
	return nextTofree;
}
/*
 * Cachea el sector "sector" del diso "disk" en el indice "index"
 */
PRIVATE void _cachSector(int index, int disk, short sector) {
	cachedData[index].disk = disk;
	cachedData[index].sector = sector;
	cachedData[index].accessCount = 1;
	cachedData[index].dirty = false;
	ata_read(disk, cachedData[index].contents, SECTOR_SIZE, sector, 0);
	log(L_DEBUG, "Catching %d -> [%d, %d]", index, sector, 0);
}

PRIVATE void _flushCache(int index) {
	if (index != -1) {
		log(L_DEBUG, "flushing sector %d", cachedData[index].sector);
		ata_write(cachedData[index].disk, cachedData[index].contents, SECTOR_SIZE, cachedData[index].sector, 0);
		cachedData[index].accessCount = -1;
	}
}
