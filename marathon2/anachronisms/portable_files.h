/*

	portable_files.h
	Saturday, August 26, 1995 1:12:37 PM- rdm created. (for the sake of portability)

	Needless to say, these can be changed to suit your particular OS
*/

#ifndef __FILE_DESCRIPTIONS__
#define __FILE_DESCRIPTIONS__
/* ------------- file descriptions */
typedef struct {
	short vRefNum;
	long parID;
	unsigned char name[64];
} FileDesc; /* FileDesc==FileDescriptor */

/* ------------- data types */
typedef short FileError; /* same as OSErr */
typedef short fileref; /* File descriptor, for portability */

/* ------------- file error codes! */
enum {
	errFileNotFound= -43
};

/* ------------- protos */

FileError create_file(FileDesc *file, unsigned long file_type); // creator for mac, extension for dos

/* NONE is considered failure! */
fileref open_file_for_reading(FileDesc *file);
fileref open_file_for_writing(FileDesc *file);

void close_file(fileref refnum);

unsigned long get_fpos(fileref refnum);
FileError set_fpos(fileref refnum, unsigned long offset);

unsigned long get_file_length(fileref refnum);

FileError read_file(fileref refnum, unsigned long count, void *buffer);
FileError write_file(fileref refnum, unsigned long count, void *buffer);

FileError delete_file(FileDesc *file);

FileError find_preferences_location(FileDesc *file);

/* ------ miscellaneous routines */
FileError add_application_name_to_fsspec(FileDesc *file, char *pascal_name);
void get_application_filedesc(FileDesc *file);
FileError memory_error(void);
#endif