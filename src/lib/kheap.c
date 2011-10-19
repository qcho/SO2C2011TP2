#include <lib/kheap.h>

#define ALIGN 		1
#define NOT_ALIGN	!ALIGN
#define NO_PHYS		0

// proviene de link.ld
extern int end;
extern page_directory_t* kernel_directory;

int nf_address = (int) &end; // ( Next Free Address )
heap_t *kheap=0;


// funcion principal de alocacion de memoria 
void* our_malloc ( int size, int align, u32int *phys )
{
    page_t *page;
    if (kheap != 0) {
        void *addr = alloc(size, (int)align, kheap);
        if (phys != 0) {
            page = (page_t*)get_page((int)addr, (int)0, kernel_directory);
            *phys = page->frame*0x1000 + (((int)addr) & 0xFFF);
	    // la direccion fisic esta pensada para guardar las direcciones fisicas de las tablas de paginas
	    // registro CR3
        }
        return addr;
    } else {
        if (align == 1 && (nf_address & 0xFFFFF000) ) {
            // Align the placement address;
            nf_address &= 0xFFFFF000;
            nf_address += 0x1000;
        }
        if (phys)
            *phys = nf_address;

	void* tmp = (void*)nf_address;
        nf_address += size * 10;
        return (void*)tmp;
    }

}

void kfree(void* p)
{
    free((void*)p, kheap);
}


void* kmalloc( int size ) {
  return our_malloc( size, NOT_ALIGN, NO_PHYS );
}

void* kmalloc_a( int size ) {
  return our_malloc( size, PAGE_ALIGN, NO_PHYS );
}

void* kmalloc_p( int size, u32int* phys ) {
  return our_malloc( size, NOT_ALIGN, phys );
}

void* kmalloc_ap( int size, u32int* phys ) {
  return our_malloc( size, PAGE_ALIGN, phys );
}

static void expand(int new_size, heap_t *heap) {
    // Sanity check.
    ASSERT(new_size > heap->end_address - heap->start_address);

    // Get the nearest following page boundary.
    if ((new_size & 0xFFFFF000) != 0) {
        new_size &= 0xFFFFF000;
        new_size += 0x1000;
    }

    // Make sure we are not overreaching ourselves.
    ASSERT(heap->start_address+new_size <= heap->max_address);

    // This should always be on a page boundary.
    int old_size = heap->end_address-heap->start_address;

    int i = old_size;
    while (i < new_size) {
        alloc_frame( get_page(heap->start_address+i, 1, kernel_directory),
                     (heap->supervisor)?1:0, (heap->readonly)?0:1);
        i += 0x1000 /* page size */;
    }
    heap->end_address = heap->start_address+new_size;
}

static int contract(int new_size, heap_t *heap) {
    // Sanity check.
    ASSERT(new_size < heap->end_address-heap->start_address);

    // Get the nearest following page boundary.
    if (new_size&0x1000) {
        new_size &= 0x1000;
        new_size += 0x1000;
    }

    // Don't contract too far!
    if (new_size < HEAP_MIN_SIZE)
        new_size = HEAP_MIN_SIZE;

    int old_size = heap->end_address-heap->start_address;
    int i = old_size - 0x1000;
    while (new_size < i) {
        free_frame(get_page(heap->start_address+i, 0, kernel_directory));
        i -= 0x1000;
    }

    heap->end_address = heap->start_address + new_size;
    return new_size;
}

static int find_smallest_hole(int size, int page_align, heap_t *heap) {
    // Find the smallest hole that will fit.
    int iterator = 0;
    while (iterator < heap->index.size) {
        header_t *header = (header_t *)lookup_ordered_array(iterator, &heap->index);
        // If the user has requested the memory be page-aligned
        if (page_align > 0) {
            // Page-align the starting point of this header.
            int location = (int)header;
            int offset = 0;
            if (((location+sizeof(header_t)) & 0xFFFFF000) != 0)
                offset = 0x1000 /* page size */  - (location+sizeof(header_t))%0x1000;
            int hole_size = (int)header->size - offset;
            // Can we fit now?
            if (hole_size >= (int)size)
                break;
        }
        else if (header->size >= size)
            break;
        iterator++;
    }
    // Why did the loop exit?
    if (iterator == heap->index.size)
        return -1; // We got to the end and didn't find anything.
    else
        return iterator;
}

static int header_t_less_than(void*a, void *b) {
    return (((header_t*)a)->size < ((header_t*)b)->size)?1:0;
}

heap_t *create_heap(int start, int end_addr, int max, int supervisor, int readonly) {
    heap_t *heap = (heap_t*)kmalloc(sizeof(heap_t));

    // All our assumptions are made on startAddress and endAddress being page-aligned.
    ASSERT(start%0x1000 == 0);
    ASSERT(end_addr%0x1000 == 0);
    
    // Initialise the index.
    heap->index = place_ordered_array( (void*)start, HEAP_INDEX_SIZE, (lessthan_predicate_t) &header_t_less_than);
    
    // Shift the start address forward to resemble where we can start putting data.
    start += sizeof(type_t)*HEAP_INDEX_SIZE;

    // Make sure the start address is page-aligned.
    if ((start & 0xFFFFF000) != 0) {
        start &= 0xFFFFF000;
        start += 0x1000;
    }
    // Write the start, end and max addresses into the heap structure.
    heap->start_address = start;
    heap->end_address = end_addr;
    heap->max_address = max;
    heap->supervisor = supervisor;
    heap->readonly = readonly;

    // We start off with one large hole in the index.
    header_t *hole = (header_t *)start;
    hole->size = end_addr-start;
    hole->magic = HEAP_MAGIC;
    hole->is_hole = 1;
    insert_ordered_array((void*)hole, &heap->index);     

    return heap;
}




void *alloc(int size, int page_align, heap_t *heap) {

    // Make sure we take the size of header/footer into account.
    int new_size = size + sizeof(header_t) + sizeof(footer_t);
    // Find the smallest hole that will fit.
    int iterator = find_smallest_hole(new_size, page_align, heap);

    if (iterator == -1) { // If we didn't find a suitable hole
        // Save some previous data.
        int old_length = heap->end_address - heap->start_address;
        int old_end_address = heap->end_address;

        // We need to allocate some more space.
        expand(old_length+new_size, heap);
        int new_length = heap->end_address-heap->start_address;

        // Find the endmost header. (Not endmost in size, but in location).
        iterator = 0;
        // Vars to hold the index of, and value of, the endmost header found so far.
        int idx = -1; int value = 0x0;
        while (iterator < heap->index.size) {
            int tmp = (int)lookup_ordered_array(iterator, &heap->index);
            if (tmp > value) {
                value = tmp;
                idx = iterator;
            }
            iterator++;
        }

        // If we didn't find ANY headers, we need to add one.
        if (idx == -1) {
            header_t *header = (header_t *)old_end_address;
            header->magic = HEAP_MAGIC;
            header->size = new_length - old_length;
            header->is_hole = 1;
            footer_t *footer = (footer_t *) (old_end_address + header->size - sizeof(footer_t));
            footer->magic = HEAP_MAGIC;
            footer->header = header;
            insert_ordered_array((void*)header, &heap->index);
        } else {
            // The last header needs adjusting.
            header_t *header = lookup_ordered_array(idx, &heap->index);
            header->size += new_length - old_length;
            // Rewrite the footer.
            footer_t *footer = (footer_t *) ( (int)header + header->size - sizeof(footer_t) );
            footer->header = header;
            footer->magic = HEAP_MAGIC;
        }
        // We now have enough space. Recurse, and call the function again.
        return alloc(size, page_align, heap);
    }

    header_t *orig_hole_header = (header_t *)lookup_ordered_array(iterator, &heap->index);
    int orig_hole_pos = (int)orig_hole_header;
    int orig_hole_size = orig_hole_header->size;
    // Here we work out if we should split the hole we found into two parts.
    // Is the original hole size - requested hole size less than the overhead for adding a new hole?
    if (orig_hole_size-new_size < sizeof(header_t)+sizeof(footer_t)) {
        // Then just increase the requested size to the size of the hole we found.
        size += orig_hole_size-new_size;
        new_size = orig_hole_size;
    }

    // If we need to page-align the data, do it now and make a new hole in front of our block.
    if (page_align && orig_hole_pos&0xFFFFF000) {
        int new_location   = orig_hole_pos + 0x1000 /* page size */ - (orig_hole_pos&0xFFF) - sizeof(header_t);
        header_t *hole_header = (header_t *)orig_hole_pos;
        hole_header->size     = 0x1000 /* page size */ - (orig_hole_pos&0xFFF) - sizeof(header_t);
        hole_header->magic    = HEAP_MAGIC;
        hole_header->is_hole  = 1;
        footer_t *hole_footer = (footer_t *) ( (int)new_location - sizeof(footer_t) );
        hole_footer->magic    = HEAP_MAGIC;
        hole_footer->header   = hole_header;
        orig_hole_pos         = new_location;
        orig_hole_size        = orig_hole_size - hole_header->size;
    } else {
        // Else we don't need this hole any more, delete it from the index.
        remove_ordered_array(iterator, &heap->index);
    }

    // Overwrite the original header...
    header_t *block_header  = (header_t *)orig_hole_pos;
    block_header->magic     = HEAP_MAGIC;
    block_header->is_hole   = 0;
    block_header->size      = new_size;
    // ...And the footer
    footer_t *block_footer  = (footer_t *) (orig_hole_pos + sizeof(header_t) + size);
    block_footer->magic     = HEAP_MAGIC;
    block_footer->header    = block_header;

    // We may need to write a new hole after the allocated block.
    // We do this only if the new hole would have positive size...
    if (orig_hole_size - new_size > 0) {
        header_t *hole_header = (header_t *) (orig_hole_pos + sizeof(header_t) + size + sizeof(footer_t));
        hole_header->magic    = HEAP_MAGIC;
        hole_header->is_hole  = 1;
        hole_header->size     = orig_hole_size - new_size;
        footer_t *hole_footer = (footer_t *) ( (int)hole_header + orig_hole_size - new_size - sizeof(footer_t) );
        if ((int)hole_footer < heap->end_address) {
            hole_footer->magic = HEAP_MAGIC;
            hole_footer->header = hole_header;
        }
        // Put the new hole in the index;
        insert_ordered_array((void*)hole_header, &heap->index);
    }
    
    // ...And we're done!
    return (void *) ( (int)block_header+sizeof(header_t) );
}




void free(void *p, heap_t *heap) {
    // Exit gracefully for null pointers.
    if (p == 0)
        return;

    // Get the header and footer associated with this pointer.
    header_t *header = (header_t*) ( (int)p - sizeof(header_t) );
    footer_t *footer = (footer_t*) ( (int)header + header->size - sizeof(footer_t) );

    // Sanity checks.
    ASSERT(header->magic == HEAP_MAGIC);
    ASSERT(footer->magic == HEAP_MAGIC);

    // Make us a hole.
    header->is_hole = 1;

    // Do we want to add this header into the 'free holes' index?
    char do_add = 1;

    // Unify left
    // If the thing immediately to the left of us is a footer...
    footer_t *test_footer = (footer_t*) ( (int)header - sizeof(footer_t) );
    if (test_footer->magic == HEAP_MAGIC &&
        test_footer->header->is_hole == 1) {
        int cache_size = header->size; // Cache our current size.
        header = test_footer->header;     // Rewrite our header with the new one.
        footer->header = header;          // Rewrite our footer to point to the new header.
        header->size += cache_size;       // Change the size.
        do_add = 0;                       // Since this header is already in the index, we don't want to add it again.
    }

    // Unify right
    // If the thing immediately to the right of us is a header...
    header_t *test_header = (header_t*) ( (int)footer + sizeof(footer_t) );
    if (test_header->magic == HEAP_MAGIC &&
        test_header->is_hole) {
        header->size += test_header->size; // Increase our size.
        test_footer = (footer_t*) ( (int)test_header + // Rewrite it's footer to point to our header.
                                    test_header->size - sizeof(footer_t) );
        footer = test_footer;
        // Find and remove this header from the index.
        int iterator = 0;
        while ( (iterator < heap->index.size) &&
                (lookup_ordered_array(iterator, &heap->index) != (void*)test_header) )
            iterator++;

        // Make sure we actually found the item.
        ASSERT(iterator < heap->index.size);
        // Remove it.
        remove_ordered_array(iterator, &heap->index);
    }

    // If the footer location is the end address, we can contract.
    if ( (int)footer+sizeof(footer_t) == heap->end_address) {
        int old_length = heap->end_address-heap->start_address;
        int new_length = contract( (int)header - heap->start_address, heap);
        // Check how big we will be after resizing.
        if (header->size - (old_length-new_length) > 0) {
            // We will still exist, so resize us.
            header->size -= old_length-new_length;
            footer = (footer_t*) ( (int)header + header->size - sizeof(footer_t) );
            footer->magic = HEAP_MAGIC;
            footer->header = header;
        } else {
            // We will no longer exist :(. Remove us from the index.
            int iterator = 0;
            while ( (iterator < heap->index.size) &&
                    (lookup_ordered_array(iterator, &heap->index) != (void*)test_header) )
                iterator++;
            // If we didn't find ourselves, we have nothing to remove.
            if (iterator < heap->index.size)
                remove_ordered_array(iterator, &heap->index);
        }
    }

    // If required, add us to the index.
    if (do_add == 1)
        insert_ordered_array((void*)header, &heap->index);
}

