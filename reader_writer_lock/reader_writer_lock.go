package main

import "sync"

type RWLockInterface interface {
	Lock()
	RLock()
	RUnlock()
	Unlock()
}

// ReaderWriterLock provides the type of data protection that
// allows multi-thread read access, and single-thread write access
type ReaderWriterLock struct {

	// counter tracks the number of threads that has access to the protected data.
	// The values of counter, and the scenarios represents are:
	//    counter = -1; The ReaderWriterLock is acquired by 1 writer thread.
	//    counter = 0;  The ReaderWriterLock is acquired by 0 writer thread, and 0 reader thread.
	//    counter > 0;  The ReaderWriterLock is acquired by N reader thread.
	counter int64

	// The lock on the counter
	counter_mutex *sync.Mutex

	// condition_variable for the readers
	reader_cv *sync.Cond

	// condition_variable for the writer
	writer_cv *sync.Cond
}

func NewReaderWriterLock() *ReaderWriterLock {
	var counter_mutex sync.Mutex
	r_cv := sync.NewCond(&counter_mutex)
	w_cv := sync.NewCond(&counter_mutex)

	return &ReaderWriterLock{
		counter:       0,
		counter_mutex: &counter_mutex,
		reader_cv:     r_cv,
		writer_cv:     w_cv,
	}
}

// Acquire the read lock of the protected data
func (rwl *ReaderWriterLock) RLock() {
	rwl.counter_mutex.Lock()
	defer rwl.counter_mutex.Unlock()

	// Wait if one thread is writing to the protected data
	for rwl.counter == -1 {
		rwl.reader_cv.Wait()
	}
	rwl.counter++
}

// Release the read lock of the protected data
func (rwl *ReaderWriterLock) RUnlock() {
	rwl.counter_mutex.Lock()
	defer rwl.counter_mutex.Unlock()

	// Once this read is done, reduce the counter by 1
	// and if no readers anymore, notify any writer in waiting
	rwl.counter--
	if rwl.counter == 0 {
		rwl.writer_cv.Signal()
	}
}

// Acquire the writer lock of the protected data
func (rwl *ReaderWriterLock) Lock() {
	rwl.counter_mutex.Lock()
	defer rwl.counter_mutex.Unlock()

	// Wait as long as one reader is reading the data
	for rwl.counter > 0 {
		rwl.writer_cv.Wait()
	}
	rwl.counter = -1
}

// Release the writer lock of the protected data
func (rwl *ReaderWriterLock) Unlock() {
	rwl.counter_mutex.Lock()
	defer rwl.counter_mutex.Unlock()

	// One this write operation is done, notify
	// all readers, and one writer in wait.
	rwl.counter = 0

	// Give readers a slightly higher priority
	rwl.reader_cv.Broadcast()
	rwl.writer_cv.Signal()
}
