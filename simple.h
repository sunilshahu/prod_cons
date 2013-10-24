#ifndef SCULL_QUANTUM
#define SCULL_QUANTUM 1024  /* one quantum 1 kb of size */
#endif

#ifndef SCULL_QSET
#define SCULL_QSET    1024 /* one quantum set is 1024kb = 1 mb of size*/
#endif

/*size of max allowed data  = 32 MB */
#ifndef SCULL_MAX_DATA
#define SCULL_MAX_DATA    (SCULL_QUANTUM * SCULL_QSET * 32)
#endif

#define DEVCREATEERR	1001
#define DEVADDERR	1002
struct scull_qset {
	void **data;
	struct scull_qset *next;
};


struct scull_dev {
	struct scull_qset *data;  /* Pointer to first quantum set */
	int quantum;              /* the current quantum size */
	int qset;                 /* the current array size */
	unsigned long size;       /* amount of data stored here */
	unsigned int access_key;  /* used by sculluid and scullpriv */
};
