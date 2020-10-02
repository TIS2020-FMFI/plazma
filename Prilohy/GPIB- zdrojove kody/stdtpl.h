//****************************************************************************
//*                                                                          *
//*  STDTPL.H: Standard utility templates                                    *
//*                                                                          *
//*  jmiles@pop.net                                                          *
//*                                                                          *
//****************************************************************************

#ifndef STDTPL_H
#define STDTPL_H

#include <assert.h>

#include <stdlib.h>
#include <minmax.h>

//
// CQueue
// LList
// CList
// HashPool
// Pool
// List
// DynArray
// DynAllocArray
// LIFO
// FastList
//

//
// Simple iteration macro for LLists and CLists
//
// ITERATE creates a while() loop which is executed once for every item in
// list, without using an index variable
//
//   lst = name of list variable
//   var = name of variable to receive each list entry in succession
//

#define ITERATE(lst,var) (var)=NULL; \
                         while (((var) = (lst).next(var)) != NULL)

//
// FOR_ITERATE creates a for() loop which is executed once for every item in 
// list
//
//   lst = name of list variable
//   idx = name of index variable (such as "i")
//   var = name of variable to receive each list entry in succession
//

#define FOR_ITERATE(lst,idx,var) for (idx = 0, (var) = (lst).first(); \
                                      idx < (lst).count();            \
                                    ++idx, (var) = (lst).next((var)))

//
// Simple wrapper to store C strings, e.g. in a DynArray<>:
//
//    DynArray<CSTR> strings;
//    strings.add(CSTR("some text"));
//
//    for (i=0; i <= strings.max_index(); i++) 
//       printf("%s\n",strings[i].text);
//

struct CSTR       
{
   C8 *text;

   CSTR(C8 *init=NULL)
      {
      if (init != NULL)
         text = _strdup(init);
      else
         text = NULL;
      }

   CSTR(CSTR const &copy)
      {
      text = _strdup(copy.text);
      }

   CSTR & operator= (CSTR const &copy)
      {
      if (this != &copy)
         {
         if (text != NULL) free(text);

         if (copy.text == NULL)
            text = NULL;
         else
            text = _strdup(copy.text);
         }

      return *this;
      }

   ~CSTR()
      {
      if (text != NULL)
         {
         free(text);
         text = NULL;
         }
      }
};

//****************************************************************************
//
// CQueue
//
// Template used to represent circular queue
//
// Use CQueue when:
//
//    - You want to maintain a circular buffer of arbitrary size
//
//    - You want to insulate memory- or message-based communication 
//      between threads with a critsec-protected mailbox layer
//
//    - You want to retrieve inserted entries in FIFO (first-in, first-out)
//      order
//
// The # of slots in the queue available for data insertion equals size-1
//
// For compilation speed, we only declare this template if WINDOWS.H has
// already been included by someone else
//
//****************************************************************************

#ifdef _INC_WINDOWS

template <typename T, S32 size=256> class CQueue
{
   T                queue[size];
   S32              tail;
   S32              head;
   CRITICAL_SECTION cs;

public:

   //
   // Constructor and destructor
   //

   CQueue()
      {
      InitializeCriticalSection(&cs);
      clear();
      }

  ~CQueue()
      {
      DeleteCriticalSection(&cs);
      }

   //
   // Reset queue to empty state by abandoning contents
   //

   void clear(void)
      {
      EnterCriticalSection(&cs);

      tail = 0;
      head = 0;

      LeaveCriticalSection(&cs);
      }

   //
   // Return # of free entry slots in queue
   //
   // This value is guaranteed to remain conservatively valid until more 
   // data is added
   //

   S32 space_available(void)
      {
      EnterCriticalSection(&cs);

      S32 result;

      if (tail > head)
         {
         result = (tail - head) - 1;
         }
      else
         {
         result = (size - (head - tail)) - 1;
         }

      LeaveCriticalSection(&cs);
      return result;
      }

   //
   // Return # of occupied entry slots in queue
   //
   // This value is guaranteed to remain conservatively valid until data
   // is fetched
   // 

   S32 data_available(void)
      {
      EnterCriticalSection(&cs);

      S32 result;

      if (head >= tail)
         {
         result = head - tail;
         }
      else
         {
         result = size - (tail - head);
         }

      LeaveCriticalSection(&cs);

      return result;
      }

   //
   // Fetch data from queue
   //
   // IMPORTANT: Does not verify the requested amount of data is actually
   // in the queue -- always check data_available() first!
   //

   void pop(T   *dest,
            S32  amount)
      {
      EnterCriticalSection(&cs);

      if ((tail + amount) >= size)
         {
         memcpy(dest,
               &queue[tail],
                (size - tail) * sizeof(T));
       
         dest   += (size - tail);
         amount -= (size - tail);

         tail = 0;
         }

      if (amount)
         {
         memcpy(dest,
               &queue[tail],
                amount * sizeof(T));

         tail += amount;
         }

      LeaveCriticalSection(&cs);
      }

   T &pop(void)
      {
      T &result;

      pop(&result, 1);

      return result;
      }

   //
   // Put data into queue
   //
   // IMPORTANT: Does not verify the necessary amount of space is actually
   // available in the queue -- always check space_available() first!
   //

   void push(T  *src, 
             S32 amount)
      {
      EnterCriticalSection(&cs);

      if ((head + amount) >= size)
         {
         memcpy(&queue[head],
                 src,
                 (size - head) * sizeof(T));

         src    += (size - head);
         amount -= (size - head);

         head = 0;
         }
   
      if (amount)
         {
         memcpy(&queue[head],
                 src,
                 amount * sizeof(T));

         head += amount;
         }

      LeaveCriticalSection(&cs);
      }

   void push(T src)
      {
      push(&src, 1);
      }

   CQueue(CQueue<T, size> & src)
      {
      assert(0); // TBD
      }

   CQueue<T, size> & operator= (CQueue<T, size> &src)
      {
      assert(0); // TBD
      return *this;
      }
};

template <typename T, S32 size=256> class FastQueue      // unprotected version for single-threaded use
{
   T   queue[size];
   S32 tail;
   S32 head;

public:

   //
   // Constructor and destructor
   //

   FastQueue()
      {
      clear();
      }

  ~FastQueue()
      {
      }

   //
   // Reset queue to empty state by abandoning contents
   //

   void clear(void)
      {
      tail = 0;
      head = 0;
      }

   //
   // Return # of free entry slots in queue
   //
   // This value is guaranteed to remain conservatively valid until more 
   // data is added
   //

   S32 space_available(void)
      {
      S32 result;

      if (tail > head)
         {
         result = (tail - head) - 1;
         }
      else
         {
         result = (size - (head - tail)) - 1;
         }

      return result;
      }

   //
   // Return # of occupied entry slots in queue
   //
   // This value is guaranteed to remain conservatively valid until data
   // is fetched
   // 

   S32 data_available(void)
      {
      S32 result;

      if (head >= tail)
         {
         result = head - tail;
         }
      else
         {
         result = size - (tail - head);
         }

      return result;
      }

   //
   // Fetch data from queue
   //
   // IMPORTANT: Does not verify the requested amount of data is actually
   // in the queue -- always check data_available() first!
   //

   void pop(T   *dest,
            S32  amount)
      {
      if ((tail + amount) >= size)
         {
         memcpy(dest,
               &queue[tail],
                (size - tail) * sizeof(T));
       
         dest   += (size - tail);
         amount -= (size - tail);

         tail = 0;
         }

      if (amount)
         {
         memcpy(dest,
               &queue[tail],
                amount * sizeof(T));

         tail += amount;
         }
      }

   T &pop(void)
      {
      T &result;

      pop(&result, 1);

      return result;
      }

   //
   // Put data into queue
   //
   // IMPORTANT: Does not verify the necessary amount of space is actually
   // available in the queue -- always check space_available() first!
   //

   void push(T  *src, 
             S32 amount)
      {
      if ((head + amount) >= size)
         {
         memcpy(&queue[head],
                 src,
                 (size - head) * sizeof(T));

         src    += (size - head);
         amount -= (size - head);

         head = 0;
         }
   
      if (amount)
         {
         memcpy(&queue[head],
                 src,
                 amount * sizeof(T));

         head += amount;
         }
      }

   void push(T src)
      {
      push(&src, 1);
      }

   FastQueue(FastQueue<T, size> & src)
      {
      assert(0); // TBD
      }

   FastQueue<T, size> & operator= (FastQueue<T, size> &src)
      {
      assert(0); // TBD
      return *this;
      }
};

#endif

//****************************************************************************
//
// LList
//
// Template used to represent doubly-linked list
//
// Use LList when:
//
//    - You want to maintain a linked list of arbitrary size
//
//    - You don't mind incurring heap-function overhead when allocating
//      and deleting entries         
//
//    - Fast random-access searchability is not important
//
//    - You want linear, not circular, list traversal (traversal stops when
//      end of list reached)
//
//    - The application is responsible for the contents' lifetime
//
//****************************************************************************

template <class T> class LList
{
   T  *first_entry;
   T  *last_entry;
   S32 cnt;

public:

   //
   // Constructor initializes empty list
   //

   LList()
      {
      reset();
      }

   //
   // Reset list to empty state by abandoning contents
   //

   void reset(void)
      {
      first_entry = NULL;
      last_entry  = NULL;
      cnt         = 0;
      }

   //
   // Return entry count
   //

   S32 count(void) const
      {
      return cnt;
      }

   //
   // Return first list entry (NULL if list empty)
   //

   T *first(void) const
      {
      return first_entry;
      }

   //
   // Return last list entry (NULL if list empty)
   //

   T *last(void) const
      {
      return last_entry;
      }

   //
   // Return next list entry (NULL if end of list reached)
   //
   // Return first list entry if current==NULL
   //

   T *next(T *current) const
      {
      if (current == NULL)
         {
         return first();
         }

      return current->next;
      }

   //
   // Return previous list entry (NULL if beginning of list reached)
   //
   // Return last list entry if current==NULL
   //

   T *prev(T *current) const
      {
      if (current == NULL)
         {
         return last();
         }

      return current->prev;
      }

   //
   // Link new item into list before specified entry
   // If specified entry==NULL, insert at end of list
   //

   T *link(T *entry, T *next = NULL)
      {
      T *prev;

      if (next == NULL)
         {
         prev = last_entry;
         last_entry = entry;
         }
      else
         {
         prev = (T *) next->prev;
         next->prev = entry;
         }

      if (prev == NULL)
         {
         first_entry = entry;
         }
      else
         {
         prev->next = entry;
         }

      entry->next = next;
      entry->prev = prev;

      ++cnt;

      return entry;
      }

   //
   // Unlink item from list (without destroying it)
   //

   void unlink(T *entry)
      {
      if (entry->prev == NULL)
         {
         first_entry = (T *) entry->next;
         }
      else
         {
         entry->prev->next = entry->next;
         }

      if (entry->next == NULL)
         {
         last_entry = (T *) entry->prev;
         }
      else
         {
         entry->next->prev = entry->prev;
         }

      --cnt;
      }

   //
   // Allocate entry and insert before specified entry
   // If specified entry==NULL, insert at end of list
   //

   T *alloc(T *next = NULL)
      {
      T *entry;

      entry = new T;

      if (entry == NULL)
         {
         return NULL;
         }

      return link(entry, next);
      }

   //
   // Unlink item from list and destroy it
   //

   void free(T *entry)
      {
      unlink(entry);
      delete entry;
      }

   //
   // Unlink and destroy all list items
   //

   void free(void)
      {
      T  *t;
      T  *next;

      t = first_entry;

      while (cnt)
         {
         next = t->next;
         free(t);
         t = next;
         }
      }

   LList(LList<T> & src)
      {
      assert(0); // TBD
      }

   LList<T> & operator= (LList<T> &src)
      {
      assert(0); // TBD
      return *this;
      }
};

//****************************************************************************
//
// CList
//
// Template used to represent circular doubly-linked list with storage
// count
//
// Use CList when:
//
//    - You want to maintain a linked list of arbitrary size
//
//    - You don't mind incurring heap-function overhead when allocating
//      and deleting entries         
//
//    - Fast random-access searchability is not important
//
//    - You want circular, not linear, list traversal (traversal wraps from
//      last to first entry in list)
//
//    - The application is responsible for the contents' lifetime
//
//****************************************************************************

template <class T> class CList
{
   T  *first_entry;
   S32 cnt;

public:

   //
   // Constructor initializes empty list
   //

   CList()
      {
      reset();
      }

   //
   // Reset list to empty state by abandoning contents
   //

   void reset(void)
      {
      first_entry = NULL;
      cnt         = 0;
      }

   //
   // Return entry count
   //

   S32 count(void) const
      {
      return cnt;
      }

   //
   // Return arbitrary "first" list entry (NULL if list empty)
   //

   T *first(void) const
      {
      return first_entry;
      }

   //
   // Return arbitrary "last" list entry (NULL if list empty)
   //

   T *last(void) const
      {
      if (first_entry == NULL)
         {
         return NULL;
         }

      return first_entry->prev;
      }

   //
   // Return next list entry
   //
   // Return "first" list entry if current==NULL
   //

   T *next(T *current) const
      {
      if (current == NULL)
         {
         return first();
         }

      return current->next;
      }

   //
   // Return previous list entry
   //
   // Return "last" list entry if current==NULL
   //

   T *prev(T *current) const
      {
      if (current == NULL)
         {
         return last();
         }

      return current->prev;
      }

   //
   // Link new item into list before specified entry
   // If specified entry==NULL, insert at end of list
   //

   T *link(T *entry, T *next = NULL)
      {
      T *prev;

      if (first_entry == NULL)
         {
         //
         // List is currently empty -- insert first entry
         //

         next = prev = first_entry = entry;
         }
      else
         {
         //
         // Insert subsequent entries into list
         //

         if (next == NULL)
            {
            next = first_entry;
            }

         prev = next->prev;
         }

      //
      // Insert new entry between prev and next
      //

      if (next != NULL)
         {
         next->prev = entry;
         }

      if (prev != NULL)
         {
         prev->next = entry;
         }

      entry->next = next;
      entry->prev = prev;

      ++cnt;

      return entry;
      }

   //
   // Unlink item from list (without destroying it)
   //

   void unlink(T *entry)
      {
      if (entry == first_entry)
         {
         //
         // Are we deleting the only entry in the list?
         // If so, set first_entry to NULL
         //

         if (entry->next == entry)
            {
            first_entry = NULL;
            }
         else
            {
            first_entry = entry->next;
            }
         }

      entry->prev->next = entry->next;
      entry->next->prev = entry->prev;

      --cnt;
      }

   //
   // Allocate entry and insert before specified entry
   // If specified entry==NULL, insert at "end" of list
   //

   T *alloc(T *next = NULL)
      {
      T *entry;

      entry = new T;

      if (entry == NULL)
         {
         return NULL;
         }

      return link(entry, next);
      }

   //
   // Unlink item from list and destroy it
   //

   void free(T *entry)
      {
      unlink(entry);
      delete entry;
      }

   //
   // Unlink and delete all items in list
   //

   void free(void)
      {
      T  *t;
      T  *next;

      t = first_entry;

      while (cnt)
         {
         next = t->next;
         free(t);
         t = next;
         }
      }

   CList(CList<T> & src)
      {
      assert(0); // TBD
      }

   CList<T> & operator= (CList<T> &src)
      {
      assert(0); // TBD
      return *this;
      }
};

//****************************************************************************
//
// HashPool
//
// Template used to maintain a dynamically-expandable list of structures for
// fast allocation and reference
//
// WARNING: HashPool elements may be moved in memory by the allocate() 
// method.  Do not rely on stored pointers to HashPool elements!  Use array
// indices instead for persistent references.
//
// WARNING: Objects stored in a HashPool will not have their destructors called
// until the HashPool object itself goes out of scope.  Unlinking a HashPool
// entry simply moves the entry to a free list.  Any contents, if any, must be
// explicitly cleared by the shutdown() method.
//
// Methods:
//   allocate()
//   unlink()
//   reset()
//   clear()
//   search()
//   cache()
//   display()
//   count()
//   [] by either index or key
//
// HashPool makes use of the following members of class ENTRY_TYPE:
//
//   U32         hash_key;      // For internal use only
//   ENTRY_TYPE *hash_next;   
//   ENTRY_TYPE *hash_prev;  
//   ENTRY_TYPE *next;          
//   ENTRY_TYPE *prev;       
//                           
//   S32         index;         // Index of this entry in linear array
//
//   U32    hash      (void *)  // Derive key from object
//   BOOL32 compare   (void *)  // Compared object with object stored in T
//   void   initialize(void *)  // Store indicated object in T
//   void   shutdown  (void)    // Called when unlinked from pool
//   void   display   (void)    // Optionally display contents of T
//
// ENTRY_TYPE may be thought of as a "package" that allows an object of any
// desired type (string, structure, etc...) to be stored in a dynamically-
// expandable linear array which is indexed by a hash table.  Along with the
// mandatory data and code members above, ENTRY_TYPE should also contain one
// or more user data fields representing the object being stored in ENTRY_TYPE.
// 
// Use HashPool when:
//
//    - You need to insert and remove entries in a list whose capacity needs 
//      to expand at runtime
//
//    - You need most allocations and deletions to be very fast
//
//    - You don't mind storing indexes to pool entries, rather than pointers
// 
//    - Fast random-access searchability is required
//
//    - You will never need to shrink an existing pool's memory allocation
//
// Example usage of HashPool to store a list of ASCII strings for fast 
// access:
//
//       class MESSAGE_TYPE
//       {
//       public:
//          //
//          // Data and methods required by HashPool template
//          //
//       
//          U32           hash_key;  // Hash key for this entry
//       
//          MESSAGE_TYPE *hash_next; // Next/prev pointers in each hash bucket,
//          MESSAGE_TYPE *hash_prev; // organized for access speed
//       
//          MESSAGE_TYPE *next;      // Next/prev pointers in allocation list or  
//          MESSAGE_TYPE *prev;      // free list, depending on entry's status
//       
//          S32           index;     // Index of this entry in linear array,
//                                   // or -1 if not currently allocated
//       
//          //
//          // HashPool hash function to derive 8-bit key from string
//          // by XOR'ing the first two characters together
//          //
//       
//          static inline U32 hash(void *object)
//             {
//             assert(((U8 *) object)[0]);
//             return (U32) (((U8 *) object)[0] ^ ((U8 *) object)[1]);
//             }
//       
//          //
//          // HashPool search comparison function -- returns TRUE if match
//          // found
//          //
//       
//          inline BOOL32 compare(void *object)
//             {
//             return !strcmp(name, (C8 *) object);
//             }
//       
//          //
//          // HashPool initialization function -- called when allocating new 
//          // entry
//          //
//       
//          inline void initialize(void *object)
//             {
//             strcpy(name, (C8 *) object);
//             }
//
//          //
//          // HashPool shutdown function -- called when unlinking entry from
//          // pool
//          //
//
//          inline void shutdown(void)
//             {
//             }
//       
//          //
//          // HashPool diagnostic display function
//          //
//       
//          void display(void)
//             {
//             printf("Entry #%d: [%s]",name, index);
//             }
//       
//          //
//          // User data
//          //
//       
//          C8 name[64];             // 64-character ASCII message name
//       };
// 
//****************************************************************************

template <class T, S32 init_size, S32 hash_size=256> class HashPool
{
   T   *last_alloc;              // Pointer to most-recently-allocated entry
   T   *first_free;              // Pointer to first-available entry
   T   *hash_table[hash_size];   // Hash bucket array for fast searches

public:
   
   T   *list;                    // List of user-specified structures
   S32  list_size;               // # of structures in list

   //
   // Create structure pool
   //

   HashPool(void)
      {
      //
      // Allocate initial list entries
      // 

      list_size = init_size;

      list = new T[list_size];

      reset();
      }

   //
   // Destroy structure pool
   //                         

   void free (void);
   void clear (void);

   ~HashPool(void)
      {
      free();
      }

   //
   // Reset list to empty state by abandoning contents
   //

   void reset(void)
      {
      //
      // Chain all list entries together in 'free' list
      //

      for (S32 i=0; i < list_size; i++)
         {
         list[i].index = -1;
         list[i].prev  = &list[i-1];
         list[i].next  = &list[i+1];
         }

      list[0          ].prev = NULL;
      list[list_size-1].next = NULL;

      last_alloc = NULL;
      first_free = &list[0];

      //
      // Initialize hash table
      //

      for (S32 i=0; i < hash_size; i++)
         {
         hash_table[i] = NULL;
         }
      }

   //
   // Remove entry from free list
   //

   void remove_from_free_list(T *entry)
      {
      if (entry == first_free)
         {
         first_free = (T *) entry->next;
         }
      else
         {
         entry->prev->next = entry->next;

         if (entry->next != NULL)
            {
            entry->next->prev = entry->prev;
            }
         }
      }

   //
   // Add entry to allocated list
   //

   void add_to_alloc_list(T *entry, U32 key)
      {
      //
      // Insert new entry in allocation-order list, from the beginning
      //

      entry->next = last_alloc;
      entry->prev = NULL;

      if (last_alloc != NULL)
         {
         last_alloc->prev = entry;
         }

      last_alloc = entry;

      //
      // Insert entry into hash bucket, from the beginning
      //
      // Associate hash key with entry for ease of reference
      //

      entry->hash_key  = key;
      entry->hash_next = hash_table[key];
      entry->hash_prev = NULL;

      if (hash_table[key] != NULL)
         {
         hash_table[key]->hash_prev = entry;
         }

      hash_table[key] = entry;

      entry->index = (S32) ((UINTa(entry) - UINTa(list)) / sizeof(T));
      }

   //
   // Expand list
   //

   void grow_list(S32 new_size)
      {
      S32 old_size = list_size;

      T *old_list = list;
      T *new_list = new T[new_size];

      if (new_list == NULL)
         {
         //
         // Allocation failed (should not normally happen)
         //

         return;
         }

      UINTa fixup = ((UINTa) new_list) - ((UINTa) old_list);

      //
      // Copy existing entries from old list to new list, adjusting
      // links to new base address
      //

      for (S32 i=0; i < old_size; i++)
         {
         new_list[i] = old_list[i];

         if (new_list[i].hash_next != NULL)
            {
            new_list[i].hash_next = (T *) 
                                    (((size_t) new_list[i].hash_next) + fixup);
            }

         if (new_list[i].hash_prev != NULL)
            {
            new_list[i].hash_prev = (T *) 
                                    (((size_t) new_list[i].hash_prev) + fixup);
            }

         if (new_list[i].next != NULL)
            {
            new_list[i].next = (T *) 
                                 (((size_t) new_list[i].next) + fixup);
            }

         if (new_list[i].prev != NULL)
            {
            new_list[i].prev = (T *) 
                                 (((size_t) new_list[i].prev) + fixup);
            }
         }

      //
      // Fix up hash table
      //

      for (S32 i=0; i < hash_size; i++)
         {
         if (hash_table[i] != NULL)
            {
            hash_table[i] = (T *) (((size_t) hash_table[i]) + fixup);
            }
         }

      //
      // Adjust pool pointers, and chain all newly-appended list entries 
      // together in 'free' list
      //       

      if (last_alloc != NULL)
         {
         last_alloc = (T *) (((UINTa) last_alloc) + fixup);
         }

      if (first_free != NULL)
         {
         first_free = (T *) (((UINTa) first_free) + fixup);
         first_free->prev = &new_list[new_size-1];
         }

      for (S32 i=old_size; i < new_size; i++)
         {
         new_list[i].index = -1;
         new_list[i].prev  = &new_list[i-1];
         new_list[i].next  = &new_list[i+1];
         }

      new_list[new_size-1].next = first_free;
      new_list[old_size].prev   = NULL;

      first_free = &new_list[old_size];

      //
      // Finally, replace old list
      //

      list      = new_list;
      list_size = new_size;

      if (old_list != NULL)
         {
         delete [] old_list;
         }
      }

   //
   // Allocate pool entry to represent *object, growing pool if necessary
   // to accomodate new allocation
   //                                                               

   S32 allocate(const void *object)
      {
      U32 key = T::hash(object);

      //
      // Grow list if necessary
      //

      if (first_free == NULL)
         {
#if 0
         grow_list(list_size + init_size);
#else
         grow_list(list_size * 2);
#endif
         }

      //
      // Get pointer to free entry and move it into the 'allocated' list
      //

      T *entry = first_free;

      remove_from_free_list(entry);
      add_to_alloc_list    (entry,key);

      //
      // Initialize newly-allocated entry with object
      //

      entry->initialize(object);

      //
      // Return entry's position in linear array
      //

      return entry->index;
      }

   //
   // Unlink existing entry from hash list and allocation list
   //

   void unlink(T *entry)
      {
      //
      // Call deallocation routine before unlinking object
      //

      entry->shutdown();

      //
      // Unlink from hash table
      //

      if (entry->hash_next != NULL)
         {
         entry->hash_next->hash_prev = entry->hash_prev;
         }

      if (entry->hash_prev != NULL)
         {
         entry->hash_prev->hash_next = entry->hash_next;
         }

      //
      // Unlink from allocation-order list
      //

      entry->index = -1;

      if (entry->next != NULL)
         {
         entry->next->prev = entry->prev;
         }

      if (entry->prev != NULL)
         {
         entry->prev->next = entry->next;
         }

      //
      // Adjust pool list pointers, if deleting first entry
      //

      if (last_alloc == entry)
         {
         last_alloc = (T *) entry->next;
         }

      if (hash_table[entry->hash_key] == entry)
         {
         hash_table[entry->hash_key] = (T *) entry->hash_next;
         }

      //
      // Finally, add this entry to the beginning of the free list
      //

      entry->next = first_free;
      entry->prev = NULL;

      if (first_free != NULL)
         {
         first_free->prev = entry;
         }

      first_free = entry;
      }

   //
   // Unlink existing entry by index #, if index # is valid and 
   // entry has not already been unlinked
   //

   void unlink(S32 index)
      {
      if ((index != -1) && (list[index].index != -1))
         {
         unlink(&list[index]);
         }
      }

   //
   // Unlink existing entry by search key, if present
   //

   void unlink(const void *search_string)
      {
      unlink(search(search_string));
      }

   //
   // Search pool for entry which represents *object
   //
   // Return entry index, or -1 if object not found
   //

   S32 search(const void *object)
      {
      T *result;

      result = hash_table[T::hash(object)];

      while (result != NULL)
         {
         if (result->compare(object))
            {
            break;
            }

         result = (T *) result->hash_next;
         }

      if (result)
         {
         return result->index;
         }

      return -1;
      }

   //
   // Similar to search(), but automatically allocates an entry on failure
   //

   S32 cache(const void *object)
      {
      S32 i = search(object);

      if (i == -1)
         {
         i = allocate(object);
         }

      return i;
      }

   //
   // Perform diagnostic dump of all objects in list
   //

   void display(void)
      {
      for (S32 i=0; i < list_size; i++)
         {
         if (list[i].index != -1)
            {
            list[i].display();
            }
         }
      }

   //
   // Return current # of entries actually in use
   //

   S32 count(void)
      {
      S32 n = 0;

      for (S32 i=0; i < list_size; i++)
         {
         if (list[i].index != -1)
            {
            ++n;
            }
         }

      return n;
      }

   //
   // Sequential index operator
   //

   T &operator [] (S32 index)
      {
      return list[index];
      }

   S32 operator [] (const void *search_string)
      {
      return search(search_string);
      }

   //
   // Create pool as a copy of another, such that the
   // source pool can be destroyed without harming the copy
   //

   HashPool(HashPool<T, init_size, hash_size> & src)
      {
      //
      // Allocate initial list entries and link them into 'free' list
      // 

      list_size = src.list_size;

      list = new T[list_size];

      reset();

      //
      // Each active entry in old pool must be copied to new pool at the
      // same index
      //

      for (S32 i=0; i < src.list_size; i++)
         {
         if (src.list[i].index != -1)
            {
            remove_from_free_list(&list[i]);

            list[i] = src.list[i];

            add_to_alloc_list(&list[i], src.list[i].hash_key);
            }
         }
      }

   //
   // Assign contents of one array to another
   //

   HashPool<T, init_size, hash_size> & operator= (HashPool<T, init_size, hash_size> &src)
      {
      if (&src != this)
         {
         //
         // Discard current contents of array, adjust list size if
         // necessary, and mark all entries free
         //

         clear();

         if (list_size < src.list_size)
            {
            grow_list(src.list_size);
            }

         reset();

         for (S32 i=0; i < src.list_size; i++)
            {
            if (src.list[i].index != -1)
               {
               remove_from_free_list(&list[i]);

               list[i] = src.list[i];

               add_to_alloc_list(&list[i], src.list[i].hash_key);
               }
            }
         }

      return *this;
      }
};

//
// Warning: free() leaves list in non-working state -- should 
// be called only from destructor
//

template <class T, S32 init_size, S32 hash_size> void HashPool<T, init_size, hash_size>::free (void)
{
   if (list != NULL)
      {
      //
      // Unlink and shut down all objects in list, in
      // their reverse order of allocation
      //

      while (last_alloc != NULL)
         {
         unlink(last_alloc);
         }

      //
      // Free memory used by pool
      //

      delete [] list;
      list = NULL;

      list_size = 0;
      }
}

template <class T, S32 init_size, S32 hash_size> void HashPool<T, init_size, hash_size>::clear (void)
{
   if (list != NULL)
      {
      //
      // Unlink and shut down all objects in list, in
      // their reverse order of allocation
      //

      while (last_alloc != NULL)
         {
         unlink(last_alloc);
         }
      }
}

//
// General-purpose hashed-text container entry
//

struct HASHSTR
{
   U32       hash_key;                                          // For internal use only
   HASHSTR  *hash_next;   
   HASHSTR  *hash_prev;  
   HASHSTR  *next;          
   HASHSTR  *prev;       
                             
   S32 index;                                                   // Index of this entry in linear array

   static U32 hash(const void *V)                                     // Derive hash key from object descriptor
      { 
      U8 *_name = (U8 *) V;

      assert(_name[0]);

      return (U32) (toupper(_name[0]) ^ toupper(_name[1]));
      } 

   bool compare(const void *V)                                     
      { 
      C8 *_name = (C8 *) V;

      return (!_stricmp(name, _name));
      }

   void   initialize(const void *V) 
      {
      assert(V != NULL);

      name = (C8 *) malloc(strlen((C8 *) V) + 1);

      strcpy(name, (C8 *) V);
      }

   void   shutdown  (void)    
      {
      if (name != NULL)
         {
         free(name);
         name = NULL;
         }
      }

   void   display   (void)    
      {
      }                                

   //
   // User vars
   //

   C8 *name;  
   S32 val;
};

struct HASHLIST_CONTENTS                  // Objects stored in HashList<> can inherit from this
{                                         // if desired...
   U32                 hash_key;     
   HASHLIST_CONTENTS  *hash_next;   
   HASHLIST_CONTENTS  *hash_prev;  
   HASHLIST_CONTENTS  *next;          
   HASHLIST_CONTENTS  *prev;       
                             
   S32 allocated;

   static U32 hash(const void *V)   
      { 
      U8 *_name = (U8 *) V;

      if (_name[0] == 0) 
         {
         return 0;
         }

      return (U32) (toupper(_name[0]) ^ toupper(_name[1]));
      } 

   S32 lex_compare(const void *V)         // Lexical comparison, not boolean like HashPool uses!                       
      {                                   // Note that parameter V is a key value, not a key/value object
      C8 *_name = (C8 *) V;               // (i.e., a C8 * rather than a HASHLIST_CONTENTS *)

      return _stricmp(name, _name);       
      }

   S32 sort_compare(HASHLIST_CONTENTS *V) // Comparison function used for qsorting two existing HASHLIST_CONTENTS by 
      {                                   // name
      return _stricmp(name, V->name);
      }

   void initialize(const void *V) 
      {
      assert(V != NULL);

      name = (C8 *) malloc(strlen((C8 *) V) + 1);

      strcpy(name, (C8 *) V);
      }

   void shutdown(void)    
      {
      if (name != NULL)
         {
         free(name);
         name = NULL;
         }
      }

   void display(void)    
      {
      }                                

   C8 *name;  
};

//****************************************************************************
//
// Pool
//
// Template used to maintain a dynamically-expandable list of structures for
// fast allocation and reference
//
// WARNING: Pool elements may be moved in memory by the allocate() 
// method.  Do not rely on stored pointers to Pool elements!  Use array
// indices instead for persistent references.
//
// WARNING: Objects stored in a Pool will not have their destructors called
// until the Pool object itself goes out of scope.  Unlinking a Pool
// entry simply moves the entry to a free list.  Any contents, if any, must be
// explicitly cleared by the shutdown() method.
//
// Methods:
//   allocate()
//   unlink()
//   reset()
//   capacity()
//   display()
//   count()
//   [] by index
//
// Pool makes use of the following members of class ENTRY_TYPE:
//
//   ENTRY_TYPE *next;          // For internal use only
//   ENTRY_TYPE *prev;         
//                             
//   S32         index;         // Index of this entry in linear array
//
//   void   initialize(void *)  // Store indicated object in ENTRY_TYPE
//   void   shutdown  (void)    // Called when unlinked from pool
//   void   display   (void)    // Optionally display contents of ENTRY_TYPE
//
// ENTRY_TYPE may be thought of as a "package" that allows an object of any
// desired type (string, structure, etc...) to be stored in a dynamically-
// expandable linear array.  Along with the mandatory data and code members 
// above, ENTRY_TYPE should also contain one or more user data fields 
// representing the object being stored in ENTRY_TYPE
// 
// Use Pool when:
//
//    - You need to insert and remove entries in a list whose capacity needs 
//      to be determined at runtime
//
//    - You need most allocations and deletions to be very fast
//                                                   
//    - You don't mind storing indexes to pool entries, rather than pointers
//
//    - You will keep track of entry indexes separately, so random-access 
//      searchability is not required (otherwise, use HashPool)
//
//    - You will never need to shrink an existing pool's memory allocation
//
// Example usage of Pool to store an expandable list of ASCII strings:
//
//       class MESSAGE_TYPE
//       {
//       public:
//          //
//          // Data and methods required by Pool template
//          //
//       
//          MESSAGE_TYPE *next;      // Next/prev pointers in allocation list or  
//          MESSAGE_TYPE *prev;      // free list, depending on entry's status
//       
//          S32           index;     // Index of this entry in linear array,
//                                   // or -1 if not currently allocated
//       
//          //
//          // Pool initialization function -- called when allocating new 
//          // entry
//          //
//       
//          inline void initialize(void *object)
//             {
//             strcpy(name, (C8 *) object);
//             }
//
//          //
//          // Pool shutdown function -- called when unlinking entry from
//          // pool
//          //
//
//          inline void shutdown(void)
//             {
//             }
//       
//          //
//          // Pool diagnostic display function
//          //
//       
//          void display(void)
//             {
//             printf("Entry #%d: [%s]",name, index);
//             }
//       
//          //
//          // User data
//          //
//       
//          C8 name[64];             // 64-character ASCII message name
//       };
// 
//****************************************************************************

template <class T, S32 init_size> class Pool
{
   S32  n_alloc;           // # of allocated entries

public:

   T   *last_alloc;        // Pointer to most-recently-allocated entry
   T   *first_free;        // Pointer to first-available entry

   T   *list;              // List of user-specified structures
   S32  list_size;         // # of structures in list

   //
   // Create structure pool
   //

   Pool(void)
      {
      //
      // Allocate initial list entries
      // 

      list_size = init_size;

      list = new T[list_size];

      reset();
      }

   //
   // Destroy structure pool
   //                         

   void clear (void);

   ~Pool(void)
      {
      if (list != NULL)
         {
         //
         // Unlink and shut down all objects in list, in
         // their reverse order of allocation
         //

         clear();

         //
         // Free memory used by pool
         //

         delete [] list;
         }
      }

   //
   // Reset list to empty state by abandoning contents
   //

   void reset(void)
      {
      //
      // Chain all list entries together in 'free' list
      //

      for (S32 i=0; i < list_size; i++)
         {
         list[i].index = -1;
         list[i].prev  = &list[i-1];
         list[i].next  = &list[i+1];
         }

      list[0          ].prev = NULL;
      list[list_size-1].next = NULL;

      last_alloc = NULL;
      first_free = &list[0];

      n_alloc = 0;
      }

   //
   // Remove entry from free list
   //

   void remove_from_free_list(T *entry)
      {
      if (entry == first_free)
         {
         first_free = entry->next;
         }
      else
         {
         entry->prev->next = entry->next;

         if (entry->next != NULL)
            {
            entry->next->prev = entry->prev;
            }
         }
      }

   //
   // Add entry to allocated list
   //

   void add_to_alloc_list(T *entry)
      {
      //
      // Set index member to entry's position in linear array
      //

      entry->index = (S32) ((UINTa(entry) - UINTa(list)) / sizeof(T));

      //
      // Insert new entry in allocation-order list, from the beginning
      //

      entry->next = last_alloc;
      entry->prev = NULL;

      if (last_alloc != NULL)
         {
         last_alloc->prev = entry;
         }

      last_alloc = entry;
      }

   //
   // Expand list
   //

   void grow_list(S32 new_size)
      {
      S32 old_size = list_size;

      T *old_list = list;
      T *new_list = new T[new_size];

      if (new_list == NULL)
         {
         //
         // Allocation failed (should not normally happen)
         //

         return;
         }

      UINTa fixup = ((UINTa) new_list) - ((UINTa) old_list);

      //
      // Copy existing entries from old list to new list, adjusting
      // links to new base address
      //

      for (S32 i=0; i < old_size; i++)
         {
         new_list[i] = old_list[i];

         if (new_list[i].next != NULL)
            {
            new_list[i].next = (T *) 
                                 (((U32) new_list[i].next) + fixup);
            }

         if (new_list[i].prev != NULL)
            {
            new_list[i].prev = (T *) 
                                 (((U32) new_list[i].prev) + fixup);
            }
         }

      //
      // Adjust pool pointers, and chain all newly-appended list entries 
      // together in 'free' list
      //

      if (last_alloc != NULL)
         {
         last_alloc = (T *) (((UINTa) last_alloc) + fixup);
         }

      if (first_free != NULL)
         {
         first_free = (T *) (((UINTa) first_free) + fixup);
         first_free->prev = &new_list[new_size-1];
         }

      for (S32 i=old_size; i < new_size; i++)
         {
         new_list[i].index = -1;
         new_list[i].prev  = &new_list[i-1];
         new_list[i].next  = &new_list[i+1];
         }

      new_list[new_size-1].next = first_free;
      new_list[old_size].prev   = NULL;

      first_free = &new_list[old_size];

      //
      // Finally, replace old list
      //

      list      = new_list;
      list_size = new_size;

      if (old_list != NULL)
         {
         delete [] old_list;
         }
      }

   //
   // Initialize an existing unallocated entry
   //

   S32 initialize(T *entry, const void *object=NULL)
      {
      remove_from_free_list(entry);
      add_to_alloc_list    (entry);

      ++n_alloc;

      S32 result = entry->index;

      entry->initialize(object);

      return result;
      }

   void initialize(S32 index, const void *object=NULL)
      {
      initialize(&list[index], object);
      }

   //
   // Allocate pool entry to represent *object, growing pool if necessary
   // to accomodate new allocation
   //                                                               

   S32 allocate(const void *object=NULL)
      {
      //
      // Grow list if necessary
      //

      if (first_free == NULL)
         {
#if 0
         grow_list(list_size + init_size);
#else
         grow_list(list_size * 2);
#endif
         }

      //
      // Get pointer to free entry and move it into the 'allocated' list,
      // returning entry's position in linear array
      //

      T *entry = first_free;

      return initialize(entry, object);
      }

   //
   // Unlink existing entry from hash list and allocation list
   //

   void unlink(T *entry)
      {
      //
      // Call deallocation routine before unlinking object
      //

      entry->shutdown();

      //
      // Unlink from allocation-order list
      //

      --n_alloc;

      entry->index = -1;

      if (entry->next != NULL)
         {
         entry->next->prev = entry->prev;
         }

      if (entry->prev != NULL)
         {
         entry->prev->next = entry->next;
         }

      //
      // Adjust pool list pointers, if deleting first entry
      //

      if (last_alloc == entry)
         {
         last_alloc = entry->next;
         }

      //
      // Finally, add this entry to the beginning of the free list
      //

      entry->next = first_free;
      entry->prev = NULL;

      if (first_free != NULL)
         {
         first_free->prev = entry;
         }

      first_free = entry;
      }

   //
   // Unlink existing entry by index #, if index # is valid and 
   // entry has not already been unlinked
   //

   void unlink(S32 index)
      {
      if ((index != -1) && (list[index].index != -1))
         {
         unlink(&list[index]);
         }
      }

   //
   // Perform diagnostic dump of all objects in list
   //

   void display(void)
      {
      for (S32 i=0; i < list_size; i++)
         {
         if (list[i].index != -1)
            {
            list[i].display();
            }
         }
      }

   //
   // Return current pool capacity
   //

   S32 capacity(void)
      {
      return list_size;
      }

   //
   // Return current # of entries actually in use
   //

   S32 count(void)
      {
      return n_alloc;
      }

   //
   // Array-indexing operator
   //
   // Grow pool if necessary to ensure that the referenced entry is 
   // accessible
   //
   // WARNING: This operator can return entries in the free list.  Because we can't 
   // tell the difference between read and write accesses here, applications that 
   // allocate new entries by reference may need to call initialize() manually 
   // to finish the allocation process
   //

   T &operator [] (S32 index)
      {
      assert(index >= 0);

      if (index >= list_size)
         {
         grow_list((index == 0) ? 1 : index * 2);
         }

      return list[index];
      }

   //
   // Create pool as a copy of another, such that the
   // source pool can be destroyed without harming the copy
   //

   Pool(Pool<T, init_size> & src)
      {
      //
      // Allocate initial list entries and link them into 'free' list
      // 

      list_size = src.list_size;

      list = new T[list_size];

      reset();

      n_alloc = src.n_alloc;

      //
      // Each active entry in old pool must be copied to new pool at the
      // same index
      //

      for (S32 i=0; i < src.list_size; i++)
         {
         if (src.list[i].index != -1)
            {
            remove_from_free_list(&list[i]);

            list[i] = src.list[i];

            add_to_alloc_list(&list[i]);
            }
         }
      }

   //
   // Assign contents of one array to another
   //

   Pool<T, init_size> & operator= (Pool<T, init_size> &src)
      {
      if (&src != this)
         {
         //
         // Discard current contents of array, adjust list size if
         // necessary, and mark all entries free
         //

         clear();

         if (list_size < src.list_size)
            {
            grow_list(src.list_size);
            }

         reset();

         for (S32 i=0; i < src.list_size; i++)
            {
            if (src.list[i].index != -1)
               {
               remove_from_free_list(&list[i]);

               list[i] = src.list[i];

               add_to_alloc_list(&list[i]);

               ++n_alloc;
               }
            }
         }

      return *this;
      }
};

template <class T, S32 init_size> void Pool<T, init_size>::clear (void)
{
   if (list != NULL)
      {
      //
      // Unlink and shut down all objects in list, in
      // their reverse order of allocation
      //

      while (last_alloc != NULL)
         {
         unlink(last_alloc);
         }

      assert(n_alloc == 0);
      }
}

//****************************************************************************
//
// List
//
// Template used to represent list of object instances for rapid allocation/
// traversal/deletion
//
// Use List when:
//
//    - You need to insert and remove entries in a list without using 
//      potentially-slow heap functions, or incurring heap-entry overhead
//
//    - You don't mind being limited to a fixed number of entries (declared
//      at time of list creation)
//
//    - Fast traversal is important, but random-access searchability is not
//
//****************************************************************************

template <class T, S32 size> class List
{     
   T *alloc_list;        // Pointer to first allocated entry
   T *last_alloc;        // Last-allocated entry
   T *free_list;         // Pointer to first-available entry
   T *list;              // Pool of entries

public:
   
   //
   // Create fixed-size list
   //

   List(void)
      {
      //
      // Allocate list array from heap
      //

      list = new T[size];

      //
      // Link all entries into "free" list
      //

      last_alloc = NULL;
      alloc_list = NULL;
      free_list  = &list[0];

      list[0].prev = NULL;
      list[0].next = &list[1];

      for (S32 i=1; i < (size-1); i++)
         {
         list[i].prev = &list[i-1];
         list[i].next = &list[i+1];
         }

      list[size-1].prev = &list[size-2];
      list[size-1].next = NULL;
      }

   //
   // Destroy list
   //

   ~List()
      {
      delete[] list;
      }

   //
   // Return last allocated list entry (NULL if list empty)
   //

   T *last(void)
      {
      return last_alloc;
      }

   //
   // Return first allocated list entry (NULL if list empty)
   //

   T *first(void)
      {
      return alloc_list;
      }

   //
   // Return nth entry in list (slow -- use Pool or HashPool if you
   // need fast random indexing!)
   //

   T *nth(S32 n)
      {
      T *result = alloc_list;

      while ((result != NULL) && n)
         {
         --n;
         result = result->next;
         }

      return result;
      }

   //
   // Allocate an entry
   //
   // Returns NULL if list already full
   //

   T *allocate(void)
      {
      T *entry = free_list;

      if (entry == NULL)
         {
         return NULL;
         }

      //
      // Unlink first entry from beginning of free list
      // 

      free_list = free_list->next;

      if (free_list != NULL)
         {
         free_list->prev = NULL;
         }

      //
      // Relink as last entry in allocated list
      // 

      if (alloc_list == NULL)
         {
         alloc_list = entry;
         }

      if (last_alloc != NULL)
         {
         last_alloc->next = entry;
         }

      entry->prev = last_alloc;
      entry->next = NULL;

      //
      // Return pointer to entry
      //

      last_alloc = entry;

      return entry;
      }

   //
   // Free an entry (move it from the allocated list to the free list)
   //

   void free(T *entry)
      {
      //
      // If freeing last-allocated entry, back off last_alloc pointer to
      // previous entry
      //

      if (entry == last_alloc)
         {
         last_alloc = entry->prev;
         }

      //
      // Unlink specified entry from allocated list
      //

      if (entry->next != NULL)
         {
         entry->next->prev = entry->prev;
         }

      if (entry->prev == NULL)   
         {
         alloc_list = entry->next;
         }
      else
         {
         entry->prev->next = entry->next;
         }

      //
      // Relink as first entry in free list
      //

      entry->next = free_list;

      if (free_list != NULL)
         {
         free_list->prev = entry;
         }

      free_list = entry;
      }

   T ** pfirst (void)
      {
      return (const T **) &alloc_list;
      }

   List(List<T, size> & src)
      {
      assert(0); // TBD
      }

   List<T, size> & operator= (List<T, size> &src)
      {
      assert(0); // TBD
      return *this;
      }
};

//****************************************************************************
//
// DynArray
//
// Template used to represent automatically-growable array
//
// Use DynArray when:
//
//    - You need a linear array to contain an amount of data which is 
//      not known a priori
//
//    - Fast sequential and random traversal are important, but out-of-order
//      insertability is not needed
//
//    - You don't mind (potentially) incurring C heap overhead when 
//      allocating entries
//
//    - You don't mind storing indexes to array entries, rather than pointers
//
//    - You want to store any type without using a wrapper class
//
//    - You want to be able to shrink the array's storage allocation as well as
//      grow it
//
// DynArray has copy assignment/constructors, so it can be safely placed 
// inside objects stored in a Pool or HashPool
// 
//****************************************************************************

template <typename T, S32 init_size=16> class DynArray
{
   T  *array;
   S32 n_entries;
   S32 highest_referenced_index;

public:

    // Remove the index by swapping it with the last entry and decrementing the total count.
    void RemoveSwap(S32 i_Index)
    {
        assert(i_Index >= 0 && i_Index < highest_referenced_index+1);
        array[i_Index] = array[max_index()];
        highest_referenced_index--;
    }
   
   //
   // Resize array, copying contents from old array before freeing it
   //

   void set_cnt (S32 new_size)
      {
      if (new_size == 0)
         {
         n_entries = 0;
         highest_referenced_index = -1;

         if (array != NULL)
            {
            delete [] array;
            array = NULL;
            }

         return;
         }

      T *new_array = new T[new_size];

      if (array != NULL)
         {
         S32 n = min(n_entries, new_size);
#if 1
         for (S32 i=0; i < n; i++)
            {
            new_array[i] = array[i];
            }
#else
         memcpy(new_array,    // faster, but doesn't support nonprimitives
                array,
                min(n_entries, new_size) * sizeof(T));
#endif
         delete [] array;
         array = NULL;
         }

      array = new_array;

      n_entries = new_size;

      if (highest_referenced_index >= n_entries)
         {
         //
         // Array shrunk, bring highest referenced entry back inside
         //

         highest_referenced_index = n_entries-1;
         }
      }

   //
   // Create resizable array
   //

   DynArray(void)
      {
      array = NULL;
      clear();
      }

   //
   // Create resizable array as a copy of another, such that the
   // source array can be destroyed without harming the copy
   //

   DynArray(DynArray<T, init_size> & src)
      {
      //
      // Initialize new array normally, then copy contents of source array
      //

      array                    = NULL;
      n_entries                = 0;
      highest_referenced_index = -1;

      S32 n   = src.capacity();
      S32 max = src.highest_referenced_index;

      set_cnt(n);

      for (S32 i=0; i < n; i++)
         {
         array[i] = src[i];
         }

      highest_referenced_index = max;
      src.highest_referenced_index = max;
      }

   //
   // Assign contents of one array to another
   //

   DynArray<T, init_size> & operator= (DynArray<T, init_size> &src)
      {
      if (&src != this)
         {
         //
         // Discard current contents of array, then copy contents of
         // source array
         //

         clear();

         S32 n   = src.capacity();
         S32 max = src.highest_referenced_index;

         set_cnt(n);

         for (S32 i=0; i < n; i++)
            {
            array[i] = src[i];
            }

         highest_referenced_index     = max;
         src.highest_referenced_index = max;
         }

      return *this;
      }

   //
   // Wipe out all allocated array entries
   //

   void wipe(void)
      {
      if (array != NULL)
         {
         delete [] array;
         array = NULL;
         }

      n_entries                = 0;
      highest_referenced_index = -1;
      }

   //
   // Destroy resizable array
   // 

   ~DynArray()
      {
      wipe();
      }

   //
   // Wipe current contents of array
   //

   void clear(void)
      {
      wipe();
      set_cnt(init_size);
      }

   //
   // Return largest integer ever used as an array index, or -1 if 
   // the array has never been indexed
   //
   // WARNING: this can yield misleading results if an array's contents
   // are written by reference (e.g., strcpy(&string[0],contents) after 
   // a manual set_cnt() operation).  Don't use it as a strlen() substitute!
   //

   S32 max_index(void)
      {
      return highest_referenced_index;
      }

   //
   // Return # of valid indexes (which, for a linear array, is just the
   // highest_referenced_index + 1)
   //

   S32 count(void)
      {
      return highest_referenced_index + 1;
      }

   //
   // Return total size of linear array, based on entries actually in use
   //
   // This is the # of bytes required to copy or stream the entire array
   //
   // WARNING: same problem as max_index() above
   //

   S32 indexed_size(void)
      {
      return (highest_referenced_index + 1) * sizeof(T);
      }

   //
   // Return index after previous highest-referenced index
   // Like methods above, don't mix this with set_cnt()
   //

   S32 alloc(void)
      {
      return highest_referenced_index + 1;
      }

   //
   // Return current capacity (# of entries available without causing a 
   // grow operation)
   //

   S32 capacity(void)
      {
      return n_entries;
      }

   //
   // Add new element to end of array
   //

   T & add(T &element)
      {
      (*this)[highest_referenced_index+1] = element;

      return element;
      }

   //
   // Dereference array member
   //
   // Keep track of highest index ever used to reference array, so we can
   // return the actual # of entries in use
   //
   // Select growth strategy with #if clause below
   //

   T & operator [] (S32 index)
      {
      assert(index >= 0);

      if (index > highest_referenced_index)
         {
         highest_referenced_index = index;

         if (index >= n_entries)
            {
            assert(init_size != 0);      // array must be grown manually elsewhere!

#if 1    // Array grows to 2X the required size

            set_cnt(index ? ((index + 1) * 2) : init_size);

#else    // Array grows in additive increments of init_size elements

		      set_cnt((((index + 1) / init_size) + 1) * init_size);
#endif
            }
         }

      return array[index];
      }
};

//****************************************************************************
//
// LIFO
//
// Template used to represent growable LIFO stack
//
// Objects are passed by reference and copied to stack
//
//****************************************************************************

template <typename T, U32 init_size=16> class LIFO
{
   DynArray <T,init_size> data;
   S32                    STK;

public:
   
   LIFO(void)
      {
      STK = -1;
      }

   void clear(void)              // Initialize stack, discarding existing contents
      {
      STK = -1;
      }

   void push(T &object)          // Push object onto stack
      {
      data[++STK] = object;
      }

   T &pop(void)                  // Remove topmost object from stack
      {
      assert(STK >= 0);

      return data[STK--];
      }

   S32 depth(void)               // Return # of objects on stack
      {
      return STK+1;
      }

   T &nth(S32 n)                 // Return nth entry from top of stack (0=top)
      {
      assert(STK >= n);

      return data[STK-n];
      }

   T &top(void)                  // Return most recently pushed object (top of stack)
      {
      return nth(0);
      }

   LIFO(LIFO<T, init_size> & src)
      {
      assert(0); // TBD
      }

   LIFO<T, init_size> & operator= (LIFO<T, init_size> &src)
      {
      assert(0); // TBD
      return *this;
      }
};

//****************************************************************************
//
// FastList
//
// Template used to represent doubly-linked list optimized for 
// high-performance allocation at the expense of C++ functionality
//
// Use FastList when:
//
//    - You want to maintain a linked list of arbitrary size
//
//    - You don't want to incur heap-function overhead when allocating
//      and deleting most entries
//
//    - You don't care about C++ construction, destruction, and copying 
//      functionality for the allocated entries
//
//    - Fast random-access searchability is not important
//
//    - You want linear, not circular, list traversal (traversal stops when
//      end of list reached)
//
//****************************************************************************

template <class T, S32 grow_size> class FastList
{
   struct LIST
      {
      T *first_entry;
      T *last_entry;
      };

   LIST A;
   LIST F;

   DynArray<T *> allocated_blocks;

   S32 cnt;
   S32 peak;

public:

   //
   // Constructor initializes empty list
   //

   FastList()
      {
      A.first_entry = NULL;
      A.last_entry  = NULL;
      F.first_entry = NULL;
      F.last_entry  = NULL;

      cnt  = 0;
      peak = 0;
      }

   ~FastList()
      {
      for (S32 i=0; i <= allocated_blocks.max_index(); i++)
         {
         delete [] allocated_blocks[i];
         }
      }

   //
   // Return first list entry (NULL if list empty)
   //

   T *first(void) const
      {
      return A.first_entry;
      }

   //
   // Return last list entry (NULL if list empty)
   //

   T *last(void) const
      {
      return A.last_entry;
      }

   //
   // Return next list entry (NULL if end of list reached)
   //
   // Return first list entry if current==NULL
   //

   T *next(T *current) const
      {
      if (current == NULL)
         {
         return first();
         }

      return current->next;
      }

   //
   // Return previous list entry (NULL if beginning of list reached)
   //
   // Return last list entry if current==NULL
   //

   T *prev(T *current) const
      {
      if (current == NULL)
         {
         return last();
         }

      return current->prev;
      }

   //
   // Link new item into list before specified entry
   // If specified entry==NULL, insert at end of list
   //

   T *link(LIST &list, T *entry, T *next = NULL)
      {
      T *prev;

      if (next == NULL)
         {
         prev = list.last_entry;
         list.last_entry = entry;
         }
      else
         {
         prev = next->prev;
         next->prev = entry;
         }

      if (prev == NULL)
         {
         list.first_entry = entry;
         }
      else
         {
         prev->next = entry;
         }

      entry->next = next;
      entry->prev = prev;

      return entry;
      }

   //
   // Unlink item from list (without destroying it)
   //

   void unlink(LIST &list, T *entry)
      {
      if (entry->prev == NULL)
         {
         list.first_entry = entry->next;
         }
      else
         {
         entry->prev->next = entry->next;
         }

      if (entry->next == NULL)
         {
         list.last_entry = entry->prev;
         }
      else
         {
         entry->next->prev = entry->prev;
         }
      }

   //
   // Allocate entry and insert before specified entry
   // If specified entry==NULL, insert at end of list
   //

   T *allocate(T *next = NULL)
      {
      if (F.first_entry == NULL)
         {
         //
         // No entries in free list -- allocate a new block and add it 
         // to the list of block pointers to be freed at destruction time
         //
         
         T *block = new T[grow_size];
         assert(block != NULL);

         allocated_blocks.add(block);

         T *ptr = block;

         for (S32 i=0; i < grow_size; i++)
            {
            link(F, ptr++);
            }
         }

      //
      // Remove first entry from "free" list and add it to the "allocated" 
      // list
      //

      if (++cnt > peak)
         {
         peak = cnt;
         }

      T *entry = F.first_entry;

      unlink(F, entry);

      return link(A, entry, next);
      }

   //
   // Unlink item from the "allocated" list and move it to the "free" list
   //

   void free(T *entry)
      {
      --cnt;

      unlink(A, entry);
      link(F, entry);
      }

   //
   // Unlink all list items
   //

   void free(void)
      {
      T *t;
      T *next;

      t = A.first_entry;

      while (t)
         {
         next = t->next;
         free(t);
         t = next;
         }

      assert(cnt==0);
      }

   //
   // Instrumentation hooks
   //

   S32 peak_allocation(void)
      {
      return peak;
      }

   S32 count(void)
      {
      return cnt;
      }

   //
   // Copy/assignment operators
   //

   FastList(FastList<T,grow_size> &src)
      {
      assert(0); // TBD
      }

   FastList<T,grow_size> & operator= (FastList<T,grow_size> &src)
      {
      assert(0); // TBD
      return *this;
      }

   //
   // Use of square brackets with a FastList simply dereferences the 
   // index as a T *
   // 
   // This improves code compatibility with descriptor-based templates 
   // like Pool<>
   //

   inline T &operator [] (S32 index)
      {
      return *((T *) index);
      }
};

//****************************************************************************
//
// HashList
//
// Template used to maintain a linked list of structures with a separate index
// for fast allocation and reference
//
// WARNING: Objects stored in a HashList will not have their destructors called
// until the HashList object itself goes out of scope.  Unlinking a HashList
// entry simply moves the entry to a free list.  Any contents, if any, must be
// explicitly cleared by the shutdown() method.
//
// Methods:
//   allocate()
//   unlink()
//   relabel()
//   search()
//   reset()
//   clear()
//   count()
//   cache()
//   display()
//   sort_simple_contents()
//   [] by index or string
//
// HashList makes use of the following members of class ENTRY_TYPE:
//
//   U32         hash_key;      // For internal use only
//   ENTRY_TYPE *hash_next;   
//   ENTRY_TYPE *hash_prev;  
//   ENTRY_TYPE *next;          
//   ENTRY_TYPE *prev;       
//                           
//   U32    hash        (void *)  // Derive key from object
//   S32    lex_compare (void *)  // Compare object with object stored in T, returning *lexical* result
//   void   initialize  (void *)  // Store indicated object in T
//   void   shutdown    (void)    // Called when unlinked from pool
//
// ENTRY_TYPE may be thought of as a "package" that allows an object of any
// desired type (string, structure, etc...) to be stored in a linked list
// which is indexed by a hash table.  Along with the mandatory data and code 
// members above, ENTRY_TYPE should also contain one or more user data fields 
// representing the object being stored in ENTRY_TYPE.
// 
// HashList works like HashPool except that it uses an LList instead of an
// expandable linear array to store its contents.  Use it when:
//
//    - You need to insert and remove entries in a list whose capacity needs 
//      to expand at runtime
//
//    - You don't care about heap overhead or cache locality, and/or you 
//      don't want a large initial footprint
//
//    - You need repeated allocations and deletions to be very fast, but don't 
//      mind if initial allocations go through new
//
//    - You want to be able to store pointers to entries, and you don't need
//      to store integer indexes
// 
//    - Fast random-access searchability is required
//
//    - Fast sequential iteration is required, with entries returned in order
//      of their original allocation
//
//    - You will never need to shrink an existing HashList's memory footprint
//
// Example usage of HashList to store a list of ASCII strings for fast 
// access:
//
//       class MESSAGE_TYPE
//       {
//       public:
//          //
//          // Data and methods required by HashList template
//          //
//       
//          U32           hash_key;  // Hash key for this entry
//       
//          MESSAGE_TYPE *hash_next; // Next/prev pointers in each hash bucket,
//          MESSAGE_TYPE *hash_prev; // organized for access speed
//       
//          MESSAGE_TYPE *next;      // Next/prev pointers in allocation list or  
//          MESSAGE_TYPE *prev;      // free list, depending on entry's status
//       
//          S32           allocated; // Nonzero if allocated, zero if in free list
//       
//          //
//          // HashList hash function to derive 8-bit key from string
//          // by XOR'ing the first two characters together
//          //
//       
//          static inline U32 hash(void *object)
//             {
//             assert(((U8 *) object)[0]);
//             return (U32) (((U8 *) object)[0] ^ ((U8 *) object)[1]);
//             }
//       
//          //
//          // HashList search comparison function -- returns TRUE if match
//          // found
//          //
//       
//          inline S32 lex_compare(void *object)
//             {
//             return strcmp(name, (C8 *) object);
//             }
//       
//          //
//          // HashList initialization function -- called when allocating new 
//          // entry
//          //
//       
//          inline void initialize(void *object)
//             {
//             strcpy(name, (C8 *) object);
//             }
//
//          //
//          // HashList shutdown function -- called when unlinking entry from
//          // pool
//          //
//
//          inline void shutdown(void)
//             {
//             }
//       
//          //
//          // User data
//          //
//       
//          C8 name[64];             // 64-character ASCII message name
//       };
// 
//****************************************************************************

template <class T, S32 hash_size=31> class HashList
{
   T *hash_table[hash_size];     // Hash bucket array for fast searches

public:
   
   LList<T> alloc_list;          // List of user-specified structures
   LList<T> free_list;

   S32 rover_index;              // (For fast sequential indexing)
   T  *rover_entry;

   HashList(void)
      {
      //
      // Initialize hash table
      //

      for (S32 i=0; i < hash_size; i++)
         {
         hash_table[i] = NULL;
         }

      rover_index = -1;
      rover_entry = NULL;
      }

   //
   // Destroy structure pool
   //                         

   void free (void);
   void clear (void);

   ~HashList(void)
      {
      free();
      }

   //
   // Sort contents lexically
   //

   static int sort_ascending(const void *a, const void *b)
      {
      return ((T *) a)->sort_compare(((T *) b));
      }

   static int sort_descending(const void *a, const void *b)
      {
      return ((T *) b)->sort_compare(((T *) a));
      }

   void sort_simple_contents(S32 polarity=1)     // 1=ascending, 0=descending
      {
      //
      // This (slow) function works by qsorting a temporary linear copy of alloc_list,
      // then relinking the sorted entries
      //
      // Supports only containers whose objects are relocatable in memory without
      // copy assignment (hence the function's name)
      //

      S32 n = alloc_list.count();

      if (n < 2)
         {
         return;
         }

      rover_index = -1;
      rover_entry = NULL;

      T  *list  = (T *)  malloc(n * sizeof(T  ));
      T **addrs = (T **) malloc(n * sizeof(T *));

      if ((list == NULL) || (addrs == NULL))
         {
         return;
         }

      T *LE = NULL;
      S32 i = 0;

      while ((LE = alloc_list.first()) != NULL)
         {
         list [i]   = *LE;
         addrs[i++] =  LE;    // we'll need to reuse the original objects' storage

         alloc_list.unlink(LE);
         remove_from_hash_table(LE);
         }

      qsort(list, 
            n, 
            sizeof(list[0]), 
            (polarity >= 0) ? sort_ascending : sort_descending);

      for (i=0; i < n; i++)
         {
         LE = addrs[i];       

         *LE = list[i];

         alloc_list.link(LE);
         add_to_hash_table(LE, LE->hash_key);
         }

      assert(alloc_list.count() == n);

      ::free(list);           // clean up the temporary contiguous arrays used by qsort()
      ::free(addrs); 
      }

   //
   // Add/remove hash table link
   //

   void add_to_hash_table(T *entry, U32 key)
      {
      //
      // Insert entry into hash bucket, from the beginning
      //
      // Associate hash key with entry for ease of reference
      //

      entry->hash_key  = key;
      entry->hash_next = hash_table[key];
      entry->hash_prev = NULL;

      if (hash_table[key] != NULL)
         {
         hash_table[key]->hash_prev = entry;
         }

      hash_table[key] = entry;
      }

   void remove_from_hash_table(T *entry)
      {
      if (entry->hash_next != NULL)
         {
         entry->hash_next->hash_prev = entry->hash_prev;
         }

      if (entry->hash_prev != NULL)
         {
         entry->hash_prev->hash_next = entry->hash_next;
         }
      else
         {
         hash_table[entry->hash_key] = (T *) entry->hash_next;
         }
      }

   //
   // Allocate pool entry labelled by *object, growing list if necessary
   // to accomodate new allocation
   //                                                               

   T *allocate(const void *object)
      {
      rover_index = -1;
      rover_entry = NULL;

      U32 key = T::hash(object) % hash_size;

      //
      // Get pointer to free entry and move it into the 'allocated' list
      //

      T *entry = free_list.last();     // (more likely to be in cache than first entry)

      if (entry == NULL)    
         {
         entry = alloc_list.alloc();   // no free list entries available, must allocate new one from heap

         if (entry == NULL)
            {
            return NULL;
            }
         }
      else
         {
         free_list.unlink(entry);
         alloc_list.link(entry);
         }

      add_to_hash_table(entry,key);

      //
      // Initialize newly-allocated entry with object, and return pointer
      //

      entry->allocated = TRUE;
      entry->initialize(object);

      return entry;
      }

   //
   // Unlink existing entry from hash list and allocation list
   //

   void unlink(T *entry)
      {
      if ((entry == NULL) || (!entry->allocated))
         {
         return;
         }

      rover_index = -1;
      rover_entry = NULL;

      //
      // Call deallocation routine before unlinking object
      //

      entry->shutdown();
      entry->allocated = FALSE;

      //
      // Unlink from hash table
      //

      remove_from_hash_table(entry);

      //
      // Move from allocated list to free list
      //

      alloc_list.unlink(entry);
      free_list.link(entry);
      }

   //
   // Unlink existing entry by search key, if present
   //

   void unlink(void *search_string)
      {
      unlink(search(search_string));
      }

   //
   // Relabel an existing entry, moving it to a different hash bucket without destroying it
   //
   // Caller is responsible for actually modifying the object's name string or other tag!
   //

   void relabel(T *entry, const void *new_label)
      {
      if ((entry == NULL) || (new_label == NULL))
         {
         return;
         }

      U32 new_key = T::hash(new_label) % hash_size;

      remove_from_hash_table(entry);
      add_to_hash_table(entry, new_key);
      }

   //
   // Search pool for entry which represents *object
   //
   // Return entry pointer, or NULL if object not found
   //

   T *search(const void *object)
      {
      T *result;

      result = hash_table[T::hash(object) % hash_size];

      while (result != NULL)
         {
         if (!result->lex_compare(object))
            {
            return result;
            }

         result = (T *) result->hash_next;
         }

      return NULL;
      }

   //
   // Return current # of entries actually in use
   //

   S32 count(void)
      {
      return alloc_list.count();
      }

   //
   // Similar to search(), but automatically allocates an entry on failure
   //

   T *cache(const void *object)
      {
      T *result = search(object);

      if (result == NULL)
         {
         result = allocate(object);
         }

      return result;
      }

   //
   // Perform diagnostic dump of all objects in list
   //

   void display(void)
      {
      S32 n = count();

      for (S32 i=0; i < n; i++)
         {
         (*this)[i].display();
         }
      }

   //
   // Index-by-integer operator
   //
   // This is O(1) if you're walking the entire list in order, and not
   // adding or deleting any entries... otherwise, it's O(n) due to
   // the need to find the nth list entry
   //

   T &operator [] (S32 index)
      {
      if (index == rover_index)        // (i.e., if we've advanced to the next entry in the LList
         {                             // immediately after the last one we indexed)
         T *result = rover_entry;

         if (rover_entry == NULL) 
            rover_index = -1;
         else
            {
            rover_index++;
            rover_entry = (T *) rover_entry->next;
            }

         return *result;
         }

      S32 i = 0;                       // find nth entry the slow way...
      T *LE = alloc_list.first();

      while (LE != NULL)
         {
         if (i == index)
            {
            break;
            }

         LE = (T *) LE->next;
         i++;
         }

      if (LE == NULL)                  // tried to index past end of list?
         {
         rover_entry = NULL;
         rover_index = -1;

         return *alloc_list.first();   // gotta return a valid reference, might as well be the first element
         }

      rover_entry = (T *) LE->next;    // otherwise, log address of next LList member (if there is one)
      rover_index = i+1;               // and keep track of its index in case we ask for it next

      return *LE;
      }

   //
   // These index operators return pointers, not references, so that they
   // can fail by returning NULL
   //

   T *operator [] (void *search_string)
      {
      return search(search_string);
      }

   T *operator [] (const void *search_string)
      {
      return search((void *) search_string);
      }

   //
   // Create pool as a copy of another, such that the
   // source pool can be destroyed without harming the copy
   //

   HashList(HashList<T, hash_size> & src)
      {
      assert(0); // TODO
      }

   //
   // Assign contents of one array to another
   //

   HashList<T, hash_size> & operator= (HashList<T, hash_size> &src)
      {
      if (&src != this)
         {
         assert(0); // TODO
         }

      return *this;
      }
};

template <class T, S32 hash_size> void HashList<T, hash_size>::free (void)
{
   //
   // Shut down all entries and move them to the free list
   //

   clear();

   //
   // Destroy and delete all entries in the free list
   //

   for (;;) 
      {
      T *obj = free_list.last();

      if (obj == NULL)
         {
         break;
         }

      free_list.free(obj);
      }
}

template <class T, S32 hash_size> void HashList<T, hash_size>::clear (void)
{
   //
   // Unlink and shut down all objects in list, in
   // their reverse order of allocation
   //

   rover_index = -1;
   rover_entry = NULL;

   for (;;)
      {
      T *obj = alloc_list.last();

      if (obj == NULL)
         {
         break;
         }

      unlink(obj);
      }
}

#endif
