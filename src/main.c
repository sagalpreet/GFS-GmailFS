#define FUSE_USE_VERSION 31

#define _GNU_SOURCE

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

const int LOG = 1;

#define EMAIL_ (((struct data*) fuse_get_context()->private_data) -> username)
#define PWD_ (((struct data*) fuse_get_context()->private_data) -> password)
#define ROOT (((struct data*) fuse_get_context()->private_data) -> root)
#define upload_text (((struct data*) fuse_get_context()->private_data) -> uploadText)
#define MOUNT "CS303"

char *file_text = "Subject: "; // append message in the form "Subject: <subject here>\r\r\n<content here>"

void *gfs_init (struct fuse_conn_info *conn, struct fuse_config *config)
{
    /**
	 * Initialize filesystem
	 *
	 * The return value will passed in the private_data field of
	 * fuse_context to all file operations and as a parameter to the
	 * destroy() method.
	 *
	 * Introduced in version 2.3
	 * Changed in version 2.6
	 */
	
	gfs_mkdir(ROOT ,0);

	struct data* gfs_data = (struct data*) fuse_get_context()->private_data; // user data

    return gfs_data	; // returned
}

int gfs_getattr (const char *path, struct stat *stbuf, struct fuse_file_info *fi)
{
    /** Get file attributes.
	 *
	 * Similar to stat().  The 'st_dev' and 'st_blksize' fields are
	 * ignored.	 The 'st_ino' field is ignored except if the 'use_ino'
	 * mount option is given.
	 */
	memset(stbuf, 0, sizeof(struct stat));

	int is_dir = isDirectory(path), is_file = isFile(path);
	if (is_dir)
	{
		stbuf->st_mode = S_IFDIR | 0777;
		stbuf->st_nlink = 2;
	}
	else if (is_file)
	{
		stbuf->st_mode = S_IFREG | 0777;
		stbuf->st_nlink = 1;
		char *filename, *content;
		getFileDetails(path, filename, content);
		stbuf->st_size = strlen(content);
		free(content);
		free(filename);
	}
	else
	{
		return -ENOENT;
	}

    return 0;
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

	if (isFile(path) || isDirectory(path))
	{
		printf("error: file already exists\n");
		return -EEXIST;
	}

	char *filename, *parent_dir;
	int isValid = extractFileDetails(path, &filename, &parent_dir);
	if ((!isValid) || strlen(filename) < 2 || (!isDirectory(parent_dir)))
	{
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

	/* Creating an empty file with specified filename at specified location */
	char descriptor[strlen(file_text) + strlen(filename) + 3 + 1];
	strcpy(descriptor, file_text);
	descriptor[strlen(file_text)] = 0;
	strcat(descriptor, filename);
	descriptor[strlen(file_text) + strlen(filename)] = 0;
	strcat(descriptor, "\r\r\n\0");

	int res = createFile(gpath, descriptor);
	if (res != 0) return -ECONNABORTED;

    return 0;
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
		printf("error: directory already exists\n");
		return -EEXIST;
	}

	char *filename, *parent_dir;
	int isValid = extractFileDetails(path, &filename, &parent_dir); // extract the name of directory and its parent directory

	/** checks
	 * directory name is valid
	 * parent directory exists
	 * */
	if ((!isValid) || strlen(filename) < 2 || (!isDirectory(parent_dir)))
	{
		// freeing
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
	if (res != 0) return -ECONNABORTED;

    return 0; // if no error
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

int gfs_rmdir (const char *path)
{
    /** Remove a directory */
	if (!isDirectory(path))
	{
		printf("error: directory doesn't exist\n");
		return -ENOENT;
	}
	return deleteDir(path);
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

	size_t len;
	if(!isFile(path))
		return -ENOENT;

	char *filename, *content;
	getFileDetails(&filename, &content);

	len = strlen(content);

	if (offset < len) {
		if (offset + size > len)
			size = len - offset;
		memcpy(buf, content + offset, size);
	} else
		size = 0;

	return size;
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

	if (!isFile(path))
	{
		printf("error: file doesn't exist\n");
		return -ENOENT;
	}

	// get file details (name and content)
	char *filename, *content, *parent_dir;
	getFileDetails(path, &filename, &content);
	extractFileDetails(path, &filename, &parent_dir);

	len = strlen(content);

	if (offset < len) {
		if (offset + size > len)
			content = (char *) realloc(content, size + offset);
		memcpy(content + offset, buf, size);
	} else
		size = 0;

	if (size == 0)
	{
		return 0;
	}

	deleteFile(path);

	/* creating exact path inside gmail root label */
	char gpath[strlen(ROOT) + strlen(parent_dir) + 1];
	strcpy(gpath, ROOT);
	gpath[strlen(ROOT)] = 0;
	strcat(gpath, parent_dir);
	gpath[strlen(ROOT) + strlen(parent_dir)] = 0;

	/* Creating an empty file with specified filename at specified location */
	char descriptor[strlen(file_text) + strlen(filename) + 3 + 1 + strlen(content)];
	strcpy(descriptor, file_text);
	descriptor[strlen(file_text)] = 0;
	strcat(descriptor, filename);
	descriptor[strlen(file_text) + strlen(filename)] = 0;
	strcat(descriptor, "\r\r\n\0");
	strcat(descriptor, content);
	descriptor[strlen(file_text) + strlen(filename) + 3 + strlen(content)] = 0;

	int res = createFile(gpath, descriptor);
	// if (res != 0) return ECONNABORTED; no use of detecting error here <=> equivalent to power failure (internet failure in this case)

    return size;
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

	filler(buf, ".", NULL, 0); // may choose to remove this later
	filler(buf, "..", NULL, 0); // may choose to remove this later

	char **files = getAllFiles(path);

	// accessing and freeing side by side
	int i = 0;
	while (*(files+i))
	{
		filler(buf, files+i, NULL, 0);
		free(files+i);
		i++;
	}
	free(files);

    return 0;
}

int gfs_rename (const char *from, const char *to)
{
    /** Rename a file */
	if (strcmp(from, to) == 0) return 0;

	if (!isFile(from))
	{
		printf("error: source file doesn't exist\n");
		return -ENOENT;
	}

	int res = gfs_create(to, 0, 0);
	
	if (res != 0)
	{
		printf("error: destination doesn't exist\n");
		return -ENOENT;
	}

	// get file details
	char *filename, *parent_dir, *content;
	getFileDetails(from, &filename, &content);
	extractFileDetails(from, &filename, &parent_dir);

	gfs_write (to, content, strlen(content), 0, 0);
	gfs_unlink (from);

    return 0;
}

static const struct fuse_operations gfs_oper = {
    .init = gfs_init,
    .getattr = gfs_getattr,
    // .readlink = gfs_readlink,
    .create = gfs_create,
    .mkdir = gfs_mkdir,
    .unlink = gfs_unlink,
    .rmdir = gfs_rmdir,
    // .chmod = gfs_chmod,
    .read = gfs_read,
    .write = gfs_write,
    // .opendir = gfs_opendir,
    .readdir = gfs_readdir,
};

struct data
{
	char *username,
	char *password,
	char *root,
	char *uploadText
};

int main(int argc, char *argv[])
{
	// validate the command line arguments
	if (argc != 3)
	{
		printf("Exactly 2 command line arguments should be given\n");
		exit(-1);
	}

	// allocate dynamic memory for reading configuration file
	char *host_name, *port_number, *username, *password;
	host_name = malloc(100);
    port_number = malloc(100);
    username = malloc(100);
    password = malloc(100);

	// read configuration file and close it
	FILE* f = fopen(argv[2]);
	fscanf(f, "%s", root);
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

	// give up free space by reducing the space allocated
	host_name = realloc(host_name, 1 + strlen(host_name));
    username = realloc(host_name, 1 + strlen(username));
    password = realloc(host_name, 1 + strlen(password));
	
	argc--;
	argv[2] = 0;

	int ret;
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv); // set fuse arguments

	struct data *gfs_data = malloc(sizeof(struct data)); // private data to store

	// setting value for private data
	gfs_data -> username = username;
	gfs_data -> password = password;
	gfs_data -> root = host_name;
	gfs_data -> uploadText = 0;

	// calling fuse_main
    ret = fuse_main(args.argc, args.argv, &gfs_oper, gfs_data);
    fuse_opt_free_args(&args);

    return ret;
}

// NOTE: Error codes are negative