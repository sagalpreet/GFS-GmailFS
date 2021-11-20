#define FUSE_USE_VERSION 30

#include <fuse.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <curl/curl.h>

#include "helper.h"

#define EMAIL_ (((struct data*) fuse_get_context()->private_data) -> username)
#define PWD_ (((struct data*) fuse_get_context()->private_data) -> password)
#define ROOT (((struct data*) fuse_get_context()->private_data) -> root)
#define upload_text (((struct data*) fuse_get_context()->private_data) -> uploadText)
#define MOUNT "CS303"

char *file_text = "Subject: "; // append message in the form "Subject: <subject here>\r\r\n<content here>"

#define LOG 1

struct data
{
	char *username, *password, *root, *uploadText;
};

int gfs_getattr (const char *path, struct stat *stbuf, struct fuse_file_info *fi)
{
    /** Get file attributes.
	 *
	 * Similar to stat().  The 'st_dev' and 'st_blksize' fields are
	 * ignored.	 The 'st_ino' field is ignored except if the 'use_ino'
	 * mount option is given.
	 */

	printf("getattr: %s\n", path);

	if (strcmp(path, "/.svn") == 0)
	{
		printf("~~~ google's garbage is not accepted\n");
		return -ENOENT;
	}
	if (strcmp(path, "/.git") == 0)
	{
		printf("~~~ google's garbage is not accepted\n");
		return -ENOENT;
	}
	if (strcmp(path, "/HEAD") == 0)
	{
		printf("~~~ google's garbage is not accepted\n");
		return -ENOENT;
	}
	if (strcmp(path, "/.hg") == 0)
	{
		printf("~~~ google's garbage is not accepted\n");
		return -ENOENT;
	}

	memset(stbuf, 0, sizeof(struct stat));

	int is_dir = isDirectory(path);
	if (is_dir)
	{
		if (LOG) printf("~~~ this is a directory\n");
		stbuf->st_mode = S_IFDIR | 0777;
        char **subDirs = getAllFolders(path);

		if (LOG) printf("~~~ subdirectories fetched\n");

		stbuf->st_nlink = 2;
        
        int i = 0;
        while (*(subDirs+i))
        {
            (stbuf->st_nlink)++;
			i++;
        }
		if (LOG) printf("~~~ %d hard links\n", stbuf->st_nlink);
        return 0;
	}

	
	int is_file = isFile(path);
    if (is_file)
	{
		printf("~~~ this is a file\n");
		stbuf->st_mode = S_IFREG | 0777;
		stbuf->st_nlink = 1;
		char *filename, *content;
		getFileDetails(path, &filename, &content);
		stbuf->st_size = strlen(content);

		if (LOG)
		{
			printf("~~~ size: %d\n", stbuf->st_size);
			printf("~~~ filename: %s\n", filename);
		}

		free(content);
		free(filename);
        return 0;
	}
	
	printf("~~~ no such file/directory\n");
    return -ENOENT;
}

int gfs_readdir (const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
    /** Read directory
	 *
	 * This supersedes the old getdir() interface.  New applications
	 * should use this.
	 *
	 * The filesystem may choose between two modes of operation:
	 *
	 * 1) The readdir implementation ignores the offset parameter, and
	 * passes zero to the filler function's offset.  The filler
	 * function will not return '1' (unless an error happens), so the
	 * whole directory is read in a single readdir operation.  This
	 * works just like the old getdir() method.
	 *
	 * 2) The readdir implementation keeps track of the offsets of the
	 * directory entries.  It uses the offset parameter and always
	 * passes non-zero offset to the filler function.  When the buffer
	 * is full (or an error happens) the filler function will return
	 * '1'.
	 *
	 * Introduced in version 2.3
	 */

	printf("readdir: %s\n", path);

	filler(buf, ".", NULL, 0, 0); // may choose to remove this later
	filler(buf, "..", NULL, 0, 0); // may choose to remove this later

	// accessing folders and freeing side by side
	char **folders = getAllFolders(path);
	printf("~~~ fetched all subdirectories\n");

	// accessing files and freeing side by side
	int i = 0;
	while (*(folders+i))
	{
		filler(buf, *(folders+i), NULL, 0, 0);
		free(*(folders+i));
		i++;
	}
	free(folders);

	char **files = getAllFiles(path);
	printf("~~~ fetched all files\n");

	// accessing and freeing side by side
	i = 0;
	while (*(files+i))
	{
		filler(buf, *(files+i), NULL, 0, 0);
		free(*(files+i));
		i++;
	}
	free(files);

    return 0;
}

int gfs_rmdir (const char *path)
{
    /** Remove a directory */
	if (LOG) printf("rmdir: path\n");

	int isDir = isDirectory(path);
	if (!isDir)
	{
		printf("error: directory doesn't exist\n");
		return -ENOENT;
	}

	printf("~~~ seems like a valid path\n");
	return deleteDir(path);
}

int gfs_mkdir (const char *path, mode_t mode)
{
    /** Create a directory 
	 *
	 * Note that the mode argument may not have the type specification
	 * bits set, i.e. S_ISDIR(mode) can be false.  To obtain the
	 * correct directory type bits use  mode|S_IFDIR
	 * */

	// make a check that this directory/file doesn't already exist
    if (isFile(path) || isDirectory(path))
	{
		printf("error: directory/file at this path already exists\n");
		return -EEXIST;
	}

	char *filename, *parent_dir;
	int isValid = extractFileDetails(path, &filename, &parent_dir); // extract the name of directory and its parent directory

	if (LOG)
	{
		printf("~~~ parent directory: %s\n", parent_dir);
		printf("~~~ filename: %s\n", filename);
	}

	/** checks
	 * directory name is valid
	 * parent directory exists
	 * */
	if ((!isValid) || strlen(filename) < 2 || (!isDirectory(parent_dir)))
	{
		// freeing

		if (LOG) printf("~~~ invalid path: check filename and path\n");

		free(filename);
		free(parent_dir);
		printf("error: invalid file name\n");
		return -ENOENT;
	}

	/** attempt creating directory: error is returned if 
	 * path is incorrect (should happen already in the 
	 * previous check, just for the sake of enforcing security)
	 * */
	int res = createDirectory(path);

	if (LOG) printf("~~~ operation completed\n");

	if (res != 0) return -ECONNABORTED;

    return 0; // if no error
}

int gfs_create (const char *path, mode_t mode, struct fuse_file_info *fi)
{
    /**
	 * Create and open a file
	 *
	 * If the file does not exist, first create it with the specified
	 * mode, and then open it.
	 *
	 * If this method is not implemented or under Linux kernel
	 * versions earlier than 2.6.15, the mknod() and open() methods
	 * will be called instead.
	 *
	 * Introduced in version 2.5
	 */

	printf("create: path\n");

	if (isFile(path) || isDirectory(path))
	{
		printf("error: file/directory at this path already exists\n");
		return -EEXIST;
	}

	char *filename, *parent_dir;
	int isValid = extractFileDetails(path, &filename, &parent_dir);
	printf("~~~ extracted parent_dir and filename\n");

	if ((!isValid) || strlen(filename) < 2 || (!isDirectory(parent_dir)))
	{
		if (LOG) printf("~~~not a valid path: check filename and path\n");
		// freeing
		free(filename);
		free(parent_dir);
		printf("error: invalid file name\n");
		return -ENOENT;
	}
		
	/* creating exact path inside gmail root label */
	char gpath[strlen(ROOT) + strlen(parent_dir) + 1];
	strcpy(gpath, ROOT);
	gpath[strlen(ROOT)] = 0;
	strcat(gpath, parent_dir);
	gpath[strlen(ROOT) + strlen(parent_dir)] = 0;

	if (LOG) printf("~~~ the gmail path is %s\n", gpath);

	/* Creating an empty file with specified filename at specified location */
	char descriptor[strlen(file_text) + strlen(filename) + 3 + 1];
	strcpy(descriptor, file_text);
	descriptor[strlen(file_text)] = 0;
	strcat(descriptor, filename);
	descriptor[strlen(file_text) + strlen(filename)] = 0;
	strcat(descriptor, "\r\r\n\0");

	if (LOG)
	{
		printf("~~~ the initial commit to the mail is %s\n", descriptor);
		printf("~~~\n");
	}

	int res = createFile(gpath, descriptor);

	if (LOG) printf("~~~ operation completed\n");

	if (res != 0) return -ECONNABORTED;

    return 0;
}

int gfs_write (const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    /** Write data to an open file
	 *
	 * Write should return exactly the number of bytes requested
	 * except on error.	 An exception to this is when the 'direct_io'
	 * mount option is specified (see read operation).
	 *
	 * Changed in version 2.2
	 */

	printf("write: %s\n");

	if (!isFile(path))
	{
		printf("error: file doesn't exist\n");
		return -ENOENT;
	}

	// get file details (name and content)
	char *filename, *content, *parent_dir;
	getFileDetails(path, &filename, &content);

	if (LOG) printf("~~~ filename, content and parent_dir extracted\n");

	extractFileDetails(path, &filename, &parent_dir);

	size_t len = strlen(content);

	if (LOG) printf("~~~ length: %d offset: %d size: %d\n", len, offset, size);
	if (LOG) printf("~~~ buffer: %s", buf);
	if (LOG) printf("~~~\n");

	if (offset <= len) {
		if (offset + size > len)
			content = (char *) realloc(content, size + offset);
		memcpy(content + offset, buf, size);
	} else
		size = 0;

	if (size == 0)
	{
		return 0;
	}

	if (LOG) printf("~~~ deleting previous file\n");
	deleteFile(path);

	/* creating exact path inside gmail root label */
	char gpath[strlen(ROOT) + strlen(parent_dir) + 1];
	strcpy(gpath, ROOT);
	gpath[strlen(ROOT)] = 0;
	strcat(gpath, parent_dir);
	gpath[strlen(ROOT) + strlen(parent_dir)] = 0;

	if (LOG) printf("~~~ path: %s\n", gpath);

	/* Creating an empty file with specified filename at specified location */
	char descriptor[strlen(file_text) + strlen(filename) + 3 + 1 + strlen(content)];
	strcpy(descriptor, file_text);
	descriptor[strlen(file_text)] = 0;
	strcat(descriptor, filename);
	descriptor[strlen(file_text) + strlen(filename)] = 0;
	strcat(descriptor, "\r\r\n\0");
	strcat(descriptor, content);
	descriptor[strlen(file_text) + strlen(filename) + 3 + strlen(content)] = 0;

	if (LOG)
	{
		printf("~~~ new content\n");
		printf("%s", descriptor);
		printf("~~~\n");
	}

	int res = createFile(gpath, descriptor);

	if (LOG) printf("~~~ operation completed\n");
	// if (res != 0) return ECONNABORTED; no use of detecting error here <=> equivalent to power failure (internet failure in this case)

    return size;
}

int gfs_read (const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    /** Read data from an open file
	 *
	 * Read should return exactly the number of bytes requested except
	 * on EOF or error, otherwise the rest of the data will be
	 * substituted with zeroes.	 An exception to this is when the
	 * 'direct_io' mount option is specified, in which case the return
	 * value of the read system call will reflect the return value of
	 * this operation.
	 *
	 * Changed in version 2.2
	 */

	printf("readdir: %s\n", path);

	size_t len;
	if(!isFile(path))
		return -ENOENT;

	char *filename, *content;
	getFileDetails(path, &filename, &content);

	if (LOG) printf("~~~ file specs extracted\n");

	len = strlen(content);

	if (offset <= len) {
		if (offset + size > len)
			size = len - offset;
		memcpy(buf, content + offset, size);
	} else
		size = 0;

	printf("~~~ bytes read: %d\n", size);
	return size;
}

int gfs_unlink (const char *path)
{
    /** Remove a file */
	
	if (!isFile(path)) // check if file exists
	{
		printf("error: file does not exist\n");
		return -ENOENT;
	}

	// get file details
    char *filename, *parent_dir;
    extractFileDetails(path, &filename, &parent_dir);

	if (filename[0] >= '0' && filename[0] <= '9')
	{
		printf("error: can't remove this file (numeric)\n");
		return -EACCES;
	}

	return deleteFile(path); // call utility function
}

static const struct fuse_operations gfs_oper = {
    .getattr = gfs_getattr, // works fine: checked for folders atleast
    .create = gfs_create, // works fine: a redundant warning is of no use
    .mkdir = gfs_mkdir, // works fine
    .unlink = gfs_unlink, // works fine
    .rmdir = gfs_rmdir, // works fine
    .read = gfs_read, // works fine
    .write = gfs_write, // works fine
    .readdir = gfs_readdir, // works fine
};

int main(int argc, char *argv[])
{
	// validate the command line arguments
	if (argc < 3)
	{
		printf("Atleast 2 command line arguments should be given with last being the config file\n");
		exit(-1);
	}

	// allocate dynamic memory for reading configuration file
	char *host_name, *port_number, *username, *password;
	host_name = malloc(100);
    port_number = malloc(100);
    username = malloc(100);
    password = malloc(100);

	// read configuration file and close it
	FILE* f = fopen(argv[argc-1], "r");
	fscanf(f, "%s", host_name);
	fscanf(f, "%s", port_number);
	fscanf(f, "%s", username);
	fscanf(f, "%s", password);
	fclose(f);

	// construct host_name string
	strcat(host_name, ":\0");
	strcat(host_name, port_number);
	free(port_number);
	strcat(host_name, "/\0");
	strcat(host_name, MOUNT);
	
	argc--;

	char* args[argc];
	args[0][0] = 0;
	for (int i = 1; i < argc; i++)
    {
        args[i] = (char*)malloc(100);
		strcpy(args[i], argv[i]);
        if (LOG)
        {
            printf("args: %s\n", args[i]);
        }
    }

	int ret;

	struct data *gfs_data = malloc(sizeof(struct data)); // private data to store

	// setting value for private data
	gfs_data -> username = username;
	gfs_data -> password = password;
	gfs_data -> root = host_name;
	gfs_data -> uploadText = 0;

	// calling fuse_main
    ret = fuse_main(argc, args, &gfs_oper, gfs_data);

    return ret;
}
